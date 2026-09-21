#pragma once
#include "../util/Windows.hpp"
#include <hidsdi.h>
#include <hidpi.h>
#include <optional>
#include <stop_token>
#include <vector>

namespace x52 {
inline constexpr USHORT SAITEK_VENDOR_ID = 0x06A3;
inline constexpr USHORT X52_PRODUCT_ID = 0x075C;

struct HidDeviceInfo {
    std::wstring path;
    std::wstring manufacturer;
    std::wstring product;
    std::optional<HIDD_ATTRIBUTES> attributes;
    std::optional<HIDP_CAPS> capabilities;
    std::vector<HIDP_VALUE_CAPS> values;
    std::vector<HIDP_BUTTON_CAPS> buttons;
    std::vector<std::wstring> errors;
    [[nodiscard]] bool isPs28() const noexcept
    {
        return attributes && attributes->VendorID == SAITEK_VENDOR_ID &&
            attributes->ProductID == X52_PRODUCT_ID;
    }
};

// Returns a point-in-time inventory, not a liveness guarantee.
// Metadata queries use zero desired access. No output or feature reports are sent.
// A top-level failure throws; per-interface failures remain visible in errors.
[[nodiscard]] std::vector<HidDeviceInfo> EnumerateHid(std::stop_token stop = {});
}
