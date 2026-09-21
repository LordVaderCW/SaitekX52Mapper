#include "MfdSettings.hpp"
#include <setupapi.h>
#include <initguid.h>
#include <devpkey.h>

namespace x52 {
namespace {
class SettingsDevice final {
    UniqueHandle handle_;
public:
    explicit SettingsDevice(const std::wstring& path)
        : handle_(CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr))
    {
        if (!handle_.valid()) throw WindowsException("Open X52 settings", GetLastError());
        HIDD_ATTRIBUTES attributes{}; attributes.Size = sizeof(attributes);
        if (!HidD_GetAttributes(handle_.get(), &attributes) || attributes.VendorID != SAITEK_VENDOR_ID || attributes.ProductID != X52_PRODUCT_ID)
            throw std::runtime_error("Settings require the original X52 (06A3:075C)");
        struct Info {
            HDEVINFO value{SetupDiCreateDeviceInfoList(nullptr, nullptr)};
            ~Info() { if (value != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(value); }
        } info;
        if (info.value == INVALID_HANDLE_VALUE) throw WindowsException("Settings device information", GetLastError());
        SP_DEVICE_INTERFACE_DATA iface{}; iface.cbSize = sizeof(iface);
        if (!SetupDiOpenDeviceInterfaceW(info.value, path.c_str(), 0, &iface))
            throw WindowsException("Find settings driver", GetLastError());
        SP_DEVINFO_DATA device{}; device.cbSize = sizeof(device);
        DWORD needed{};
        SetupDiGetDeviceInterfaceDetailW(info.value, &iface, nullptr, 0, &needed, &device);
        std::array<wchar_t, 128> version{};
        DEVPROPTYPE type{};
        if (!SetupDiGetDevicePropertyW(info.value, &device, &DEVPKEY_Device_DriverVersion,
            &type, reinterpret_cast<PBYTE>(version.data()), sizeof(version), nullptr, 0) ||
            type != DEVPROP_TYPE_STRING || std::wstring_view(version.data()) != L"8.0.116.0")
            throw std::runtime_error("MFD settings support Logitech X52 driver 8.0.116.0; this driver has not been validated");
    }
    void Io(DWORD code, void* input, DWORD inSize, void* output, DWORD outSize)
    {
        UniqueHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (!event.valid()) throw WindowsException("Settings event", GetLastError());
        OVERLAPPED operation{}; operation.hEvent = event.get();
        DWORD count{};
        if (!DeviceIoControl(handle_.get(), code, input, inSize, output, outSize, &count, &operation)) {
            const auto error = GetLastError();
            if (error != ERROR_IO_PENDING) throw WindowsException("X52 settings command", error);
            if (WaitForSingleObject(event.get(), 1000) != WAIT_OBJECT_0) {
                CancelIoEx(handle_.get(), &operation);
                GetOverlappedResult(handle_.get(), &operation, &count, TRUE); // drain before releasing buffers
                throw WindowsException("X52 settings timeout", ERROR_TIMEOUT);
            }
            if (!GetOverlappedResult(handle_.get(), &operation, &count, FALSE))
                throw WindowsException("X52 settings completion", GetLastError());
        }
        if (outSize && count != outSize) throw std::runtime_error("X52 settings returned an incomplete response");
    }
    DWORD Get(DWORD code) { DWORD value{}; Io(code, nullptr, 0, &value, sizeof(value)); return value; }
    void Set(DWORD code, DWORD value) { Io(code, &value, sizeof(value), nullptr, 0); }
    MfdSettings Read()
    {
        MfdSettings result;
        const auto clutch = Get(0x223604), latched = Get(0x22360c);
        if ((clutch != 0 && clutch != 0x9001e) || latched > 1)
            throw std::runtime_error("Unrecognized X52 clutch configuration");
        result.clutch = clutch != 0; result.latched = latched != 0;
        result.mfdBrightness = Get(0x223614);
        DWORD ledIndex{};
        Io(0x222004, &ledIndex, sizeof(ledIndex), &result.ledBrightness, sizeof(DWORD));
        if (result.mfdBrightness > 100 || result.ledBrightness > 100)
            throw std::runtime_error("Unrecognized X52 brightness range");
        for (DWORD i = 0; i < 3; ++i) {
            std::array<DWORD, 2> clock{i, 0};
            Io(0x22362c, clock.data(), sizeof(clock), clock.data(), sizeof(clock));
            if (clock[0] != i || clock[1] > 1) throw std::runtime_error("Unrecognized X52 clock format");
            result.twelveHour[i] = clock[1] != 0;
        }
        return result;
    }
};
}
MfdSettings ReadMfdSettings(const std::wstring& path) { return SettingsDevice(path).Read(); }
MfdSettings SetMfdOption(const std::wstring& path, MfdOption option, DWORD value)
{
    const bool brightness = option == MfdOption::MfdBrightness || option == MfdOption::LedBrightness;
    if (value > (brightness ? 100u : 1u)) throw std::runtime_error("Invalid X52 setting value");
    SettingsDevice device(path);
    (void)device.Read(); // all required queries must succeed before any write
    switch (option) {
    case MfdOption::Clutch: device.Set(0x223608, value ? 0x9001e : 0); break;
    case MfdOption::Latched: device.Set(0x223610, value); break;
    case MfdOption::MfdBrightness: device.Set(0x223618, value); break;
    case MfdOption::LedBrightness: device.Set(0x222000, value << 16); break; // WORD LED index 0, WORD percent
    case MfdOption::Clock1: case MfdOption::Clock2: case MfdOption::Clock3: {
        std::array<DWORD, 2> clock{static_cast<DWORD>(option) - static_cast<DWORD>(MfdOption::Clock1), value};
        device.Io(0x223630, clock.data(), sizeof(clock), nullptr, 0); break;
    }
    default: throw std::runtime_error("Unknown X52 setting");
    }
    auto result = device.Read();
    DWORD actual{};
    switch (option) {
    case MfdOption::Clutch: actual = result.clutch; break;
    case MfdOption::Latched: actual = result.latched; break;
    case MfdOption::MfdBrightness: actual = result.mfdBrightness; break;
    case MfdOption::LedBrightness: actual = result.ledBrightness; break;
    default: actual = result.twelveHour[static_cast<std::size_t>(option) - static_cast<std::size_t>(MfdOption::Clock1)]; break;
    }
    if (actual != value) throw std::runtime_error("X52 did not retain the requested setting; refresh its current settings");
    return result;
}
}
