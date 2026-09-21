#include "InventoryText.hpp"
#include <iomanip>
#include <sstream>

namespace x52 {
std::wstring InventoryText(std::span<const HidDeviceInfo> devices)
{
    std::wostringstream out;
    out << L"X52 Inspector - Windows HID inventory\r\n"
        L"Descriptor-derived capabilities. Report length includes Windows report-ID byte.\r\n"
        L"Physical control names and stick/throttle ownership are UNKNOWN.\r\n\r\n";
    std::size_t targets{};
    for (const auto& device : devices) if (device.isPs28()) ++targets;
    out << L"PS28 matching HID collections: " << targets << L"\r\n"
        L"All present HID collections: " << devices.size() << L"\r\n\r\n";
    // Display the target first, while retaining inaccessible/other collections.
    for (const bool target : {true, false}) {
        for (const auto& device : devices) {
            if (device.isPs28() != target) continue;
            out << (target ? L"PS28 TARGET" : L"HID COLLECTION") << L"\r\n";
            out << L"Manufacturer: " << device.manufacturer << L"\r\n"
                L"Product: " << device.product << L"\r\n";
            if (device.attributes) {
                out << L"VID: " << std::hex << std::uppercase << std::setfill(L'0')
                    << std::setw(4) << device.attributes->VendorID << L"  PID: "
                    << std::setw(4) << device.attributes->ProductID << std::dec << L"\r\n";
            } else out << L"VID/PID: unavailable (identity unverified)\r\n";
            out << L"Path: " << device.path << L"\r\n"
                L"Connection: enumerated present at snapshot; input liveness untested\r\n";
            if (device.capabilities) {
                const auto& caps = *device.capabilities;
                out << L"Usage page: 0x" << std::hex << caps.UsagePage
                    << L"  Usage: 0x" << caps.Usage << std::dec << L"\r\n"
                    L"Report byte lengths: input=" << caps.InputReportByteLength
                    << L", output=" << caps.OutputReportByteLength
                    << L", feature=" << caps.FeatureReportByteLength << L"\r\n";
                for (const auto& value : device.values) {
                    out << L"  VALUE report=" << static_cast<unsigned>(value.ReportID)
                        << L" page=0x" << std::hex << value.UsagePage
                        << L" usage=0x" << (value.IsRange ? value.Range.UsageMin : value.NotRange.Usage)
                        << L"..0x" << (value.IsRange ? value.Range.UsageMax : value.NotRange.Usage)
                        << std::dec << L" link=" << value.LinkCollection
                        << L" logical=[" << value.LogicalMin << L"," << value.LogicalMax << L"]"
                        << L" bits=" << value.BitSize << L" count=" << value.ReportCount
                        << L" null=" << static_cast<unsigned>(value.HasNull) << L"\r\n";
                }
                for (const auto& button : device.buttons) {
                    out << L"  BUTTON report=" << static_cast<unsigned>(button.ReportID)
                        << L" page=0x" << std::hex << button.UsagePage
                        << L" usage=0x" << (button.IsRange ? button.Range.UsageMin : button.NotRange.Usage)
                        << L"..0x" << (button.IsRange ? button.Range.UsageMax : button.NotRange.Usage)
                        << std::dec << L" link=" << button.LinkCollection << L"\r\n";
                }
            }
            for (const auto& error : device.errors) out << L"  ERROR: " << error << L"\r\n";
            out << L"\r\n";
        }
    }
    return out.str();
}
}
