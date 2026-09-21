#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace x52 {
enum class InputGroup { Unknown, Stick, Throttle };
enum class ControlKind { Axis, Button, Hat };
struct Control {
    std::string id; // report / usage page / usage / link collection; descriptor-derived
    ControlKind kind{ControlKind::Axis};
    std::int64_t raw{}, minimum{}, maximum{};
    double normalized{}; // axes -1..1; buttons 0/1; hats ordinal or -1 (null)
    bool valid{true};
    std::uint8_t reportId{};
    double lastSeenMs{};
};
struct X52State {
    std::map<std::string, Control> controls;
};
struct LearnedControl {
    std::string name;
    InputGroup group{InputGroup::Unknown};
    double neutral{}; // explicitly selected by user; hats default -1, buttons 0
    std::string physicalId, part;
    std::string status{"OBSERVED"};
};
using Assignments = std::map<std::string, LearnedControl>;
struct BitChange {
    std::size_t byte{};
    std::uint8_t before{}, after{}, mask{};
};
inline std::vector<BitChange> Differences(std::span<const std::uint8_t> before,
    std::span<const std::uint8_t> after)
{
    std::vector<BitChange> changes;
    if (before.size() != after.size()) return changes; // separate report IDs/lengths are not comparable
    for (std::size_t i = 0; i < after.size(); ++i)
        if (before[i] != after[i]) changes.push_back({i, before[i], after[i],
            static_cast<std::uint8_t>(before[i] ^ after[i])});
    return changes;
}
inline double Normalize(std::int64_t value, std::int64_t low, std::int64_t high)
{
    if (high <= low) return 0;
    return std::clamp(2.0 * static_cast<double>(value - low) /
        static_cast<double>(high - low) - 1.0, -1.0, 1.0);
}
inline std::int64_t SignExtend(std::uint32_t value, unsigned bits, bool isSigned)
{
    if (bits == 0 || bits > 32) return value;
    const std::uint64_t modulus = std::uint64_t{1} << bits;
    const auto masked = static_cast<std::int64_t>(value & (modulus - 1));
    return isSigned && (static_cast<std::uint64_t>(masked) & (modulus >> 1)) ?
        masked - static_cast<std::int64_t>(modulus) : masked;
}
}
