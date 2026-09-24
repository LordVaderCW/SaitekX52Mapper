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
    DWORD Io(DWORD code, void* input, DWORD inSize, void* output, DWORD outSize, bool exact = true)
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
        if (exact && outSize && count != outSize) throw std::runtime_error("X52 settings returned an incomplete response");
        return count;
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
        result.dateFormat=Get(0x223624);
        const auto daylight=Get(0x223658);
        if(result.dateFormat>2 || daylight>1) throw std::runtime_error("Unsupported X52 date/daylight setting");
        result.daylight=daylight!=0;
        for(int i=0;i<2;++i){std::array<LONG,2> zone{i+1,0};Io(0x22361c,zone.data(),sizeof(zone),zone.data(),sizeof(zone));
            if(zone[0]!=i+1 || std::find(X52TimeZones.begin(),X52TimeZones.end(),zone[1])==X52TimeZones.end()) throw std::runtime_error("Unsupported X52 time zone");
            result.zoneMinutes[static_cast<std::size_t>(i)]=zone[1];}
        return result;
    }
};
}
MfdSettings ReadMfdSettings(const std::wstring& path) { return SettingsDevice(path).Read(); }
MfdSettings SetMfdOption(const std::wstring& path, MfdOption option, DWORD value)
{
    const bool brightness = option == MfdOption::MfdBrightness || option == MfdOption::LedBrightness;
    if (value > (brightness ? 100u : option==MfdOption::DateFormat?2u : (option==MfdOption::Zone2 || option==MfdOption::Zone3)?36u : 1u)) throw std::runtime_error("Invalid X52 setting value");
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
    case MfdOption::DateFormat: device.Set(0x223628,value); break;
    case MfdOption::Daylight: device.Set(0x22365c,value); break;
    case MfdOption::Zone2: case MfdOption::Zone3: {
        std::array<LONG,2> zone{option==MfdOption::Zone2?1:2,X52TimeZones[value]};
        device.Io(0x223620,zone.data(),sizeof(zone),nullptr,0);break;
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
    case MfdOption::DateFormat: actual=result.dateFormat;break;
    case MfdOption::Daylight: actual=result.daylight;break;
    case MfdOption::Zone2: case MfdOption::Zone3: actual=static_cast<DWORD>(std::find(X52TimeZones.begin(),X52TimeZones.end(),result.zoneMinutes[option==MfdOption::Zone2?0:1])-X52TimeZones.begin());break;
    default: actual = result.twelveHour[static_cast<std::size_t>(option) - static_cast<std::size_t>(MfdOption::Clock1)]; break;
    }
    if (actual != value) throw std::runtime_error("X52 did not retain the requested setting; refresh its current settings");
    return result;
}
std::filesystem::path ReadX52CalibrationPath(const std::wstring& path)
{
    SettingsDevice device(path);
    std::array<DWORD, 2> input{};
    std::array<wchar_t, 512> output{};
    const auto bytes = device.Io(0x222804, input.data(), sizeof(input), output.data(), sizeof(output), false);
    // CPL 0x10DC0: DWORD length, WORD path kind (0 = absolute), UTF-16 path.
    if (bytes < 8 || bytes > sizeof(output) || bytes % 2 || output[2] != 0)
        throw std::runtime_error("Unsupported X52 calibration path response");
    const auto end = std::find(output.begin() + 3, output.begin() + bytes / 2, L'\0');
    if (end == output.begin() + bytes / 2) throw std::runtime_error("Unterminated X52 calibration path");
    return std::filesystem::path(std::wstring(output.begin() + 3, end));
}
void ReloadX52Calibration(const std::wstring& path, const std::filesystem::path& calibration)
{
    if (ReadX52CalibrationPath(path) != calibration) throw std::runtime_error("X52 calibration changed; refresh before applying");
    const auto name = calibration.wstring();
    if (name.size() > 260) throw std::runtime_error("Calibration path is too long");
    SettingsDevice device(path);
    // CPL 0x10BE0: zeroed 10-byte header followed by NUL-terminated UTF-16 path.
    std::vector<BYTE> packet(10 + (name.size() + 1) * sizeof(wchar_t));
    memcpy(packet.data() + 10, name.c_str(), (name.size() + 1) * sizeof(wchar_t));
    std::array<DWORD, 2> request{};
    device.Io(0x222800, packet.data(), static_cast<DWORD>(packet.size()), &request[1], sizeof(DWORD));
    DWORD result{};
    device.Io(0x22280c, request.data(), sizeof(request), &result, sizeof(result));
    if (ReadX52CalibrationPath(path) != calibration) throw std::runtime_error("X52 calibration path readback mismatch");
}

}
