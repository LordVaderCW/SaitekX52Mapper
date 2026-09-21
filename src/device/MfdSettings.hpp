#pragma once
#include "HidEnumerator.hpp"

namespace x52 {
enum class MfdOption { Clutch, Latched, MfdBrightness, LedBrightness, Clock1, Clock2, Clock3 };
struct MfdSettings {
    bool clutch{}, latched{};
    DWORD mfdBrightness{}, ledBrightness{};
    std::array<bool, 3> twelveHour{};
};
// Original X52 only. Commands traced from the installed Logitech 8.0.116.0 CPL.
// This is a Windows driver interface, not USB feature reports or onboard profiles.
MfdSettings ReadMfdSettings(const std::wstring& path);
MfdSettings SetMfdOption(const std::wstring& path, MfdOption option, DWORD value);
}
