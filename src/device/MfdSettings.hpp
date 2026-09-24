#pragma once
#include "HidEnumerator.hpp"
#include <filesystem>

namespace x52 {
enum class MfdOption { Clutch, Latched, MfdBrightness, LedBrightness, Clock1, Clock2, Clock3, Zone2, Zone3, DateFormat, Daylight };
inline constexpr std::array<int,37> X52TimeZones{-720,-660,-600,-570,-540,-510,-480,-420,-360,-300,-240,-210,-180,-120,-60,0,60,120,180,210,240,270,300,330,360,390,420,480,540,570,600,630,660,690,720,780,840};
struct MfdSettings {
    bool clutch{}, latched{}, daylight{};
    DWORD dateFormat{};
    std::array<int,2> zoneMinutes{};
    DWORD mfdBrightness{}, ledBrightness{};
    std::array<bool, 3> twelveHour{};
};
// Original X52 only. Commands traced from the installed Logitech 8.0.116.0 CPL.
// This is a Windows driver interface, not USB feature reports or onboard profiles.
std::filesystem::path ReadX52CalibrationPath(const std::wstring& path);
void ReloadX52Calibration(const std::wstring& path, const std::filesystem::path& calibration);
MfdSettings ReadMfdSettings(const std::wstring& path);
MfdSettings SetMfdOption(const std::wstring& path, MfdOption option, DWORD value);
}
