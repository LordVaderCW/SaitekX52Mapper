#pragma once
#include "../util/Windows.hpp"
#include "../diagnostics/Journal.hpp"
#include <optional>
#include <vector>

namespace x52 {
inline constexpr wchar_t EnhancedPowerValue[] = L"EnhancedPowerManagementEnabled";
struct X52PowerDevice {
    std::wstring instance, registryPath;
    std::optional<DWORD> value;
};
bool IsX52UsbInstance(std::wstring_view instance);
std::wstring X52PowerRegistryPath(std::wstring_view instance);
std::optional<DWORD> ReadEnhancedPowerValue(HKEY key);
void StoreEnhancedPowerValue(HKEY key, std::optional<DWORD> value);
std::vector<X52PowerDevice> ReadX52PowerDevices();
std::filesystem::path PowerBackupPath(const std::filesystem::path& directory, std::wstring_view instance);
Json PowerBackup(std::wstring_view instance, std::optional<DWORD> value);
std::optional<DWORD> PowerBackupValue(const Json& backup, std::wstring_view instance);
void ChangeX52Power(const std::filesystem::path& directory, std::wstring_view instance, bool restore);
}
