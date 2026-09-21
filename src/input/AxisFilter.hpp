#pragma once
#include "State.hpp"
#include <array>
#include <stdexcept>
#include <string_view>

namespace x52 {
struct AxisFilterSettings {
    std::array<bool, 4> enabled{true, true, true, true}; // throttle, side rotary, top rotary, slider
    double smoothingMs{45};
    double jitterCounts{1};
    void Validate() const {
        if (!std::isfinite(smoothingMs) || smoothingMs < 0 || smoothingMs > 250 ||
            !std::isfinite(jitterCounts) || jitterCounts < 0 || jitterCounts > 5)
            throw std::runtime_error("Filter smoothing must be 0-250 ms and jitter tolerance 0-5 raw counts");
    }
};
class AxisFilter final {
    struct Sample { double anchor{}, value{}, time{}; std::string physicalId; };
    std::map<std::string, Sample> samples_;
public:
    void Reset() { samples_.clear(); }
    X52State Process(const X52State& raw, const Assignments& assignments,
        const AxisFilterSettings& settings, std::uint8_t reportId, double now)
    {
        settings.Validate();
        X52State result = raw;
        constexpr std::array<std::string_view, 4> physicalIds{
            "throttle.main", "throttle.rotary_side", "throttle.rotary_top", "throttle.slider"};
        for (auto& [id, control] : result.controls) {
            const auto link = assignments.find(id);
            const auto name = link == assignments.end() ? std::string{} : link->second.physicalId;
            const auto found = std::find(physicalIds.begin(), physicalIds.end(), name);
            if (found == physicalIds.end() || !settings.enabled[static_cast<std::size_t>(found - physicalIds.begin())] ||
                control.kind != ControlKind::Axis || !control.valid || control.maximum <= control.minimum) {
                samples_.erase(id); continue;
            }
            if (control.reportId != reportId) {
                if (const auto previous = samples_.find(id); previous != samples_.end()) control.normalized = previous->second.value;
                continue;
            }
            const double input = control.normalized;
            auto [at, inserted] = samples_.try_emplace(id, Sample{input, input, now, name});
            auto& sample = at->second;
            const double dt = now - sample.time;
            if (inserted || sample.physicalId != name || dt < 0 || dt > 500) sample = {input, input, now, name};
            else {
                const double tolerance = 2 * settings.jitterCounts / static_cast<double>(control.maximum - control.minimum);
                if (std::abs(input - sample.anchor) > tolerance + 1e-12 || input == -1 || input == 1) sample.anchor = input;
                const double alpha = settings.smoothingMs == 0 ? 1 : -std::expm1(-dt / settings.smoothingMs);
                sample.value += alpha * (sample.anchor - sample.value);
                if (std::abs(sample.anchor - sample.value) < 0.00001) sample.value = sample.anchor;
                sample.time = now;
            }
            control.normalized = std::clamp(sample.value, -1.0, 1.0);
        }
        return result;
    }
};
}
