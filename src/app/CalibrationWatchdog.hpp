#pragma once
#include "../device/DeadzoneSettings.hpp"

namespace x52 {
struct CalibrationObservation {
    bool battlefieldRunning{};
    bool reloaded{};
    std::optional<DeadzoneSettings> settings;
};
// Process-name check only: no game handles, injection or input hooks.
CalibrationObservation ObserveBattlefieldCalibration();

// Missing driver calibration is the only trigger. Axis positions never enter
// this policy: a held stick is indistinguishable from an offset without intent.
class CalibrationWatchdog final {
public:
    bool Observe(const CalibrationObservation& observation, std::uint64_t now);
    void Pause() { paused_=true; }
    bool Paused() const { return paused_ || attempts_>=3; }
private:
    std::optional<DeadzoneSettings> previous_;
    unsigned missing_{}, active_{}, attempts_{};
    bool attemptedLoss_{}, paused_{};
    std::uint64_t retryAfter_{};
};
}
