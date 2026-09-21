#pragma once
#include "State.hpp"
#include <string_view>
#include <optional>

namespace x52 {
enum class PhysicalKind { Axis, Button, Hat, Switch, Mouse };
struct PhysicalControl {
    std::string id, name, location;
    InputGroup group{};
    PhysicalKind kind{};
    std::vector<std::string> parts; // physical positions; never assumed HID values
};
const std::vector<PhysicalControl>& PhysicalControls();
struct PhysicalChoice {
    const PhysicalControl* control{}; // borrowed from the static catalog
    std::string name;
    std::string fixedPart; // empty means use the control's normal part selector
};
std::vector<PhysicalChoice> PhysicalChoices(InputGroup group, std::optional<PhysicalKind> kind = {});
const PhysicalControl* FindPhysicalControl(std::string_view id);
bool CanLink(const PhysicalControl& physical, std::string_view part, ControlKind hidKind);
std::string PhysicalName(const PhysicalControl& physical, std::string_view part);
}
