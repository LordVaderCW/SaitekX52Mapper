#include "PhysicalControls.hpp"

namespace x52 {
const std::vector<PhysicalControl>& PhysicalControls()
{
    // Physical inventory from the manufacturer's non-Pro X52 guide, pp. 3-4.
    // Catalog IDs identify hardware features, NOT Windows button numbers.
    static const std::vector<PhysicalControl> catalog = [] {
        std::vector<PhysicalControl> out;
        const auto add = [&](const char* id, const char* name, InputGroup group, PhysicalKind kind,
            const char* location, std::vector<std::string> parts = {}) {
            out.push_back({id, name, location, group, kind, std::move(parts)});
        };
        const auto stick = InputGroup::Stick, throttle = InputGroup::Throttle;
        add("stick.x", "Stick X - left / right", stick, PhysicalKind::Axis, "Move the main grip left and right without twisting it.");
        add("stick.y", "Stick Y - forward / back", stick, PhysicalKind::Axis, "Move the main grip forward and back.");
        add("stick.twist", "Rudder - grip twist", stick, PhysicalKind::Axis, "Rotate the grip about its vertical axis; release the mechanical rudder lock first if needed.");
        add("stick.trigger1", "Trigger - stage 1", stick, PhysicalKind::Button, "Front metal trigger: light pull to the first stage.");
        add("stick.trigger2", "Trigger - stage 2", stick, PhysicalKind::Button, "Front metal trigger: full pull through the second stage. Stage 1 may also respond.");
        add("stick.fire", "Fire - under SAFE cover", stick, PhysicalKind::Button, "Open the safety cover on the top of the grip and press the button beneath it.");
        add("stick.a", "Button A", stick, PhysicalKind::Button, "Press the illuminated grip button labelled A; inspect the marking on your unit.");
        add("stick.b", "Button B", stick, PhysicalKind::Button, "Press the illuminated grip button labelled B; inspect the marking on your unit.");
        add("stick.c", "Button C", stick, PhysicalKind::Button, "Press the illuminated grip button labelled C; inspect the marking on your unit.");
        add("stick.pinkie", "Pinkie switch", stick, PhysicalKind::Button, "Metal lever near the lower front of the grip, above the hand rest.");
        const std::vector<std::string> hatParts{"Whole hat", "Up", "Up-right", "Right", "Down-right", "Down", "Down-left", "Left", "Up-left"};
        add("stick.hat_upper", "Upper grip hat", stick, PhysicalKind::Hat, "Upper hat on the grip. Location names avoid assuming a Windows POV/button number.", hatParts);
        add("stick.hat_lower", "Lower grip hat", stick, PhysicalKind::Hat, "Lower hat on the face of the grip, below the top controls.", hatParts);
        add("stick.mode", "Mode selector", stick, PhysicalKind::Switch, "Three-position mode selector at the top of the grip.", {"Whole selector", "Mode 1", "Mode 2", "Mode 3"});
        add("stick.toggle12", "Base toggle T1 / T2", stick, PhysicalKind::Switch, "First spring-loaded rocker on the stick base. Use the printed T1/T2 labels.", {"T1", "T2"});
        add("stick.toggle34", "Base toggle T3 / T4", stick, PhysicalKind::Switch, "Middle spring-loaded rocker on the stick base.", {"T3", "T4"});
        add("stick.toggle56", "Base toggle T5 / T6", stick, PhysicalKind::Switch, "Third spring-loaded rocker on the stick base.", {"T5", "T6"});
        add("throttle.main", "Main throttle lever", throttle, PhysicalKind::Axis, "Move the entire throttle handle forward and back.");
        add("throttle.rotary_top", "Rotary - top of throttle", throttle, PhysicalKind::Axis, "Turn the rotary on top of the throttle handle. This is a location name, not an assumed HID axis.");
        add("throttle.rotary_side", "Rotary - side of throttle", throttle, PhysicalKind::Axis, "Turn the side rotary on the throttle handle, near the thumb slider.");
        add("throttle.slider", "Precision thumb slider", throttle, PhysicalKind::Axis, "Small sliding control on the side of the throttle handle.");
        add("throttle.d", "Button D", throttle, PhysicalKind::Button, "Throttle fire button labelled D. Confirm the printed marking on your unit.");
        add("throttle.e", "Button E", throttle, PhysicalKind::Button, "Throttle fire button labelled E. Confirm the printed marking on your unit.");
        add("throttle.clutch", "Clutch button I", throttle, PhysicalKind::Button, "Clutch button marked I. Saitek software can reserve this control; check observed HID activity.");
        add("throttle.hat", "Throttle hat", throttle, PhysicalKind::Hat, "Eight-way hat on the throttle. Move one direction at a time.", hatParts);
        add("throttle.mouse_x", "Mouse mini-stick - horizontal", throttle, PhysicalKind::Mouse, "Small mouse controller on the throttle. May be exposed through a different collection/driver.");
        add("throttle.mouse_y", "Mouse mini-stick - vertical", throttle, PhysicalKind::Mouse, "Small mouse controller on the throttle. May be exposed through a different collection/driver.");
        add("throttle.mouse_button", "Mouse button", throttle, PhysicalKind::Button, "Mouse button near the throttle mouse controller. Availability in this joystick collection is unverified.");
        add("throttle.scroll", "Throttle scroll wheel", throttle, PhysicalKind::Mouse, "Rear/index-finger scroll wheel: roll up/down, or press inward to click (right mouse button). Link only the HID input you observe responding.", {"Wheel up", "Wheel down", "Wheel press", "Whole wheel"});
        add("throttle.mfd_function", "MFD Function button", throttle, PhysicalKind::Button, "Function button below the display. May be handled locally rather than exposed in this HID collection.");
        add("throttle.mfd_start", "MFD Start / Stop button", throttle, PhysicalKind::Button, "Start/Stop below the display. Do not assume this produces a joystick report.");
        add("throttle.mfd_reset", "MFD Reset button", throttle, PhysicalKind::Button, "Reset below the display. This names a display button, not a hardware-recovery command.");
        return out;
    }();
    return catalog;
}
std::vector<PhysicalChoice> PhysicalChoices(InputGroup group, std::optional<PhysicalKind> kind)
{
    std::vector<PhysicalChoice> choices;
    for (const auto& control : PhysicalControls()) {
        if (group != InputGroup::Unknown && control.group != group) continue;
        if (control.id == "throttle.scroll") {
            if (kind && *kind != PhysicalKind::Button && *kind != PhysicalKind::Mouse) continue;
            // Present individual button actions while retaining the saved catalog ID/parts.
            // The same action in either filter must have the same duplicate-link identity.
            choices.push_back({&control, "Scroll wheel up", "Wheel up"});
            choices.push_back({&control, "Scroll wheel down", "Wheel down"});
            choices.push_back({&control, "Scroll wheel click / right mouse button (RMB)", "Wheel press"});
            if (!kind || *kind == PhysicalKind::Mouse)
                choices.push_back({&control, "Scroll wheel - whole wheel value", "Whole wheel"});
        } else if (!kind || control.kind == *kind) {
            choices.push_back({&control, control.name, {}});
        }
    }
    return choices;
}
const PhysicalControl* FindPhysicalControl(std::string_view id)
{
    for (const auto& control : PhysicalControls()) if (control.id == id) return &control;
    return nullptr;
}
bool CanLink(const PhysicalControl& physical, std::string_view part, ControlKind hidKind)
{
    if (physical.parts.empty()) {
        if (!part.empty()) return false;
        return physical.kind == PhysicalKind::Button ? hidKind == ControlKind::Button : hidKind == ControlKind::Axis;
    }
    if (std::find(physical.parts.begin(), physical.parts.end(), part) == physical.parts.end()) return false;
    if (part.starts_with("Whole")) return hidKind != ControlKind::Button;
    return hidKind == ControlKind::Button;
}
std::string PhysicalName(const PhysicalControl& physical, std::string_view part)
{
    return physical.name + (part.empty() ? "" : " - " + std::string(part));
}
}
