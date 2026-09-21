#pragma once
#include "../input/State.hpp"
#include <array>
#include <optional>

namespace x52 {
struct GroupActivity {
    std::size_t controls{};
    std::uint64_t axisChanges{}, digitalChanges{};
    std::optional<double> lastChangeMs;
    std::string lastControl;
};
struct ActivityObservation {
    bool started{};
    double unchangedSinceMs{}, observedAtMs{};
    std::string reason;
};
class GroupActivityMonitor final {
public:
    std::array<GroupActivity, 3> groups{}; // Unknown, Stick, Throttle
    bool observationActive{};
    static constexpr double ObservationDelayMs = 250;
    void Reset() { *this = GroupActivityMonitor{}; }
    std::optional<ActivityObservation> End(double now, const std::string& reason)
    {
        std::optional<ActivityObservation> result;
        if (observationActive) result = ActivityObservation{false, unchangedSince_.value_or(now), now, reason};
        observationActive = false; unchangedSince_.reset(); throttleEvidence_ = 0; throttleAnchors_.clear();
        return result;
    }
    std::optional<ActivityObservation> Interrupt(double now, const std::string& reason)
    {
        previous_.clear();
        return End(now, reason);
    }
    std::optional<ActivityObservation> Observe(const X52State& raw, const Assignments& assignments, std::uint8_t reportId, bool valid, double now)
    {
        bool stickPresent = false, stickChanged = false, throttleChanged = false;
        for (auto& group : groups) group.controls = 0;
        for (const auto& [id, control] : raw.controls) {
            const auto link = assignments.find(id);
            const auto group = link == assignments.end() ? InputGroup::Unknown : link->second.group;
            auto& activity = groups.at(static_cast<std::size_t>(group));
            ++activity.controls;
            if (!valid || control.reportId != reportId) continue;
            if (!control.valid) { previous_.erase(id); continue; }
            if (group == InputGroup::Stick) stickPresent = true;
            const auto old = previous_.find(id);
            if (old != previous_.end() && old->second != control.raw) {
                if (control.kind == ControlKind::Axis) ++activity.axisChanges;
                else ++activity.digitalChanges;
                activity.lastChangeMs = now;
                activity.lastControl = link == assignments.end() ? id : link->second.name;
                if (group == InputGroup::Stick) stickChanged = true;
                if (group == InputGroup::Throttle && control.kind != ControlKind::Axis) throttleChanged = true;
            }
            if (group == InputGroup::Throttle && control.kind == ControlKind::Axis) {
                const auto [anchor, inserted] = throttleAnchors_.try_emplace(id, control.raw);
                // Require displacement beyond ordinary one-count jitter; raw evidence remains untouched.
                const auto threshold = std::max(3.0, 0.02 * static_cast<double>(control.maximum - control.minimum));
                if (!inserted && std::abs(static_cast<double>(control.raw - anchor->second)) >= threshold) {
                    throttleChanged = true; anchor->second = control.raw;
                }
            }
            previous_[id] = control.raw;
        }
        // Do not count a discontinuity across an invalid packet as physical activity.
        if (!valid) return Interrupt(now, "Invalid report; observation interrupted");
        if (!stickPresent) return {}; // Only compare stick values freshly sampled in this report.
        if (!unchangedSince_ || stickChanged) {
            const auto ended = End(now, "Stick raw values changed");
            unchangedSince_ = now;
            return ended;
        }
        if (throttleChanged && throttleEvidence_ < 2) ++throttleEvidence_;
        if (!observationActive && throttleChanged && throttleEvidence_ >= 2 && now - *unchangedSince_ >= ObservationDelayMs) {
            observationActive = true;
            return ActivityObservation{true, *unchangedSince_, now, "Stick unchanged while throttle activity observed; not a confirmed dropout"};
        }
        return {};
    }
private:
    std::map<std::string, std::int64_t> previous_;
    std::map<std::string, std::int64_t> throttleAnchors_;
    std::optional<double> unchangedSince_;
    unsigned throttleEvidence_{};
};
}
