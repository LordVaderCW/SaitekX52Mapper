#pragma once
#include "../device/HidEnumerator.hpp"
#include <span>

namespace x52 {
[[nodiscard]] std::wstring InventoryText(std::span<const HidDeviceInfo> devices);
}
