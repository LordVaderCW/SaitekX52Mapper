#include "Recovery.hpp"

namespace x52 {
void X52RecoveryManager::TransportLost(double now, bool disconnected)
{
    if (!failureStart_) failureStart_ = now;
    transportAvailable = false;
    transportValidated_ = false;
    validCount_ = 0;
    reportValidation_.clear();
    stickValidation_.clear();
    state = RecoveryState::WaitingForHardware;
    health.stick = disconnected ? StickHealth::Disconnected : StickHealth::Unknown;
}
void X52RecoveryManager::SuspendStick(double now)
{
    if (stickSuspended) return;
    failureStart_ = now;
    stickSuspended = true;
    validatingStick_ = false;
    stickValidation_.clear();
    validCount_ = 0;
    health.AnnotateDropout();
    state = RecoveryState::DropoutSuspected;
}
void X52RecoveryManager::ReturnStick(double now)
{
    if (!stickSuspended) return;
    validatingStick_ = true;
    stickValidation_.clear();
    validCount_ = 0;
    blendStart_ = now;
    state = RecoveryState::Validating;
    health.stick = StickHealth::Recovering;
}
void X52RecoveryManager::Tick(double now)
{
    if (transportAvailable && health.metrics.lastReport &&
        now - *health.metrics.lastReport >= settings.staleMs) TransportLost(now, false);
    if (state == RecoveryState::Restoring && now - blendStart_ >= settings.blendMs)
        state = stickSuspended ? RecoveryState::DropoutSuspected : RecoveryState::Normal;
}
X52State X52RecoveryManager::SafeState(const X52State& physical, const Assignments& assignments, double now) const
{
    auto safe = physical;
    for (auto& [id, control] : safe.controls) {
        const auto assignment = assignments.find(id);
        const bool isThrottle = assignment != assignments.end() && assignment->second.group == InputGroup::Throttle;
        const auto validation = reportValidation_.find(control.reportId);
        const bool reportValidated = validation != reportValidation_.end() && validation->second >= settings.validationReports;
        const bool stale = control.lastSeenMs > 0 && now - control.lastSeenMs >= settings.staleMs;
        const bool affected = !transportValidated_ || !transportAvailable || !reportValidated || stale || (stickSuspended && !isThrottle);
        const auto neutral = control.kind == ControlKind::Hat ? -1.0 :
            (assignment == assignments.end() ? 0.0 : assignment->second.neutral);
        if (affected) {
            const auto last = lastGood_.controls.find(id);
            control.normalized = settings.behaviour == DropoutBehaviour::HoldLast && last != lastGood_.controls.end() ?
                last->second.normalized : neutral;
            control.valid = false; // raw remains evidence; only normalized is the safety-gated value
        } else if (state == RecoveryState::Restoring && control.kind == ControlKind::Axis && settings.blendMs > 0) {
            const auto origin = blendFrom_.controls.find(id);
            const auto from = origin == blendFrom_.controls.end() ? neutral : origin->second.normalized;
            control.normalized = from + (control.normalized - from) * std::clamp((now - blendStart_) / settings.blendMs, 0.0, 1.0);
        }
    }
    return safe;
}
X52State X52RecoveryManager::Process(const X52State& physical, const Assignments& assignments,
    std::uint8_t reportId, bool valid, double now)
{
    health.Observe(now, valid);
    if (!valid) { TransportLost(now, false); return SafeState(physical, assignments, now); }
    transportAvailable = true;
    auto& reportCount = reportValidation_[reportId];
    if (reportCount < settings.validationReports) ++reportCount;
    bool stickReport = false;
    for (const auto& [id, control] : physical.controls) {
        if (control.reportId != reportId) continue;
        const auto found = assignments.find(id);
        if (found == assignments.end()) continue;
        if (found->second.group == InputGroup::Stick) { health.metrics.lastValidStickReport = now; stickReport = true; }
        if (found->second.group == InputGroup::Throttle) health.metrics.lastValidThrottleReport = now;
    }
    const bool hasAssignedStick = std::any_of(assignments.begin(), assignments.end(), [](const auto& entry) {
        return entry.second.group == InputGroup::Stick;
    });
    if (!hasAssignedStick) stickReport = true; // user return marker validates every known report when ownership is unknown
    if (validatingStick_ && stickReport) {
        auto& count = stickValidation_[reportId];
        if (count < settings.validationReports) ++count;
    }
    bool allStickReportsValidated = validatingStick_ && !physical.controls.empty();
    for (const auto& [id, control] : physical.controls) {
        const auto assigned = assignments.find(id);
        if (!hasAssignedStick || (assigned != assignments.end() && assigned->second.group == InputGroup::Stick)) {
            if (stickValidation_[control.reportId] < settings.validationReports) allStickReportsValidated = false;
        }
    }
    const bool transportReady = !transportValidated_ && ++validCount_ >= settings.validationReports;
    if (!transportValidated_ || validatingStick_) {
        state = RecoveryState::Validating;
        if (transportReady || allStickReportsValidated) {
            blendFrom_ = SafeState(physical, assignments, now);
            if (transportReady) transportValidated_ = true;
            if (allStickReportsValidated) {
                stickSuspended = false;
                validatingStick_ = false;
                health.stick = StickHealth::Recovered;
            } else health.stick = stickSuspended ? StickHealth::SuspectedDropout : StickHealth::Unknown;
            blendStart_ = now;
            state = RecoveryState::Restoring;
            if (failureStart_ && !stickSuspended) {
                lastInterruptionMs = now - *failureStart_;
                ++health.metrics.successfulRecoveries;
                failureStart_.reset();
            }
        }
    }
    Tick(now);
    auto safe = SafeState(physical, assignments, now);
    for (const auto& [id, control] : safe.controls) if (control.valid) lastGood_.controls[id] = control;
    return safe;
}
}
