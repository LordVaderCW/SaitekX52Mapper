#pragma once
#include "MfdSettings.hpp"
#include "../profiles/BattlefieldProfiles.hpp"

namespace x52 {
inline constexpr std::array<DWORD, 9> DeadzoneAxes{0x10030,0x10031,0x10035,0x10032,0x10033,0x10034,0x10036,0x50024,0x50026};
inline constexpr std::array<const wchar_t*, 9> DeadzoneNames{L"X Axis / Stick left-right",L"Y Axis / Stick forward-back",L"Z Rotation / Rudder",L"Z Axis / Throttle",L"X Rotation / Side rotary",L"Y Rotation / Top rotary",L"Slider",L"Mouse X",L"Mouse Y"};
struct AxisDeadzone {
    // Positions in the vendor's 0..65535 envelope, not HID raw counts.
    std::array<int, 4> limits{0,32768,32768,65535};
    bool operator==(const AxisDeadzone&) const = default;
};
struct DeadzoneSettings {
    std::wstring devicePath;
    std::filesystem::path file;
    std::string original;
    std::array<AxisDeadzone, 9> axes;
};
void ValidateDeadzone(const AxisDeadzone& axis);
std::array<AxisDeadzone,9> DecodeDeadzones(const Pr0Node& root);
Pr0Node UpdateDeadzones(Pr0Node root, const std::array<AxisDeadzone,9>& axes);
DeadzoneSettings ReadDeadzones(const std::wstring& devicePath);
DeadzoneSettings ApplyDeadzones(const DeadzoneSettings& desired, const std::filesystem::path& dataDirectory);
// Reapply the current saved envelope without changing its centre or limits.
// This is not a firmware reset or a proven fix for a displaced physical centre.
DeadzoneSettings ReloadSavedDeadzones(const DeadzoneSettings& expected, const std::filesystem::path& dataDirectory);
}
