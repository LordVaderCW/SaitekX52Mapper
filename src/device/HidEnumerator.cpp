#include "HidEnumerator.hpp"
#include <setupapi.h>
#include <cstddef>
#include <sstream>

namespace x52 {
namespace {
class DeviceInfoSet final {
public:
    explicit DeviceInfoSet(const GUID& guid)
        : value_(SetupDiGetClassDevsW(&guid, nullptr, nullptr,
            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE))
    {
        if (value_ == INVALID_HANDLE_VALUE)
            throw WindowsException("SetupDiGetClassDevsW", GetLastError());
    }
    ~DeviceInfoSet() noexcept { SetupDiDestroyDeviceInfoList(value_); }
    DeviceInfoSet(const DeviceInfoSet&) = delete;
    DeviceInfoSet& operator=(const DeviceInfoSet&) = delete;
    [[nodiscard]] HDEVINFO get() const noexcept { return value_; }
private:
    HDEVINFO value_;
};

class PreparsedData final {
public:
    explicit PreparsedData(HANDLE device)
    {
        if (!HidD_GetPreparsedData(device, &value_))
            throw WindowsException("HidD_GetPreparsedData", GetLastError());
    }
    ~PreparsedData() noexcept { if (value_) HidD_FreePreparsedData(value_); }
    PreparsedData(const PreparsedData&) = delete;
    PreparsedData& operator=(const PreparsedData&) = delete;
    [[nodiscard]] PHIDP_PREPARSED_DATA get() const noexcept { return value_; }
private:
    PHIDP_PREPARSED_DATA value_{};
};

std::wstring ParserError(const wchar_t* operation, NTSTATUS status)
{
    std::wostringstream text;
    text << operation << L" failed. HID parser status 0x" << std::hex <<
        static_cast<ULONG>(status) << L".";
    return text.str();
}

void ReadMetadata(HidDeviceInfo& info)
{
    UniqueHandle device(CreateFileW(info.path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, 0, nullptr));
    if (!device.valid()) {
        info.errors.push_back(WindowsError(L"CreateFileW (metadata)", GetLastError()));
        return;
    }
    HIDD_ATTRIBUTES attributes{};
    attributes.Size = sizeof(attributes);
    if (HidD_GetAttributes(device.get(), &attributes)) info.attributes = attributes;
    else info.errors.push_back(WindowsError(L"HidD_GetAttributes", GetLastError()));

    std::array<wchar_t, 256> text{};
    if (HidD_GetManufacturerString(device.get(), text.data(), static_cast<ULONG>(sizeof(text)))) {
        text.back() = L'\0';
        info.manufacturer = text.data();
    } else info.errors.push_back(WindowsError(L"HidD_GetManufacturerString", GetLastError()));
    text.fill(L'\0');
    if (HidD_GetProductString(device.get(), text.data(), static_cast<ULONG>(sizeof(text)))) {
        text.back() = L'\0';
        info.product = text.data();
    } else info.errors.push_back(WindowsError(L"HidD_GetProductString", GetLastError()));

    try {
        PreparsedData data(device.get());
        HIDP_CAPS caps{};
        const auto status = HidP_GetCaps(data.get(), &caps);
        if (status != HIDP_STATUS_SUCCESS) {
            info.errors.push_back(ParserError(L"HidP_GetCaps", status));
            return;
        }
        info.capabilities = caps;
        if (caps.NumberInputValueCaps != 0) {
            auto count = caps.NumberInputValueCaps;
            info.values.resize(count);
            const auto result = HidP_GetValueCaps(HidP_Input, info.values.data(), &count, data.get());
            if (result == HIDP_STATUS_SUCCESS) info.values.resize(count);
            else {
                info.values.clear();
                info.errors.push_back(ParserError(L"HidP_GetValueCaps", result));
            }
        }
        if (caps.NumberInputButtonCaps != 0) {
            auto count = caps.NumberInputButtonCaps;
            info.buttons.resize(count);
            const auto result = HidP_GetButtonCaps(HidP_Input, info.buttons.data(), &count, data.get());
            if (result == HIDP_STATUS_SUCCESS) info.buttons.resize(count);
            else {
                info.buttons.clear();
                info.errors.push_back(ParserError(L"HidP_GetButtonCaps", result));
            }
        }
    } catch (const std::system_error& error) {
        info.errors.push_back(WindowsError(L"HidD_GetPreparsedData",
            static_cast<DWORD>(error.code().value())));
    }
}
}

std::vector<HidDeviceInfo> EnumerateHid(std::stop_token stop)
{
    GUID guid{};
    HidD_GetHidGuid(&guid);
    DeviceInfoSet devices(guid);
    std::vector<HidDeviceInfo> results;
    for (DWORD index = 0; !stop.stop_requested(); ++index) {
        SP_DEVICE_INTERFACE_DATA interfaceData{};
        interfaceData.cbSize = sizeof(interfaceData);
        if (!SetupDiEnumDeviceInterfaces(devices.get(), nullptr, &guid, index, &interfaceData)) {
            const auto error = GetLastError();
            if (error == ERROR_NO_MORE_ITEMS) break;
            throw WindowsException("SetupDiEnumDeviceInterfaces", error);
        }
        DWORD required{};
        const auto sizeResult = SetupDiGetDeviceInterfaceDetailW(
            devices.get(), &interfaceData, nullptr, 0, &required, nullptr);
        const auto sizeError = sizeResult ? ERROR_SUCCESS : GetLastError();
        HidDeviceInfo info;
        if (sizeError != ERROR_INSUFFICIENT_BUFFER ||
            required < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) {
            info.errors.push_back(WindowsError(L"SetupDiGetDeviceInterfaceDetailW (size)",
                sizeError == ERROR_SUCCESS ? ERROR_INVALID_DATA : sizeError));
            results.push_back(std::move(info));
            continue;
        }
        // Explicit alignment for the variable-length Windows structure.
        std::vector<std::max_align_t> storage(
            (required + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(devices.get(), &interfaceData, detail,
            required, nullptr, nullptr)) {
            info.errors.push_back(WindowsError(L"SetupDiGetDeviceInterfaceDetailW", GetLastError()));
        } else {
            info.path = detail->DevicePath;
            ReadMetadata(info);
        }
        results.push_back(std::move(info));
    }
    return results;
}
}
