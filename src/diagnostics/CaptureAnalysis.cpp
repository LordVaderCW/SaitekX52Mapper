#include "CaptureAnalysis.hpp"
#include <optional>

namespace x52 {
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
        current["throttle_activity"] = throttleChanges ? "OBSERVED via user-assigned controls" : "NOT ESTABLISHED (stationary or unassigned is inconclusive)";
        intervals.push_back(current); start.reset();
    };
    for (const auto& record : records) {
        const auto event = record.value("event", "");
        if (event == "stick_dropout_user_mark") {
            finish(nullptr); start = record.at("qpc").get<std::int64_t>();
            current = {{"start_user_mark", record}, {"last_report_before_mark", lastReport}};
            reports = errors = usbRemovals = throttleChanges = stickChanges = 0;
        } else if (event == "stick_return_user_mark") finish(&record);
        else if (event == "windows_device_removal" && start) ++usbRemovals;
        else if (event == "report") {
            if (start) {
                ++reports;
                if (!record.value("valid", false)) ++errors;
                else if (record.contains("decoded")) {
                    for (const auto& [id, control] : assignments) {
                        const auto& decoded = record.at("decoded");
                        if (!previous.contains(id) || !decoded.contains(id) || previous.at(id).at("raw") == decoded.at(id).at("raw")) continue;
                        if (control.group == InputGroup::Throttle) ++throttleChanges;
                        if (control.group == InputGroup::Stick) ++stickChanges;
                    }
                }
            }
            if (record.value("valid", false)) { previous = record.at("decoded"); lastReport = record; }
        }
    }
    finish(nullptr);
    return {{"marked_intervals", intervals}, {"verified_signature", false},
        {"notes", "Marker timing includes user reaction time. Absence of USB-removal events is not proof of continuous enumeration."}};
}
}
