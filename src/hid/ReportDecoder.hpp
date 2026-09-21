#pragma once
#include "../device/HidEnumerator.hpp"
#include "../input/State.hpp"
#include <span>

namespace x52 {
class ReportDecoder final {
public:
    explicit ReportDecoder(HANDLE handle);
    ~ReportDecoder();
    ReportDecoder(const ReportDecoder&) = delete;
    ReportDecoder& operator=(const ReportDecoder&) = delete;
    [[nodiscard]] const HIDP_CAPS& caps() const noexcept { return caps_; }
    // Updates only fields belonging to this report ID. No PS28 offsets are guessed.
    [[nodiscard]] bool Decode(std::span<std::uint8_t> report, X52State& state, std::string& error) const;
private:
    PHIDP_PREPARSED_DATA data_{};
    HIDP_CAPS caps_{};
    std::vector<HIDP_VALUE_CAPS> values_;
    std::vector<HIDP_BUTTON_CAPS> buttons_;
};
}
