#include "CaptureAnalysis.hpp"
#include <optional>

namespace x52 {
namespace {
const char* GroupName(InputGroup group)
{
    return group == InputGroup::Stick ? "stick" : group == InputGroup::Throttle ? "throttle" : "unknown";
}
Json ActivityAnalysis(const std::vector<Json>& records, const Assignments& assignments, std::int64_t frequency)
{
    Json controls = Json::object(), groups = Json::object(), gaps = Json::object();
    for (const auto name : {"stick", "throttle", "unknown"})
        groups[name] = {{"observed_controls", 0}, {"axis_raw_changes", 0}, {"button_hat_changes", 0}};
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> previous; // raw, unchanged-since QPC
    std::map<unsigned, std::int64_t> reportTimes;
    std::uint64_t invalid{}, readErrors{}, removals{};
    for (const auto& record : records) {
        const auto event = record.value("event", "");
        if (event == "hid_read_error") ++readErrors;
        if (event == "windows_device_removal") ++removals;
        if (event == "device_closed" || event == "device_opened" || event == "hid_read_error") previous.clear();
        if (event != "report") continue;
        const auto tick = record.at("qpc").get<std::int64_t>();
        const auto reportId = record.value("report_id", 0u);
        const auto key = std::to_string(reportId);
        if (!gaps.contains(key)) gaps[key] = {{"reports", 0}, {"max_gap_ms", 0.0}};
        auto& gap = gaps[key];
        gap["reports"] = gap.at("reports").get<std::uint64_t>() + 1;
        if (reportTimes.contains(reportId)) {
            const auto elapsed = 1000.0 * static_cast<double>(tick - reportTimes.at(reportId)) / static_cast<double>(frequency);
            if (elapsed > gap.at("max_gap_ms").get<double>()) {
                gap["max_gap_ms"] = elapsed; gap["from_qpc"] = reportTimes.at(reportId); gap["to_qpc"] = tick;
            }
        }
        reportTimes[reportId] = tick;
        if (!record.value("valid", false)) { ++invalid; previous.clear(); continue; }
        for (const auto& [id, value] : record.at("decoded").items()) {
            if (value.value("report_id", 0u) != reportId || !value.value("valid", true)) continue;
            const auto link = assignments.find(id);
            const auto group = GroupName(link == assignments.end() ? InputGroup::Unknown : link->second.group);
            const auto raw = value.at("raw").get<std::int64_t>();
            const auto kind = value.value("kind", 0);
            if (!controls.contains(id)) {
                controls[id] = {{"name", link == assignments.end() ? id : link->second.name}, {"group", group},
                    {"samples", 0}, {"raw_changes", 0}, {"raw_min", raw}, {"raw_max", raw}, {"first_raw", raw},
                    {"first_qpc", tick}, {"longest_unchanged_ms", 0.0}, {"kind", kind}};
                groups[group]["observed_controls"] = groups[group]["observed_controls"].get<std::size_t>() + 1;
            }
            auto& stats = controls[id];
            stats["samples"] = stats.at("samples").get<std::uint64_t>() + 1;
            stats["last_raw"] = raw; stats["last_qpc"] = tick;
            stats["raw_min"] = std::min(raw, stats.at("raw_min").get<std::int64_t>());
            stats["raw_max"] = std::max(raw, stats.at("raw_max").get<std::int64_t>());
            if (const auto old = previous.find(id); old != previous.end()) {
                if (old->second.first != raw) {
                    stats["raw_changes"] = stats.at("raw_changes").get<std::uint64_t>() + 1;
                    stats["last_change_qpc"] = tick;
                    const auto field = kind == static_cast<int>(ControlKind::Axis) ? "axis_raw_changes" : "button_hat_changes";
                    groups[group][field] = groups[group][field].get<std::uint64_t>() + 1;
                    old->second = {raw, tick};
                } else {
                    const auto duration = 1000.0 * static_cast<double>(tick - old->second.second) / static_cast<double>(frequency);
                    stats["longest_unchanged_ms"] = std::max(duration, stats.at("longest_unchanged_ms").get<double>());
                }
            } else previous[id] = {raw, tick};
        }
    }
    return {{"groups", groups}, {"controls", controls}, {"report_timing", gaps},
        {"invalid_reports", invalid}, {"read_errors", readErrors}, {"windows_device_removals", removals},
        {"interpretation", "Raw axis changes include noise. Unchanged values include normal rest and held controls; neither establishes stick health. Names use the export assignment snapshot."}};
}
}
Json AssignmentsJson(const Assignments& assignments)
{
    Json result = Json::object();
    for (const auto& [id, control] : assignments) result[id] = {{"name", control.name},
        {"group", control.group == InputGroup::Stick ? "stick" : control.group == InputGroup::Throttle ? "throttle" : "unknown"},
        {"neutral", control.neutral}, {"status", control.status}, {"physical_id", control.physicalId}, {"part", control.part}};
    return result;
}
Json AnalyzeCapture(const std::vector<Json>& records, const Assignments& assignments, std::int64_t frequency)
{
    if (frequency <= 0) throw std::runtime_error("Invalid capture QPC frequency");
    Json intervals = Json::array(), current, previous, lastReport;
    std::optional<std::int64_t> start;
    std::uint64_t reports{}, errors{}, usbRemovals{}, throttleChanges{}, stickChanges{};
    const auto finish = [&](const Json* returned) {
        if (!start) return;
        current["end_user_mark"] = returned ? *returned : Json{};
        current["duration_ms"] = returned ? Json(1000.0 * static_cast<double>(returned->at("qpc").get<std::int64_t>() - *start) /
            static_cast<double>(frequency)) : Json{};
        current["reports"] = reports; current["invalid_reports"] = errors;
        current["usb_removal_events"] = usbRemovals;
        current["learned_throttle_changes"] = throttleChanges;
        current["learned_stick_changes"] = stickChanges;
        current["classification"] = "USER_MARKED; stick-side protocol signature remains UNKNOWN";
        current["throttle_activity"] = throttleChanges ? "RAW CHANGES OBSERVED; analogue noise is included" : "NOT ESTABLISHED (stationary or unassigned is inconclusive)";
        intervals.push_back(current); start.reset();
    };
    for (const auto& record : records) {
        const auto event = record.value("event", "");
        if (event == "stick_dropout_user_mark") {
            finish(nullptr); start = record.at("qpc").get<std::int64_t>();
            current = {{"start_user_mark", record}, {"last_report_before_mark", lastReport}, {"control_raw_changes", Json::object()}};
            reports = errors = usbRemovals = throttleChanges = stickChanges = 0;
        } else if (event == "stick_return_user_mark") finish(&record);
        else if (event == "windows_device_removal" && start) ++usbRemovals;
        else if (event == "device_closed" || event == "device_opened" || event == "hid_read_error") previous = Json::object();
        else if (event == "report") {
            if (start) {
                ++reports;
                if (!record.value("valid", false)) ++errors;
                else if (record.contains("decoded")) {
                    for (const auto& [id, control] : assignments) {
                        const auto& decoded = record.at("decoded");
                        if (!previous.contains(id) || !decoded.contains(id) || previous.at(id).at("raw") == decoded.at(id).at("raw")) continue;
                        if (decoded.at(id).value("report_id", 0u) != record.value("report_id", 0u)) continue;
                        current["control_raw_changes"][id] = current["control_raw_changes"].value(id, std::uint64_t{}) + 1;
                        if (control.group == InputGroup::Throttle) ++throttleChanges;
                        if (control.group == InputGroup::Stick) ++stickChanges;
                    }
                }
            }
            if (record.value("valid", false)) { previous = record.at("decoded"); lastReport = record; }
            else previous = Json::object();
        }
    }
    finish(nullptr);
    return {{"marked_intervals", intervals}, {"activity", ActivityAnalysis(records, assignments, frequency)}, {"verified_signature", false},
        {"notes", "Marker timing includes user reaction time. Absence of USB-removal events is not proof of continuous enumeration."}};
}
}
