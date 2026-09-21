#include "ReportDecoder.hpp"
#include <sstream>
#include <stdexcept>

namespace x52 {
namespace {
void Check(NTSTATUS status, const char* api)
{
    if (status != HIDP_STATUS_SUCCESS) {
        std::ostringstream out;
        out << api << " failed: HID status 0x" << std::hex << static_cast<ULONG>(status);
        throw std::runtime_error(out.str());
    }
}
std::string Id(UCHAR report, USHORT page, USHORT usage, USHORT link)
{
    std::ostringstream out;
    out << "r" << static_cast<unsigned>(report) << ":p" << std::hex << page << ":u" << usage << ":l" << link;
    return out.str();
}
}
ReportDecoder::ReportDecoder(HANDLE handle)
{
    if (!HidD_GetPreparsedData(handle, &data_)) throw WindowsException("HidD_GetPreparsedData", GetLastError());
    try {
        Check(HidP_GetCaps(data_, &caps_), "HidP_GetCaps");
        auto count = caps_.NumberInputValueCaps;
        values_.resize(count);
        if (count) Check(HidP_GetValueCaps(HidP_Input, values_.data(), &count, data_), "HidP_GetValueCaps");
        values_.resize(count);
        count = caps_.NumberInputButtonCaps;
        buttons_.resize(count);
        if (count) Check(HidP_GetButtonCaps(HidP_Input, buttons_.data(), &count, data_), "HidP_GetButtonCaps");
        buttons_.resize(count);
    } catch (...) { HidD_FreePreparsedData(data_); data_ = nullptr; throw; }
}
ReportDecoder::~ReportDecoder() { if (data_) HidD_FreePreparsedData(data_); }
bool ReportDecoder::Decode(std::span<std::uint8_t> report, X52State& state, std::string& error) const
{
    if (report.empty() || report.size() != caps_.InputReportByteLength) {
        error = "Input report length does not match HID capabilities"; return false;
    }
    X52State next = state;
    bool matched = false;
    try {
        for (const auto& cap : values_) {
            if (cap.ReportID != report[0]) continue;
            matched = true;
            if (!cap.IsRange && cap.ReportCount > 1)
                throw std::runtime_error("HID value arrays require separate decoding; input withheld");
            const auto first = cap.IsRange ? cap.Range.UsageMin : cap.NotRange.Usage;
            const auto last = cap.IsRange ? cap.Range.UsageMax : cap.NotRange.Usage;
            for (unsigned usage = first; usage <= last; ++usage) {
                ULONG raw{};
                Check(HidP_GetUsageValue(HidP_Input, cap.UsagePage, cap.LinkCollection,
                    static_cast<USAGE>(usage), &raw, data_, reinterpret_cast<PCHAR>(report.data()),
                    static_cast<ULONG>(report.size())), "HidP_GetUsageValue");
                Control control;
                control.id = Id(cap.ReportID, cap.UsagePage, static_cast<USHORT>(usage), cap.LinkCollection);
                control.reportId = cap.ReportID;
                control.kind = cap.UsagePage == 1 && usage == 0x39 ? ControlKind::Hat : ControlKind::Axis;
                control.minimum = cap.LogicalMin;
                control.maximum = cap.LogicalMin < 0 ? static_cast<std::int64_t>(cap.LogicalMax) : static_cast<std::int64_t>(static_cast<ULONG>(cap.LogicalMax));
                control.raw = SignExtend(raw, cap.BitSize, cap.LogicalMin < 0);
                const bool inRange = control.raw >= control.minimum && control.raw <= control.maximum;
                control.valid = inRange || (cap.HasNull && control.kind == ControlKind::Hat);
                control.normalized = control.kind == ControlKind::Hat ?
                    (inRange ? static_cast<double>(control.raw - control.minimum) : -1.0) :
                    Normalize(control.raw, control.minimum, control.maximum);
                if (!control.valid) throw std::runtime_error("Out-of-range HID value: " + control.id);
                next.controls[control.id] = control;
            }
        }
        for (const auto& cap : buttons_) {
            if (cap.ReportID != report[0]) continue;
            matched = true;
            const auto first = cap.IsRange ? cap.Range.UsageMin : cap.NotRange.Usage;
            const auto last = cap.IsRange ? cap.Range.UsageMax : cap.NotRange.Usage;
            ULONG count = HidP_MaxUsageListLength(HidP_Input, cap.UsagePage, data_);
            std::vector<USAGE> pressed(count);
            Check(HidP_GetUsages(HidP_Input, cap.UsagePage, cap.LinkCollection, pressed.data(), &count,
                data_, reinterpret_cast<PCHAR>(report.data()), static_cast<ULONG>(report.size())), "HidP_GetUsages");
            pressed.resize(count);
            for (unsigned usage = first; usage <= last; ++usage) {
                Control control;
                control.id = Id(cap.ReportID, cap.UsagePage, static_cast<USHORT>(usage), cap.LinkCollection);
                control.kind = ControlKind::Button;
                control.reportId = cap.ReportID;
                control.maximum = 1;
                control.raw = std::find(pressed.begin(), pressed.end(), usage) != pressed.end() ? 1 : 0;
                control.normalized = static_cast<double>(control.raw);
                next.controls[control.id] = control;
            }
        }
        if (!matched) throw std::runtime_error("No supported controls match input report ID");
        state = std::move(next);
        error.clear();
        return true;
    } catch (const std::exception& exception) { error = exception.what(); return false; }
}
}
