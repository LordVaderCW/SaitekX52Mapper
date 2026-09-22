#include "PowerManagement.hpp"
#include "../util/Text.hpp"
#include <setupapi.h>
#include <algorithm>

namespace x52 {
namespace {
constexpr std::wstring_view Prefix = L"USB\\VID_06A3&PID_075C\\";
struct RegistryKey {
    HKEY value{};
    ~RegistryKey() { if (value) RegCloseKey(value); }
};
struct DeviceSet {
    HDEVINFO value = SetupDiGetClassDevsW(nullptr, L"USB", nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    ~DeviceSet() { if (value != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(value); }
};
Json ReadBackup(const std::filesystem::path& path)
{
    if (std::filesystem::file_size(path) > 8192) throw std::runtime_error("Power setting backup exceeds 8 KiB");
    std::ifstream stream(path);
    if (!stream) throw std::runtime_error("Cannot read the original power setting backup");
    return Json::parse(stream, [](int depth, Json::parse_event_t, Json&) {
        if (depth > 8) throw std::runtime_error("Invalid power setting backup nesting");
        return true;
    });
}
}
bool IsX52UsbInstance(std::wstring_view instance)
{
    if (!instance.starts_with(Prefix) || instance.size() <= Prefix.size() || instance.size() > 180) return false;
    return std::all_of(instance.begin() + Prefix.size(), instance.end(), [](wchar_t ch) {
        return (ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'Z') || ch == L'&' || ch == L'_' || ch == L'-';
    });
}
std::wstring X52PowerRegistryPath(std::wstring_view instance)
{
    if (!IsX52UsbInstance(instance)) throw std::runtime_error("Power setting is restricted to the original X52 USB instance (06A3:075C)");
    return L"SYSTEM\\CurrentControlSet\\Enum\\" + std::wstring(instance) + L"\\Device Parameters";
}
std::optional<DWORD> ReadEnhancedPowerValue(HKEY key)
{
    DWORD value{}, type{}, bytes = sizeof(value);
    const auto result = RegQueryValueExW(key, EnhancedPowerValue, nullptr, &type, reinterpret_cast<BYTE*>(&value), &bytes);
    if (result == ERROR_FILE_NOT_FOUND) return {};
    if (result != ERROR_SUCCESS) throw WindowsException("Read X52 enhanced power setting", result);
    if (type != REG_DWORD || bytes != sizeof(DWORD) || value > 1)
        throw std::runtime_error("X52 enhanced power value is not a supported DWORD 0/1; no change made");
    return value;
}
void StoreEnhancedPowerValue(HKEY key, std::optional<DWORD> value)
{
    if (value && *value > 1) throw std::runtime_error("Power setting must be DWORD 0 or 1");
    auto result = value ? RegSetValueExW(key, EnhancedPowerValue, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&*value), sizeof(DWORD)) :
        RegDeleteValueW(key, EnhancedPowerValue);
    if (!value && result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
    if (result != ERROR_SUCCESS) throw WindowsException("Write X52 enhanced power setting (administrator access required)", result);
    if (ReadEnhancedPowerValue(key) != value) throw std::runtime_error("X52 power setting readback did not match the requested value");
}
std::vector<X52PowerDevice> ReadX52PowerDevices()
{
    DeviceSet devices;
    if (devices.value == INVALID_HANDLE_VALUE) throw WindowsException("Enumerate USB power settings", GetLastError());
    std::vector<X52PowerDevice> result;
    for (DWORD index = 0;; ++index) {
        SP_DEVINFO_DATA info{sizeof(info)};
        if (!SetupDiEnumDeviceInfo(devices.value, index, &info)) {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) break;
            throw WindowsException("Read USB device instance", GetLastError());
        }
        wchar_t instance[512]{};
        if (!SetupDiGetDeviceInstanceIdW(devices.value, &info, instance, 512, nullptr))
            throw WindowsException("Read USB instance ID", GetLastError());
        if (!IsX52UsbInstance(instance)) continue;
        const auto path = X52PowerRegistryPath(instance);
        RegistryKey key;
        const auto opened = RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_QUERY_VALUE, &key.value);
        if (opened != ERROR_SUCCESS) throw WindowsException("Open X52 device power settings", opened);
        result.push_back({instance, path, ReadEnhancedPowerValue(key.value)});
    }
    return result;
}
std::filesystem::path PowerBackupPath(const std::filesystem::path& directory, std::wstring_view instance)
{
    (void)X52PowerRegistryPath(instance);
    return directory / L"registry" / (L"x52-power-" + std::wstring(instance.substr(Prefix.size())) + L".json");
}
Json PowerBackup(std::wstring_view instance, std::optional<DWORD> value)
{
    (void)X52PowerRegistryPath(instance);
    if (value && *value > 1) throw std::runtime_error("Unsupported original power value");
    return {{"schema", 1}, {"instance", Utf8(std::wstring(instance))}, {"value_name", "EnhancedPowerManagementEnabled"},
        {"original_present", value.has_value()}, {"original_value", value ? Json(*value) : Json(nullptr)}, {"utc", UtcNow()}};
}
std::optional<DWORD> PowerBackupValue(const Json& backup, std::wstring_view instance)
{
    (void)X52PowerRegistryPath(instance);
    if (backup.at("schema") != 1 || backup.at("instance") != Utf8(std::wstring(instance)) ||
        backup.at("value_name") != "EnhancedPowerManagementEnabled") throw std::runtime_error("Power backup does not match this X52 instance");
    if (!backup.at("original_present").get<bool>()) {
        if (!backup.at("original_value").is_null()) throw std::runtime_error("Invalid absent power value backup");
        return {};
    }
    const auto& original = backup.at("original_value");
    if (!original.is_number_integer() || (original != 0 && original != 1)) throw std::runtime_error("Invalid original power value backup");
    return original.get<DWORD>();
}
void ChangeX52Power(const std::filesystem::path& directory, std::wstring_view instance, bool restore)
{
    const auto path = X52PowerRegistryPath(instance);
    const auto devices = ReadX52PowerDevices();
    if (std::none_of(devices.begin(), devices.end(), [&](const auto& device) { return device.instance == instance; }))
        throw std::runtime_error("The selected X52 USB instance is no longer connected; refresh the device list");
    RegistryKey key;
    const auto opened = RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key.value);
    if (opened != ERROR_SUCCESS) throw WindowsException("Open X52 power setting for writing (administrator access required)", opened);
    const auto previous = ReadEnhancedPowerValue(key.value);
    const auto backup = PowerBackupPath(directory, instance);
    std::optional<DWORD> desired = 0;
    if (restore) desired = PowerBackupValue(ReadBackup(backup), instance);
    else {
        std::filesystem::create_directories(backup.parent_path());
        // CREATE_NEW preserves the original across repeated applications and
        // concurrent helper processes. Never overwrite an existing backup.
        UniqueHandle file(CreateFileW(backup.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (file.valid()) {
            const auto text = PowerBackup(instance, previous).dump(2) + "\n";
            DWORD written{};
            if (!WriteFile(file.get(), text.data(), static_cast<DWORD>(text.size()), &written, nullptr) || written != text.size() || !FlushFileBuffers(file.get()))
                throw WindowsException("Save original X52 power setting before changing it", GetLastError());
        } else if (GetLastError() != ERROR_FILE_EXISTS) throw WindowsException("Create original X52 power backup", GetLastError());
        else (void)PowerBackupValue(ReadBackup(backup), instance);
    }
    StoreEnhancedPowerValue(key.value, desired);
}
}
