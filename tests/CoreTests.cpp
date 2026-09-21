#include "app/InspectorService.hpp"
#include "util/Text.hpp"
#include "diagnostics/CaptureAnalysis.hpp"
#include "input/PhysicalControls.hpp"
#include "profiles/BattlefieldProfiles.hpp"
#include <set>
#include "device/MfdSettings.hpp"
#include "ui/ControlPhoto.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void Tests()
{
    using namespace x52;
    Require(Normalize(0, 0, 100) == -1 && Normalize(50, 0, 100) == 0 && Normalize(100, 0, 100) == 1, "normalization endpoints");
    AxisFilter filter; AxisFilterSettings filterSettings;
    X52State noisy;
    noisy.controls["axis"] = {"axis", ControlKind::Axis, 128, 0, 255, Normalize(128, 0, 255), true, 0};
    noisy.controls["button"] = {"button", ControlKind::Button, 1, 0, 1, 1, true, 0};
    Assignments links{{"axis", {"Throttle", InputGroup::Throttle, -1, "throttle.main"}}};
    const auto initial = filter.Process(noisy, links, filterSettings, 0, 0);
    for (int i = 1; i <= 100; ++i) {
        noisy.controls.at("axis").raw = 128 + (i % 2);
        noisy.controls.at("axis").normalized = Normalize(noisy.controls.at("axis").raw, 0, 255);
        const auto quiet = filter.Process(noisy, links, filterSettings, 0, i * 10.0);
        Require(std::abs(quiet.controls.at("axis").normalized - initial.controls.at("axis").normalized) < 1e-10, "one-count throttle jitter held stable");
        Require(quiet.controls.at("axis").raw == noisy.controls.at("axis").raw && quiet.controls.at("button").normalized == 1, "raw values and buttons untouched");
    }
    noisy.controls.at("axis").raw = 255; noisy.controls.at("axis").normalized = 1;
    const auto moving = filter.Process(noisy, links, filterSettings, 0, 1010);
    Require(moving.controls.at("axis").normalized > initial.controls.at("axis").normalized && moving.controls.at("axis").normalized < 1, "deliberate movement smoothed");
    X52State settled;
    for (int i = 2; i <= 100; ++i) settled = filter.Process(noisy, links, filterSettings, 0, 1000 + i * 10.0);
    Require(settled.controls.at("axis").normalized == 1, "full travel endpoint reached");
    filterSettings.enabled[0] = false;
    Require(filter.Process(noisy, links, filterSettings, 0, 2010).controls.at("axis").normalized == 1, "disabled filter bypasses immediately");
    filterSettings.enabled[0] = true; filter.Reset();
    Require(filter.Process(noisy, links, filterSettings, 0, 2020).controls.at("axis").normalized == 1, "reconnect initializes at current position");
    links.at("axis").physicalId = "stick.x";
    noisy.controls.at("axis").normalized = -1;
    Require(filter.Process(noisy, links, filterSettings, 0, 2030).controls.at("axis").normalized == -1, "stick axes never filtered");
    links.at("axis").physicalId = "throttle.main";
    const auto atRate = [&](double period) {
        AxisFilter timed; AxisFilterSettings settings; auto source = noisy;
        source.controls.at("axis").normalized = 0;
        (void)timed.Process(source, links, settings, 0, 0);
        source.controls.at("axis").normalized = 0.8;
        X52State output;
        for (double t = period; t <= 100; t += period) output = timed.Process(source, links, settings, 0, t);
        return output.controls.at("axis").normalized;
    };
    Require(std::abs(atRate(5) - atRate(20)) < 1e-10, "filter response independent of report frequency");
    X52RecoveryManager filterSafety; filterSafety.settings.validationReports = 2; filterSafety.settings.blendMs = 0;
    (void)filterSafety.Process(moving, links, 0, true, 0);
    (void)filterSafety.Process(moving, links, 0, true, 1);
    filterSafety.TransportLost(2, false);
    Require(filterSafety.SafeState(moving, links, 2).controls.at("axis").normalized == -1, "failsafe bypasses filter delay immediately");
    bool invalidFilter = false;
    filterSettings.smoothingMs = -1;
    try { filterSettings.Validate(); } catch (const std::exception&) { invalidFilter = true; }
    Require(invalidFilter, "invalid filter configuration rejected");
    Require(FindControlPhoto("throttle.e")->spots[2].y < FindControlPhoto("throttle.d")->spots[2].y &&
        FindControlPhoto("throttle.d")->spots[2].y < FindControlPhoto("throttle.clutch")->spots[2].y, "user-confirmed E/D/I physical positions");
    Require(SignExtend(255, 8, true) == -1 && SignExtend(255, 8, false) == 255 && SignExtend(0x80000000, 32, true) == -2147483648LL, "signed HID values");
    const std::vector<std::uint8_t> before{0, 0x10}, after{0, 0x21};
    const auto changes = Differences(before, after);
    Require(changes.size() == 1 && changes[0].byte == 1 && changes[0].mask == 0x31, "byte and bit changes");
    X52State physical;
    physical.controls["stick"] = {"stick", ControlKind::Axis, 100, 0, 100, 1, true, 0};
    physical.controls["throttle"] = {"throttle", ControlKind::Axis, 75, 0, 100, 0.5, true, 0};
    physical.controls["hat"] = {"hat", ControlKind::Hat, 2, 0, 7, 2, true, 0};
    Assignments assignments{{"stick", {"Stick X", InputGroup::Stick, 0}}, {"throttle", {"Throttle", InputGroup::Throttle, -1}},
        {"hat", {"Hat", InputGroup::Stick, -1}}};
    X52RecoveryManager recovery;
    recovery.settings.blendMs = 100;
    for (int i = 0; i < 4; ++i) Require(!recovery.Process(physical, assignments, 0, true, i * 10).controls.at("stick").valid, "must validate multiple reports");
    auto safe = recovery.Process(physical, assignments, 0, true, 40);
    Require(safe.controls.at("stick").normalized == 0, "blend starts at neutral");
    safe = recovery.Process(physical, assignments, 0, true, 90);
    Require(std::abs(safe.controls.at("stick").normalized - 0.5) < 0.001, "halfway recovery blend");
    safe = recovery.Process(physical, assignments, 0, true, 150);
    Require(safe.controls.at("stick").normalized == 1, "full recovered input");
    Require(recovery.health.stick == StickHealth::Unknown, "packet validity does not imply stick health");
    recovery.SuspendStick(200);
    safe = recovery.Process(physical, assignments, 0, true, 210);
    Require(safe.controls.at("stick").normalized == 0 && safe.controls.at("hat").normalized == -1, "stick failsafe");
    Require(safe.controls.at("throttle").normalized == 0.5, "throttle survives stick-only suspension");
    recovery.ReturnStick(220);
    for (int i = 0; i < 5; ++i) safe = recovery.Process(physical, assignments, 0, true, 230 + i * 10);
    Require(!recovery.stickSuspended && recovery.health.metrics.successfulRecoveries == 1, "stick validation completes");
    recovery.TransportLost(400, true);
    safe = recovery.SafeState(physical, assignments, 400);
    Require(safe.controls.at("throttle").normalized == -1, "whole transport failure neutralizes all groups");
    for (int i = 0; i < 3; ++i) (void)recovery.Process(physical, assignments, 0, true, 410 + i * 10);
    (void)recovery.Process(physical, assignments, 0, false, 440);
    for (int i = 0; i < 4; ++i) safe = recovery.Process(physical, assignments, 0, true, 450 + i * 10);
    Require(!safe.controls.at("stick").valid, "malformed report resets consecutive validation");
    Require(!UnsupportedHardwareRecovery{}.AttemptRecovery(), "no speculative hardware writes");
    X52RecoveryManager stationary;
    stationary.settings.blendMs = 0;
    for (int i = 0; i < 200; ++i) (void)stationary.Process(physical, assignments, 0, true, i * 10);
    Require(stationary.state == RecoveryState::Normal && stationary.health.metrics.suspectedDropouts == 0, "stationary controls never imply dropout");
    stationary.Tick(5000);
    Require(!stationary.SafeState(physical, assignments, 5000).controls.at("stick").valid &&
        stationary.health.stick == StickHealth::Unknown, "watchdog withholds stale input without inventing diagnosis");
    X52RecoveryManager hold;
    hold.settings.blendMs = 0;
    hold.settings.behaviour = DropoutBehaviour::HoldLast;
    for (int i = 0; i < 5; ++i) (void)hold.Process(physical, assignments, 0, true, i);
    hold.SuspendStick(10);
    Require(hold.SafeState(physical, assignments, 10).controls.at("stick").normalized == 1, "explicit hold-last mode");
    X52RecoveryManager multiple;
    auto multi = physical;
    multi.controls.at("throttle").reportId = 1;
    for (int i = 0; i < 5; ++i) safe = multiple.Process(multi, assignments, 0, true, i);
    Require(!safe.controls.at("throttle").valid, "one report ID cannot validate another");
    multiple.settings.blendMs = 0;
    for (int i = 0; i < 5; ++i) (void)multiple.Process(multi, assignments, 1, true, 10 + i);
    multiple.SuspendStick(20); multiple.ReturnStick(21);
    for (int i = 0; i < 10; ++i) (void)multiple.Process(multi, assignments, 1, true, 30 + i);
    Require(multiple.stickSuspended, "throttle reports cannot validate learned stick return");
    for (int i = 0; i < 5; ++i) (void)multiple.Process(multi, assignments, 0, true, 50 + i);
    Require(!multiple.stickSuspended, "learned stick reports restore stick");
    X52RecoveryManager unassigned;
    for (int i = 0; i < 5; ++i) (void)unassigned.Process(physical, {}, 0, true, i);
    unassigned.SuspendStick(10); unassigned.ReturnStick(11);
    for (int i = 0; i < 5; ++i) (void)unassigned.Process(physical, {}, 0, true, 20 + i);
    Require(!unassigned.stickSuspended, "explicit user return can validate unassigned report groups");
    for (int cycle = 0; cycle < 100; ++cycle) {
        recovery.TransportLost(cycle * 1000.0, true);
        for (int i = 0; i < 5; ++i) (void)recovery.Process(physical, assignments, 0, true, cycle * 1000.0 + i * 10);
        Require(recovery.state == RecoveryState::Restoring, "repeat transport recovery");
    }
    const auto testDir = std::filesystem::current_path() / L"out" / L"core-test-data" / std::to_wstring(GetCurrentProcessId());
    std::filesystem::create_directories(testDir);
    const auto file = testDir / L"learned-controls.json";
    const auto battlefieldFile = testDir / L"battlefield-profile";
    { std::ofstream stream(battlefieldFile); stream << "GstKeyBinding.jet.ConceptFire.0.type 0\nGstKeyBinding.jet.ConceptFire.0.button 57\nGstKeyBinding.jet.ConceptFire.0.axis 0\nGstKeyBinding.jet.ConceptFire.0.negate 0\n"; }
    const auto imported = ReadBattlefieldBindings(battlefieldFile);
    Require(imported.size() == 1 && imported[0].context == "jet", "Battlefield import groups action fields");
    const auto spaceAction = ProfileOutput(imported[0]);
    Require(spaceAction && spaceAction->device == "keyboard" && spaceAction->usage == 0x2c && spaceAction->page == 7, "Battlefield Space maps to profiler sample USB usage");
    auto notAKey = imported[0]; notAKey.type = 2;
    Require(!ProfileOutput(notAKey), "joystick bindings cannot be silently converted to keyboard output");
    notAKey.type = 0; notAKey.button = 255;
    Require(!ProfileOutput(notAKey), "unbound keyboard sentinel cannot become a key");
    { std::ofstream stream(battlefieldFile, std::ios::app); stream << "GstKeyBinding.jet.ConceptFire.0.button 45\n"; }
    bool badBinding = false;
    try { (void)ReadBattlefieldBindings(battlefieldFile); } catch (const std::exception&) { badBinding = true; }
    Require(badBinding, "duplicate Battlefield fields are rejected");
    const auto minimalProfile = ParsePr0("[profile='Test spaces' version=5 [commands [actioncommand=sample [actionblock [action device=keyboard usage=44 page=7 value=1]]]]]");
    Require(SerializePr0(ParsePr0(SerializePr0(minimalProfile))) == SerializePr0(minimalProfile), "PR0 parser and serializer retain nested commands and quoting");
    for (const auto& invalid : {std::string("[profile='unclosed]"), std::string("[profile=x] trailing"), std::string(40, '[')}) {
        bool rejectedProfile = false;
        try { (void)ParsePr0(invalid); } catch (const std::exception&) { rejectedProfile = true; }
        Require(rejectedProfile, "malformed PR0 input rejected");
    }
    WriteJson(file, {{"schema", 1}, {"vid", "06A3"}, {"pid", "075C"}, {"controls", Json::object()}});
    SaveAssignment(file, "r0:p1:u30:l1", {"Test axis", InputGroup::Stick, 0}, {{"test", true}});
    Require(LoadAssignments(file).at("r0:p1:u30:l1").name == "Test axis", "assignment JSON roundtrip");
    std::set<std::string> catalogIds;
    for (const auto& physicalControl : PhysicalControls()) Require(catalogIds.insert(physicalControl.id).second, "physical catalog IDs are unique");
    const auto* physicalHat = FindPhysicalControl("stick.hat_upper");
    Require(physicalHat && CanLink(*physicalHat, "Up", ControlKind::Button) && !CanLink(*physicalHat, "Whole hat", ControlKind::Button) &&
        CanLink(*physicalHat, "Whole hat", ControlKind::Hat) && !CanLink(*physicalHat, "Up", ControlKind::Axis), "hat direction versus aggregate representation");
    const auto* physicalAxis = FindPhysicalControl("stick.x");
    Require(physicalAxis && CanLink(*physicalAxis, "", ControlKind::Axis) && !CanLink(*physicalAxis, "", ControlKind::Button), "axis cannot be linked to HID button");
    LearnedControl selected{PhysicalName(*physicalHat, "Up"), InputGroup::Stick, 0, physicalHat->id, "Up", "USER_ASSIGNED"};
    SaveAssignment(file, "r0:p9:u1:l1", selected, {{"test", true}});
    Require(LoadAssignments(file).at("r0:p9:u1:l1").physicalId == physicalHat->id && LoadAssignments(file).at("r0:p9:u1:l1").status == "USER_ASSIGNED", "catalog selection roundtrip and evidence status");
    bool duplicateRejected = false;
    try { SaveAssignment(file, "r0:p9:u2:l1", selected, Json::object()); } catch (const std::exception&) { duplicateRejected = true; }
    Require(duplicateRejected && !LoadAssignments(file).contains("r0:p9:u2:l1"), "duplicate physical link rejected without overwriting file");
    selected.part = "Right"; selected.name = PhysicalName(*physicalHat, selected.part);
    SaveAssignment(file, "r0:p9:u1:l1", selected, {{"test", true}});
    Require(LoadAssignments(file).at("r0:p9:u1:l1").part == "Right", "existing link can be corrected");
    RemoveAssignment(file, "r0:p9:u1:l1");
    Require(!LoadAssignments(file).contains("r0:p9:u1:l1") && LoadAssignments(file).contains("r0:p1:u30:l1"), "clear affects only selected HID link");
    const auto rejected = [&file](const std::string& content) {
        { std::ofstream stream(file); stream << content; }
        try { (void)LoadAssignments(file); return false; } catch (const std::exception&) { return true; }
    };
    Require(rejected("{bad json"), "malformed JSON rejected");
    Require(rejected(R"({"schema":1,"vid":"06A3","pid":"0762","controls":{}})"), "X52 Pro identity rejected");
    Require(rejected(R"({"schema":1,"vid":"06A3","pid":"075C","controls":{"a":{"name":"x","group":"invalid","neutral":0}}})"), "invalid group rejected");
    Require(rejected(std::string(40, '[') + "0" + std::string(40, ']')), "deep JSON rejected");
    const auto record = [](std::int64_t time, int throttle) {
        return Json{{"event", "report"}, {"qpc", time}, {"valid", true},
            {"decoded", {{"throttle", {{"raw", throttle}}}, {"stick", {{"raw", 0}}}}}};
    };
    const std::vector<Json> trace{record(0, 0), {{"event", "stick_dropout_user_mark"}, {"qpc", 100}},
        record(200, 1), {{"event", "stick_return_user_mark"}, {"qpc", 400}}};
    const auto analysis = AnalyzeCapture(trace, assignments, 1000);
    Require(analysis.at("marked_intervals")[0].at("learned_throttle_changes") == 1 &&
        analysis.at("marked_intervals")[0].at("duration_ms") == 300 && !analysis.at("verified_signature").get<bool>(), "capture analysis preserves evidence vs inference");
    std::cout << "Core tests passed\n";
}
}
int main(int argc, char** argv)
{
    try {
        Tests();
        if (argc > 1 && (std::string(argv[1]) == "--mfd" || std::string(argv[1]) == "--mfd-roundtrip")) {
            std::vector<std::wstring> paths;
            for (const auto& device : x52::EnumerateHid()) if (device.isPs28()) paths.push_back(device.path);
            Require(paths.size() == 1, "exactly one original X52 connected");
            const auto before = x52::ReadMfdSettings(paths.front());
            std::cout << "MFD live: clutch=" << before.clutch << " latched=" << before.latched << " MFD=" << before.mfdBrightness
                << " LED=" << before.ledBrightness << " clocks=" << before.twelveHour[0] << before.twelveHour[1] << before.twelveHour[2] << '\n';
            if (std::string(argv[1]) == "--mfd-roundtrip") {
                const std::array<DWORD, 7> values{before.clutch, before.latched, before.mfdBrightness, before.ledBrightness,
                    before.twelveHour[0], before.twelveHour[1], before.twelveHour[2]};
                for (std::size_t i = 0; i < values.size(); ++i) {
                    const auto option = static_cast<x52::MfdOption>(i);
                    const auto changed = (i == 2 || i == 3) ? (values[i] == 100 ? 99u : values[i] + 1) : (values[i] ? 0u : 1u);
                    try { (void)x52::SetMfdOption(paths.front(), option, changed); }
                    catch (...) { (void)x52::SetMfdOption(paths.front(), option, values[i]); throw; }
                    (void)x52::SetMfdOption(paths.front(), option, values[i]);
                    std::cout << "Setting " << i << " changed, read back, restored and read back\n";
                }
            }
        }
        if (argc > 1 && std::string(argv[1]) == "--profiles") {
            const auto profile = x52::ReadX52Template(x52::InstalledX52TemplatePath());
            const auto buttons = x52::ProfileButtons(profile);
            Require(std::any_of(buttons.begin(), buttons.end(), [](const auto& button) { return button.id == "0x0009001E"; }), "I button available for authoring with clutch mode disabled");
            Require(std::none_of(buttons.begin(), buttons.end(), [](const auto& button) { return button.id == "0x00090006"; }), "pinkie remains reserved for shifting");
            const auto directory = std::filesystem::current_path() / L"out" / L"profile-research";
            std::filesystem::create_directories(directory);
            for (const auto game : {3, 4}) {
                const auto bindings = x52::ReadBattlefieldBindings(x52::BattlefieldSettingsPath(game));
                const auto fire = std::find_if(bindings.begin(), bindings.end(), [](const auto& entry) { return entry.context == "jet" && entry.action == "ConceptFire" && entry.type == 0 && x52::ProfileOutput(entry); });
                Require(fire != bindings.end(), "local Battlefield file has a keyboard fire binding");
                const std::vector<x52::ProfileMapping> mapping{{0, "0x00090001", *fire}};
                const auto built = x52::BuildBattlefieldPr0(profile, mapping, "BF" + std::to_string(game) + " Trigger draft");
                const auto text = x52::SerializePr0(built);
                Require(x52::SerializePr0(x52::ParsePr0(text)) == text, "generated PR0 roundtrip");
                Require(text.find("mouse-x") != std::string::npos && text.find("Scroll Down") != std::string::npos && text.find("device=keyboard") != std::string::npos,
                    "profile export preserves vendor mouse defaults while adding keyboard action");
                const auto clutchDraft = x52::BuildBattlefieldPr0(profile, {{0, "0x0009001E", *fire}}, "I button draft");
                const auto clutchText = x52::SerializePr0(clutchDraft);
                Require(clutchText.find("button=0x0009001E") != std::string::npos &&
                    x52::SerializePr0(x52::ParsePr0(clutchText)) == clutchText, "I button assignment exports and roundtrips");
                const auto duplicate = std::vector<x52::ProfileMapping>{mapping[0], mapping[0]};
                bool rejectedDuplicate = false;
                try { (void)x52::BuildBattlefieldPr0(profile, duplicate, "Duplicate"); } catch (const std::exception&) { rejectedDuplicate = true; }
                Require(rejectedDuplicate, "duplicate mode/control rejected by exporter");
                const auto path = directory / (L"BF" + std::to_wstring(game) + L"-Trigger-DRAFT.pr0");
                std::ofstream stream(path, std::ios::binary); stream << x52::EncodePr0(built); stream.close();
                Require(static_cast<bool>(stream), "write local draft export");
                Require(x52::SerializePr0(x52::ReadX52Template(path)) == text, "UTF-16LE exported file reloads with complete structure");
                std::cout << "BF" << game << ": " << bindings.size() << " bindings imported; trigger-only draft export passed (not activated)\n";
            }
        }
        if (argc > 1 && std::string(argv[1]) == "--hardware") {
            const auto directory = std::filesystem::current_path() / L"out" / L"hardware-probe";
            x52::InspectorService service(directory);
            service.Start();
            const auto waitFor = [&](const auto& predicate, const char* message) {
                for (int attempt = 0; attempt < 100; ++attempt) {
                    auto snapshot = service.Read();
                    if (predicate(snapshot)) return snapshot;
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                throw std::runtime_error(message);
            };
            std::this_thread::sleep_for(std::chrono::seconds(2));
            service.Send({x52::CommandType::StartCapture, "Read-only hardware probe"});
            std::this_thread::sleep_for(std::chrono::seconds(3));
            service.Send({x52::CommandType::StopCapture});
            auto state = waitFor([](const auto& snapshot) { return !snapshot.captureActive && !snapshot.lastExport.empty(); }, "initial capture export timed out");
            service.Send({x52::CommandType::MarkDropout, "AUTOMATED TEST marker; no physical fault was induced or observed"});
            state = waitFor([](const auto& snapshot) { return snapshot.captureActive && snapshot.recovery.stickSuspended; }, "dropout marker must arm pre-roll capture and failsafe");
            service.Send({x52::CommandType::MarkReturn, "AUTOMATED TEST return; no physical fault"});
            state = waitFor([](const auto& snapshot) { return !snapshot.recovery.stickSuspended && snapshot.recovery.state == x52::RecoveryState::Normal; }, "manual return validates unassigned controls");
            const auto priorExport = state.lastExport;
            service.Send({x52::CommandType::StopCapture});
            state = waitFor([&](const auto& snapshot) { return !snapshot.captureActive && snapshot.lastExport != priorExport; }, "marker capture export timed out");
            // Exercise the picker command path in the isolated probe directory.
            // This is a synthetic assignment, not a claimed physical correlation.
            const auto axis = std::find_if(state.physical.controls.begin(), state.physical.controls.end(),
                [](const auto& pair) { return pair.second.kind == x52::ControlKind::Axis; });
            Require(axis != state.physical.controls.end(), "PS28 probe needs a scalar input");
            const auto axisId = axis->first;
            x52::Command assign{x52::CommandType::AssignControl};
            assign.id = axisId; assign.learned.physicalId = "stick.x";
            assign.request = static_cast<std::uint64_t>(x52::Qpc());
            service.Send(assign);
            state = waitFor([&](const auto& snapshot) { return snapshot.assignmentRequest == assign.request; }, "assignment acknowledgement timed out");
            Require(state.assignmentError.empty() && state.assignments.at(axisId).physicalId == "stick.x" &&
                state.assignments.at(axisId).status == "USER_ASSIGNED", "picker command must persist an unverified user assignment");
            x52::Command clear{x52::CommandType::ClearAssignment};
            clear.id = axisId; clear.request = static_cast<std::uint64_t>(x52::Qpc());
            service.Send(clear);
            state = waitFor([&](const auto& snapshot) { return snapshot.assignmentRequest == clear.request; }, "clear acknowledgement timed out");
            Require(state.assignmentError.empty() && !state.assignments.contains(axisId), "clear command removes selected link");
            std::cout << "Isolated picker assignment / clear commands passed\n";
            const auto initialReports = state.sequence;
            for (int cycle = 0; cycle < 10; ++cycle) {
                const auto previousRecoveries = service.Read().recovery.health.metrics.successfulRecoveries;
                service.Send({x52::CommandType::Reinitialize});
                bool recovered = false;
                for (int attempt = 0; attempt < 100; ++attempt) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    state = service.Read();
                    if (state.connected && state.recovery.state == x52::RecoveryState::Normal &&
                        state.recovery.health.metrics.successfulRecoveries > previousRecoveries) { recovered = true; break; }
                }
                Require(recovered, "live software reopen failed to recover within five seconds");
            }
            state = service.Read();
            std::cout << "10 live software reopen cycles passed; reports since capture: " << state.sequence - initialReports << '\n';
            std::cout << "Device: " << x52::Utf8(state.device) << "\nReports: " << state.sequence << "\nLength: " << state.raw.size()
                << "\nControls: " << state.physical.controls.size() << "\nError: " << state.error << "\nExport: " << state.lastExport << '\n';
            x52::WriteJson(directory / L"probe-summary.json", {{"reports", state.sequence}, {"raw", x52::Hex(state.raw)},
                {"decoded", x52::StateJson(state.physical)}, {"error", state.error}, {"inventory", x52::Utf8(state.inventory)}});
            service.Stop();
            Require(state.connected && state.sequence > 10 && state.error.empty(), "hardware probe requires connected PS28 and valid reports");
        }
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
