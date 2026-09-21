#include "LearnedControls.hpp"
#include <cmath>
#include "../input/PhysicalControls.hpp"

namespace x52 {
namespace {
Json Read(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path)) return {{"schema", 1}, {"vid", "06A3"}, {"pid", "075C"}, {"controls", Json::object()}};
    if (std::filesystem::file_size(path) > 4 * 1024 * 1024) throw std::runtime_error("Learned controls file exceeds 4 MiB");
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot read learned controls file");
    auto json = Json::parse(stream, [](int depth, Json::parse_event_t, Json&) {
        if (depth > 32) throw std::runtime_error("Learned controls JSON nesting exceeds 32 levels");
        return true;
    });
    if (json.at("schema") != 1 || json.at("vid") != "06A3" || json.at("pid") != "075C" || !json.at("controls").is_object())
        throw std::runtime_error("Unsupported learned controls schema/device; expected schema 1, 06A3:075C");
    return json;
}
LearnedControl Parse(const Json& value)
{
    LearnedControl control;
    control.name = value.at("name").get<std::string>();
    const auto group = value.at("group").get<std::string>();
    if (group != "unknown" && group != "stick" && group != "throttle") throw std::runtime_error("Invalid control group");
    control.group = group == "stick" ? InputGroup::Stick : group == "throttle" ? InputGroup::Throttle : InputGroup::Unknown;
    control.neutral = value.at("neutral").get<double>();
    control.physicalId = value.value("physical_id", "");
    control.part = value.value("part", "");
    control.status = value.value("status", "OBSERVED");
    if (control.status != "OBSERVED" && control.status != "USER_ASSIGNED") throw std::runtime_error("Invalid assignment evidence status");
    if (!control.physicalId.empty()) {
        const auto* physical = FindPhysicalControl(control.physicalId);
        if (!physical || physical->group != control.group || PhysicalName(*physical, control.part) != control.name ||
            (physical->parts.empty() ? !control.part.empty() : std::find(physical->parts.begin(), physical->parts.end(), control.part) == physical->parts.end()))
            throw std::runtime_error("Unknown or inconsistent physical-control selection");
    }
    if (control.name.empty() || control.name.size() > 128 || !std::isfinite(control.neutral) ||
        control.neutral < -1 || control.neutral > 1) throw std::runtime_error("Control name or neutral value out of bounds");
    return control;
}
}
Assignments LoadAssignments(const std::filesystem::path& path)
{
    Assignments assignments;
    const auto document = Read(path);
    for (const auto& [id, value] : document.at("controls").items()) assignments[id] = Parse(value);
    return assignments;
}
void SaveAssignment(const std::filesystem::path& path, const std::string& id,
    const LearnedControl& control, const Json& evidence)
{
    auto json = Read(path);
    Json value{{"name", control.name}, {"group", control.group == InputGroup::Stick ? "stick" :
        control.group == InputGroup::Throttle ? "throttle" : "unknown"}, {"neutral", control.neutral},
        {"status", control.status}, {"physical_id", control.physicalId}, {"part", control.part}, {"evidence", evidence}};
    (void)Parse(value);
    if (id.empty() || id.size() > 128) throw std::runtime_error("Invalid HID control identifier");
    if (!control.physicalId.empty()) for (const auto& [otherId, otherValue] : json.at("controls").items()) {
        if (otherId != id && otherValue.value("physical_id", "") == control.physicalId && otherValue.value("part", "") == control.part)
            throw std::runtime_error("This physical control is already linked to " + otherId + ". Clear that link first.");
    }
    json["controls"][id] = value;
    WriteJson(path, json);
}
void RemoveAssignment(const std::filesystem::path& path, const std::string& id)
{
    auto json = Read(path);
    json["controls"].erase(id);
    WriteJson(path, json);
}
}
