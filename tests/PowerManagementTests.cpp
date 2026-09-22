#include "device/PowerManagement.hpp"
#include <stdexcept>

namespace {
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct TestKey {
    std::wstring path = L"Software\\X52MapperPowerTest-" + std::to_wstring(GetCurrentProcessId());
    HKEY key{};
    ~TestKey() { if (key) { RegCloseKey(key); RegDeleteKeyW(HKEY_CURRENT_USER, path.c_str()); } }
};
}
void PowerManagementTests()
{
    using namespace x52;
    const std::wstring instance = L"USB\\VID_06A3&PID_075C\\6&1E5B6DD0&0&3";
    Check(IsX52UsbInstance(instance), "original X52 physical USB instance accepted");
    for (const auto invalid : {L"HID\\VID_06A3&PID_075C\\7&1234&0&0000", L"USB\\VID_06A3&PID_0762\\123",
        L"USB\\VID_06A3&PID_075C&MI_00\\123", L"USB\\VID_06A3&PID_075C\\", L"USB\\VID_06A3&PID_075C\\..\\Other",
        L"USB\\VID_06A3&PID_075C\\123\"", L"USB\\ROOT_HUB30\\123"}) {
        Check(!IsX52UsbInstance(invalid), "non-target, composite child and malformed registry paths rejected");
        bool rejected = false;
        try { (void)X52PowerRegistryPath(invalid); } catch (const std::exception&) { rejected = true; }
        Check(rejected, "registry path cannot escape device restriction");
    }
    for (const auto value : {std::optional<DWORD>{}, std::optional<DWORD>{0}, std::optional<DWORD>{1}})
        Check(PowerBackupValue(PowerBackup(instance, value), instance) == value, "restore preserves original zero, one and absent value");
    auto bad = PowerBackup(instance, 1); bad["original_value"] = -1;
    bool rejected = false;
    try { (void)PowerBackupValue(bad, instance); } catch (const std::exception&) { rejected = true; }
    Check(rejected, "negative backup value cannot wrap to DWORD");
    rejected = false;
    try { (void)PowerBackupValue(PowerBackup(instance, 1), L"USB\\VID_06A3&PID_075C\\DIFFERENT"); }
    catch (const std::exception&) { rejected = true; }
    Check(rejected, "backup is bound to its exact device instance");
    TestKey scratch;
    Check(RegCreateKeyExW(HKEY_CURRENT_USER, scratch.path.c_str(), 0, nullptr, 0, KEY_QUERY_VALUE | KEY_SET_VALUE,
        nullptr, &scratch.key, nullptr) == ERROR_SUCCESS, "isolated user-registry test key created");
    Check(!ReadEnhancedPowerValue(scratch.key), "absent value is not mistaken for enabled or disabled");
    StoreEnhancedPowerValue(scratch.key, 1);
    const auto original = PowerBackup(instance, ReadEnhancedPowerValue(scratch.key));
    StoreEnhancedPowerValue(scratch.key, 0);
    Check(ReadEnhancedPowerValue(scratch.key) == DWORD{0}, "disable writes DWORD zero and reads it back");
    StoreEnhancedPowerValue(scratch.key, PowerBackupValue(original, instance));
    Check(ReadEnhancedPowerValue(scratch.key) == DWORD{1}, "restore writes original DWORD one");
    StoreEnhancedPowerValue(scratch.key, {});
    Check(!ReadEnhancedPowerValue(scratch.key), "restoring originally absent value removes only the setting");
    const wchar_t text[] = L"1";
    Check(RegSetValueExW(scratch.key, EnhancedPowerValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(text), sizeof(text)) == ERROR_SUCCESS,
        "unsupported typed value created in isolated test key");
    rejected = false;
    try { (void)ReadEnhancedPowerValue(scratch.key); } catch (const std::exception&) { rejected = true; }
    Check(rejected, "non-DWORD power values rejected");
    RegDeleteValueW(scratch.key, EnhancedPowerValue);
}
