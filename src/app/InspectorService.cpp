#include "InspectorService.hpp"
#include "../hid/ReportDecoder.hpp"
#include "../ui/InventoryText.hpp"
#include "../util/Text.hpp"
#include "../diagnostics/CaptureAnalysis.hpp"
#include "../input/PhysicalControls.hpp"
#include <shlobj.h>
#include <memory>
#include <set>
#include <future>
#include <cwctype>

namespace x52 {
namespace {
constexpr DWORD ReaderWaitMs = 20;
constexpr double RediscoverMs = 1000;
constexpr std::size_t PreRollReports = 200;
constexpr std::size_t CaptureLimitBytes = 64 * 1024 * 1024;
HANDLE OpenInput(const std::wstring& path)
{
    const auto handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (handle == INVALID_HANDLE_VALUE) throw WindowsException("CreateFileW (PS28 input)", GetLastError());
    return handle;
}
class ReadSession final {
public:
    explicit ReadSession(const HidDeviceInfo& info)
        : handle(OpenInput(info.path)),
          event(CreateEventW(nullptr, TRUE, FALSE, nullptr)), path(info.path)
    {
        if (!event.valid()) throw WindowsException("CreateEventW", GetLastError());
        decoder = std::make_unique<ReportDecoder>(handle.get());
        buffer.resize(decoder->caps().InputReportByteLength);
        if (buffer.empty()) throw std::runtime_error("Selected collection has no input report");
        operation.hEvent = event.get();
    }
    ~ReadSession()
    {
        if (pending) {
            // Cancellation requests are asynchronous. Drain before freeing OVERLAPPED/buffer/handle.
            CancelIoEx(handle.get(), &operation);
            DWORD transferred{};
            GetOverlappedResult(handle.get(), &operation, &transferred, TRUE);
        }
    }
    ReadSession(const ReadSession&) = delete;
    ReadSession& operator=(const ReadSession&) = delete;
    // nullopt = still waiting; 0 = successful zero-byte report (decoder rejects it).
    std::optional<DWORD> Read()
    {
        if (!pending) {
            if (!ResetEvent(event.get())) throw WindowsException("ResetEvent", GetLastError());
            DWORD transferred{};
            if (ReadFile(handle.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &transferred, &operation)) return transferred;
            const auto error = GetLastError();
            if (error != ERROR_IO_PENDING) throw WindowsException("ReadFile", error);
            pending = true;
        }
        const auto wait = WaitForSingleObject(event.get(), ReaderWaitMs);
        if (wait == WAIT_TIMEOUT) return std::nullopt;
        if (wait != WAIT_OBJECT_0) throw WindowsException("WaitForSingleObject", GetLastError());
        DWORD transferred{};
        const auto completed = GetOverlappedResult(handle.get(), &operation, &transferred, FALSE);
        const auto error = completed ? ERROR_SUCCESS : GetLastError();
        if (error != ERROR_IO_INCOMPLETE) pending = false;
        if (!completed) throw WindowsException("GetOverlappedResult", error);
        return transferred;
    }
    UniqueHandle handle, event;
    std::unique_ptr<ReportDecoder> decoder;
    std::vector<std::uint8_t> buffer;
    std::wstring path;
private:
    OVERLAPPED operation{};
    bool pending{};
};
Json MetricsJson(const Snapshot& snapshot)
{
    const auto& metrics = snapshot.recovery.health.metrics;
    return {{"reports_received", metrics.reportsReceived}, {"read_failures", metrics.readFailures},
        {"suspected_dropouts_user_marked", metrics.suspectedDropouts},
        {"confirmed_protocol_dropouts", metrics.confirmedDropouts}, {"recoveries", metrics.successfulRecoveries},
        {"throttle_changes_during_marked_dropout", snapshot.throttleChangesDuringDropout},
        {"stick_changes_during_marked_dropout", snapshot.stickChangesDuringDropout},
        {"classification", "UNKNOWN: no verified PS28 stick-side signature"},
        {"recovery_state", RecoveryName(snapshot.recovery.state)}};
}
}
std::filesystem::path DefaultDataDirectory()
{
    PWSTR raw{};
    const auto result = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &raw);
    if (FAILED(result)) throw std::runtime_error("SHGetKnownFolderPath(LocalAppData) failed");
    const auto free = [](wchar_t* value) { CoTaskMemFree(value); };
    std::unique_ptr<wchar_t, decltype(free)> owner(raw, free);
    return std::filesystem::path(owner.get()) / L"X52BattlefieldMapper";
}
InspectorService::InspectorService(std::filesystem::path directory)
    : directory_(std::move(directory)), journal_(directory_) {}
InspectorService::~InspectorService() { Stop(); }
void InspectorService::Start()
{
    if (!worker_.joinable()) worker_ = std::jthread([this](std::stop_token stop) { Run(stop); });
}
void InspectorService::Stop() { if (worker_.joinable()) { worker_.request_stop(); worker_.join(); } }
void InspectorService::Send(Command command)
{
    std::lock_guard lock(mutex_);
    if (commands_.size() >= 128) throw std::runtime_error("Inspector command queue is full");
    commands_.push_back(std::move(command));
}
Snapshot InspectorService::Read() const { std::lock_guard lock(mutex_); return snapshot_; }
void InspectorService::Publish(const Snapshot& snapshot) { std::lock_guard lock(mutex_); snapshot_ = snapshot; }

void InspectorService::Run(std::stop_token stop)
{
    Snapshot current;
    AxisFilter axisFilter;
    std::unique_ptr<ReadSession> session;
    std::deque<Json> preRoll;
    std::vector<Json> capture;
    std::future<std::filesystem::path> exportTask;
    std::map<std::string, Json> candidateEvidence;
    std::map<std::string, std::int64_t> candidateDeviation;
    std::size_t captureBytes{};
    std::map<std::uint8_t, std::vector<std::uint8_t>> previous, baselineRaw;
    X52State baseline;
    std::set<std::string> candidates;
    auto lastRecovery = current.recovery.state;
    const auto frequency = QpcFrequency();
    const auto now = [frequency] { return 1000.0 * static_cast<double>(Qpc()) / static_cast<double>(frequency); };
    auto nextScan = 0.0;
    auto nextPublish = 0.0;
    auto lastParseError = std::string{};
    const auto record = [&](Json entry) {
        if (!entry.contains("qpc")) entry["qpc"] = Qpc();
        if (!entry.contains("utc")) entry["utc"] = UtcNow();
        preRoll.push_back(entry);
        if (preRoll.size() > PreRollReports) preRoll.pop_front();
        if (current.captureActive) {
            captureBytes += entry.dump().size();
            capture.push_back(std::move(entry));
            if (captureBytes >= CaptureLimitBytes) {
                current.captureActive = false;
                current.status = "Capture reached 64 MiB; press Stop / save capture to export";
                journal_.Event("capture_limit_reached");
            }
        }
    };
    const auto event = [&](const std::string& type, Json data = {}) {
        journal_.Event(type, data);
        data["event"] = type;
        record(std::move(data));
    };
    const auto close = [&] {
        if (session) { event("device_closed", {{"path", Utf8(session->path)}}); session.reset(); }
        current.connected = false;
        axisFilter.Reset();
        current.recovery.TransportLost(now(), false);
        current.safe = current.recovery.SafeState(current.filtered, current.assignments, now());
    };
    const auto startCapture = [&](const std::string& label) {
        if (current.captureActive) return;
        if (!capture.empty()) throw std::runtime_error("Save the existing capture before starting another");
        capture.assign(preRoll.begin(), preRoll.end()); captureBytes = 0;
        for (const auto& entry : capture) captureBytes += entry.dump().size();
        current.captureActive = true; current.captureReports = 0;
        current.throttleChangesDuringDropout = 0; current.stickChangesDuringDropout = 0;
        event("capture_started", {{"label", label}, {"preroll_records", preRoll.size()}});
        current.status = "Capturing every report. Mark dropout and return when physically observed.";
    };
    const auto exportReport = [&] {
        if (exportTask.valid()) throw std::runtime_error("A diagnostic export is still in progress");
        const auto path = directory_ / ("diagnostic-" + std::to_string(Qpc()) + ".json");
        Json report{{"schema", 1}, {"vid", "06A3"}, {"pid", "075C"}, {"utc", UtcNow()},
            {"qpc_frequency", frequency}, {"inventory", Utf8(current.inventory)},
            {"metrics", MetricsJson(current)}, {"physical", StateJson(current.physical)},
            {"safe_internal", StateJson(current.safe)}, {"events_file", Utf8(journal_.path().wstring())},
            {"filtered_normalized", StateJson(current.filtered)},
            {"input_filters", {{"enabled", current.filters.enabled}, {"smoothing_ms", current.filters.smoothingMs}, {"jitter_counts", current.filters.jitterCounts}}},
            {"assignments", AssignmentsJson(current.assignments)},
            {"event_log_dropped", journal_.Dropped()},
            {"limitations", "User markers are observations, not a verified signature. HID validity is not proof of stick health. No virtual output."}};
        current.captureActive = false;
        exportTask = std::async(std::launch::async, [path, frequency, assignments = current.assignments, report = std::move(report), records = std::move(capture)]() mutable {
            report["analysis"] = AnalyzeCapture(records, assignments, frequency);
            report["capture"] = std::move(records);
            WriteJson(path, report);
            return path;
        });
        capture.clear();
        current.status = "Writing diagnostic report in background...";
    };
    try {
        event("application_start", {{"qpc_frequency", frequency}, {"scope", "Milestone 1 inspector"}});
        try { current.assignments = LoadAssignments(directory_ / L"learned-controls.json"); }
        catch (const std::exception& error) { current.error = error.what(); event("assignments_rejected", {{"error", error.what()}}); }
        try {
            const auto path = directory_ / L"input-filters.json";
            if (std::filesystem::exists(path)) {
                if (std::filesystem::file_size(path) > 4096) throw std::runtime_error("Input filter settings exceed 4 KiB");
                std::ifstream file(path); const auto saved = Json::parse(file);
                if (saved.at("schema") != 1) throw std::runtime_error("Unsupported input filter settings schema");
                AxisFilterSettings loaded;
                loaded.enabled = saved.at("enabled").get<std::array<bool, 4>>();
                loaded.smoothingMs = saved.at("smoothing_ms").get<double>();
                loaded.jitterCounts = saved.at("jitter_counts").get<double>();
                loaded.Validate(); current.filters = loaded;
            }
        } catch (const std::exception& error) { current.error = error.what(); event("filters_rejected", {{"error", error.what()}}); }
        current.filtersReady = true;
        while (!stop.stop_requested()) {
            std::deque<Command> commands;
            { std::lock_guard lock(mutex_); commands.swap(commands_); }
            for (const auto& command : commands) {
                try {
                    switch (command.type) {
                    case CommandType::ConfigureFilters:
                        command.filters.Validate();
                        WriteJson(directory_ / L"input-filters.json", {{"schema", 1}, {"enabled", command.filters.enabled},
                            {"smoothing_ms", command.filters.smoothingMs}, {"jitter_counts", command.filters.jitterCounts}});
                        current.filters = command.filters; axisFilter.Reset();
                        current.error.clear();
                        current.status = "Input smoothing saved; raw HID evidence is unchanged";
                        event("input_filters_changed", {{"smoothing_ms", current.filters.smoothingMs}, {"jitter_counts", current.filters.jitterCounts}, {"enabled", current.filters.enabled}});
                        break;
                    case CommandType::Rescan:
                        current.inventory = InventoryText(EnumerateHid(stop));
                        nextScan = 0; break;
                    case CommandType::Reinitialize:
                        event("manual_reinitialize"); close(); current.recovery.Reopening(); nextScan = 0; break;
                    case CommandType::DeviceEvent:
                    {
                        auto path = Wide(command.text);
                        std::transform(path.begin(), path.end(), path.begin(), [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
                        if (path.find(L"vid_06a3&pid_075c") == std::wstring::npos) break;
                        event(command.removed ? "windows_device_removal" : "windows_device_arrival", {{"path", command.text}});
                        if (session && command.removed && _wcsicmp(session->path.c_str(), Wide(command.text).c_str()) == 0) {
                            close(); current.recovery.TransportLost(now(), true);
                        }
                        if (!session) nextScan = 0;
                        break;
                    }
                    case CommandType::StartLearn:
                        if (!session || current.raw.empty()) throw std::runtime_error("Wait for a PS28 input report before learning");
                        baseline = current.physical; baselineRaw = previous; candidates.clear(); candidateEvidence.clear(); candidateDeviation.clear();
                        current.learnRanges.clear();
                        for (const auto& [id, control] : baseline.controls) current.learnRanges[id] = {control.raw, control.raw};
                        current.learnCandidates.clear(); current.learnEvidence = Json::object();
                        current.learning = true; current.status = "Learning: move or press ONE control, then select the matching HID usage";
                        event("learn_started"); break;
                    case CommandType::SaveLearn:
                        if (!current.learning || !candidates.contains(command.id)) throw std::runtime_error("Select a changed control from this Learn session");
                        SaveAssignment(directory_ / L"learned-controls.json", command.id, command.learned, candidateEvidence.at(command.id));
                        current.assignments = LoadAssignments(directory_ / L"learned-controls.json");
                        current.learning = false; current.status = "Saved observed assignment; repeat testing before treating it as verified";
                        event("control_assigned", {{"id", command.id}, {"name", command.learned.name}}); break;
                    case CommandType::AssignControl:
                    {
                        const auto input = current.physical.controls.find(command.id);
                        const auto* physical = FindPhysicalControl(command.learned.physicalId);
                        if (input == current.physical.controls.end() || !physical || !CanLink(*physical, command.learned.part, input->second.kind))
                            throw std::runtime_error("The selected physical control/position does not match this HID representation");
                        auto learned = command.learned;
                        learned.name = PhysicalName(*physical, learned.part);
                        learned.group = physical->group;
                        if (input->second.kind == ControlKind::Button) learned.neutral = 0;
                        if (input->second.kind == ControlKind::Hat) learned.neutral = -1;
                        const auto evidence = candidateEvidence.find(command.id);
                        learned.status = current.learning && evidence != candidateEvidence.end() ? "OBSERVED" : "USER_ASSIGNED";
                        Json selectedEvidence{{"source", "physical control picker"}, {"utc", UtcNow()},
                            {"hid_id", command.id}, {"representation", static_cast<int>(input->second.kind)},
                            {"raw", input->second.raw}, {"physical_catalog", "X52 non-Pro; manufacturer guide pp. 3-4"}};
                        if (learned.status == "OBSERVED") selectedEvidence["learn_observation"] = evidence->second;
                        if (current.controlEvidence.contains(command.id)) selectedEvidence["last_hid_transition"] = current.controlEvidence.at(command.id);
                        SaveAssignment(directory_ / L"learned-controls.json", command.id, learned, selectedEvidence);
                        current.assignments = LoadAssignments(directory_ / L"learned-controls.json");
                        current.assignmentRequest = command.request; current.assignmentError.clear(); current.error.clear();
                        current.status = "Linked " + command.id + " to " + learned.name;
                        event("control_assigned", {{"id", command.id}, {"name", learned.name}, {"status", learned.status}, {"physical_id", learned.physicalId}, {"part", learned.part}});
                        break;
                    }
                    case CommandType::ClearAssignment:
                        RemoveAssignment(directory_ / L"learned-controls.json", command.id);
                        current.assignments = LoadAssignments(directory_ / L"learned-controls.json");
                        current.assignmentRequest = command.request; current.assignmentError.clear(); current.error.clear();
                        current.status = "Cleared link for " + command.id;
                        event("control_assignment_cleared", {{"id", command.id}}); break;
                    case CommandType::StartCapture:
                        startCapture(command.text); break;
                    case CommandType::StopCapture:
                        event("capture_stopped"); current.captureActive = false; exportReport(); break;
                    case CommandType::MarkDropout:
                        if (!current.captureActive && capture.empty()) startCapture("Automatic capture from manual dropout marker");
                        current.recovery.SuspendStick(now());
                        event("stick_dropout_user_mark", {{"note", command.text}}); break;
                    case CommandType::MarkReturn:
                        current.recovery.ReturnStick(now());
                        event("stick_return_user_mark", {{"note", command.text}}); break;
                    case CommandType::Export:
                        if (current.captureActive) event("capture_stopped_for_export");
                        exportReport(); break;
                    case CommandType::Configure:
                        if (command.settings.validationReports < 2 || command.settings.validationReports > 100 ||
                            !std::isfinite(command.settings.blendMs) || command.settings.blendMs < 0 || command.settings.blendMs > 1000 ||
                            !std::isfinite(command.settings.staleMs) || command.settings.staleMs < 100 || command.settings.staleMs > 60000)
                            throw std::runtime_error("Recovery settings: validation 2..100; blend 0..1000 ms; freshness 100..60000 ms");
                        current.recovery.settings = command.settings;
                        event("recovery_settings_changed", {{"validation_reports", command.settings.validationReports},
                            {"blend_ms", command.settings.blendMs}, {"stale_ms", command.settings.staleMs},
                            {"hold_last", command.settings.behaviour == DropoutBehaviour::HoldLast}}); break;
                    }
                } catch (const std::exception& error) {
                    current.error = error.what();
                    if (command.type == CommandType::AssignControl || command.type == CommandType::ClearAssignment) {
                        current.assignmentRequest = command.request; current.assignmentError = error.what();
                    }
                    event("command_failed", {{"error", error.what()}});
                }
            }
            if (!session && now() >= nextScan) {
                nextScan = now() + RediscoverMs;
                try {
                    auto devices = EnumerateHid(stop);
                    current.inventory = InventoryText(devices);
                    std::vector<HidDeviceInfo> targets;
                    for (const auto& device : devices)
                        if (device.isPs28() && device.capabilities && device.capabilities->UsagePage == 1 &&
                            (device.capabilities->Usage == 4 || device.capabilities->Usage == 5)) targets.push_back(device);
                    if (targets.size() == 1) {
                        session = std::make_unique<ReadSession>(targets.front());
                        previous.clear(); baselineRaw.clear(); current.physical.controls.clear(); current.safe.controls.clear();
                        current.filtered.controls.clear(); axisFilter.Reset();
                        current.raw.clear(); current.changes.clear(); current.learning = false; current.controlEvidence.clear();
                        current.device = targets.front().product + L"  |  PS28  |  06A3:075C";
                        current.connected = true;
                        current.status = "Connected; reading HID reports. Physical stick health is unverified.";
                        current.error.clear();
                        event("device_opened", {{"path", Utf8(session->path)}, {"input_report_length", session->buffer.size()}});
                    } else current.status = targets.empty() ? "Waiting for PS28 06A3:075C; automatic discovery every second" :
                        "Multiple PS28 game-controller collections; disconnect extra devices to select safely";
                } catch (const std::exception& error) {
                    if (current.error != error.what()) event("discovery_or_open_failed", {{"error", error.what()}});
                    current.error = error.what();
                }
            }
            if (session) {
                try {
                    const auto count = session->Read();
                    if (count) {
                        const auto inputTick = Qpc();
                        const auto reportTime = 1000.0 * static_cast<double>(inputTick) / static_cast<double>(frequency);
                        const auto before = current.physical;
                        std::string decodeError;
                        const auto span = std::span(session->buffer).first(*count);
                        const auto valid = session->decoder->Decode(span, current.physical, decodeError);
                        const auto reportId = span.empty() ? std::uint8_t{} : span[0];
                        if (valid) for (auto& [id, control] : current.physical.controls) {
                            (void)id;
                            if (control.reportId == reportId) control.lastSeenMs = reportTime;
                        }
                        current.previous = previous[reportId];
                        current.raw.assign(span.begin(), span.end());
                        current.changes = Differences(current.previous, current.raw);
                        previous[reportId] = current.raw;
                        current.utc = UtcNow(); current.lastReportMs = reportTime; ++current.sequence;
                        if (valid) for (const auto& [id, control] : current.physical.controls) {
                            const auto old = before.controls.find(id);
                            if (control.reportId == reportId && old != before.controls.end() && control.raw != old->second.raw)
                                current.controlEvidence[id] = {{"utc", current.utc}, {"qpc", inputTick}, {"raw_before", old->second.raw},
                                    {"raw_after", control.raw}, {"previous_report", Hex(current.previous)}, {"report", Hex(current.raw)},
                                    {"changed_bits", DiffJson(current.changes)}};
                        }
                        if (valid) current.filtered = axisFilter.Process(current.physical, current.assignments, current.filters, reportId, reportTime);
                        current.safe = current.recovery.Process(current.filtered, current.assignments, reportId, valid, reportTime);
                        current.processMs = 1000.0 * static_cast<double>(Qpc() - inputTick) / static_cast<double>(frequency);
                        if (!valid && decodeError != lastParseError) event("hid_parse_error", {{"error", decodeError}});
                        lastParseError = decodeError;
                        if (!valid) current.error = decodeError;
                        if (current.recovery.stickSuspended && valid) {
                            for (const auto& [id, control] : current.physical.controls) {
                                const auto old = before.controls.find(id);
                                const auto assigned = current.assignments.find(id);
                                if (old == before.controls.end() || assigned == current.assignments.end() || old->second.raw == control.raw) continue;
                                if (assigned->second.group == InputGroup::Throttle) ++current.throttleChangesDuringDropout;
                                if (assigned->second.group == InputGroup::Stick) ++current.stickChangesDuringDropout;
                            }
                        }
                        if (current.learning && valid) {
                            Json changed = Json::array();
                            for (const auto& [id, control] : current.physical.controls) {
                                const auto old = baseline.controls.find(id);
                                if (old != baseline.controls.end()) {
                                    auto& range = current.learnRanges[id];
                                    range.first = std::min(range.first, control.raw);
                                    range.second = std::max(range.second, control.raw);
                                }
                                if (control.reportId == reportId && old != baseline.controls.end() && old->second.raw != control.raw) {
                                    candidates.insert(id); changed.push_back(id);
                                }
                            }
                            if (!changed.empty()) current.learnEvidence = {{"utc", current.utc}, {"report_id", reportId},
                                {"baseline", Hex(baselineRaw[reportId])}, {"raw", Hex(current.raw)},
                                {"changed_bits", DiffJson(Differences(baselineRaw[reportId], current.raw))}, {"changed_controls", changed},
                                {"note", "Co-occurrence only; byte/bit correlation does not prove a physical mapping"}};
                            for (const auto& id : changed) {
                                const auto key = id.get<std::string>();
                                const auto deviation = std::abs(current.physical.controls.at(key).raw - baseline.controls.at(key).raw);
                                if (deviation > candidateDeviation[key]) { candidateEvidence[key] = current.learnEvidence; candidateDeviation[key] = deviation; }
                            }
                            current.learnCandidates.assign(candidates.begin(), candidates.end());
                        }
                        Json entry{{"event", "report"}, {"qpc", inputTick}, {"utc", current.utc}, {"read_status", "SUCCESS"},
                            {"length", *count}, {"report_id", reportId}, {"raw", Hex(current.raw)},
                            {"previous", Hex(current.previous)}, {"changed_bits", DiffJson(current.changes)},
                            {"decoded", StateJson(current.physical)}, {"valid", valid}, {"parse_error", decodeError},
                            {"recovery", RecoveryName(current.recovery.state)}};
                        record(std::move(entry));
                        if (current.captureActive) ++current.captureReports;
                    }
                } catch (const std::system_error& error) {
                    ++current.recovery.health.metrics.readFailures;
                    current.error = error.what();
                    event("hid_read_error", {{"error", error.what()}, {"win32_error", error.code().value()}});
                    close(); current.recovery.Reopening(); nextScan = now() + RediscoverMs;
                }
            } else std::this_thread::sleep_for(std::chrono::milliseconds(ReaderWaitMs));
            current.recovery.Tick(now());
            current.safe = current.recovery.SafeState(current.filtered, current.assignments, now());
            if (lastRecovery != current.recovery.state) {
                event("recovery_transition", {{"from", RecoveryName(lastRecovery)}, {"to", RecoveryName(current.recovery.state)}});
                lastRecovery = current.recovery.state;
            }
            if (exportTask.valid() && exportTask.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                try {
                    current.lastExport = Utf8(exportTask.get().wstring());
                    current.status = "Saved " + current.lastExport;
                    event("diagnostic_exported", {{"path", current.lastExport}});
                } catch (const std::exception& error) { current.error = error.what(); event("export_failed", {{"error", error.what()}}); }
            }
            if (now() >= nextPublish) {
                if (!journal_.Error().empty()) current.error = journal_.Error();
                Publish(current); nextPublish = now() + 75;
            }
        }
        close();
        if (exportTask.valid()) current.lastExport = Utf8(exportTask.get().wstring());
        if (current.captureActive || !capture.empty()) { current.captureActive = false; exportReport(); }
        if (exportTask.valid()) current.lastExport = Utf8(exportTask.get().wstring());
        event("application_shutdown");
    } catch (const std::exception& error) {
        current.error = error.what(); current.status = "Inspector stopped after error; restart after correcting the cause";
        close();
        try { event("worker_stopped", {{"error", error.what()}}); } catch (...) {}
    }
    Publish(current);
}
}
