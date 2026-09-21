#pragma once
#include "../input/State.hpp"
#include <optional>
#include <array>

namespace x52 {
enum class StickHealth { Unknown, Healthy, SuspectedDropout, Disconnected, Recovering, Recovered };
enum class RecoveryState { Normal, DropoutSuspected, DropoutConfirmed, Neutralising,
    WaitingForHardware, ReopeningDevice, Validating, Restoring };
enum class DropoutBehaviour { Neutralise, HoldLast };
struct RecoverySettings {
    unsigned validationReports{5};
    double blendMs{100};
    double staleMs{2000};
    DropoutBehaviour behaviour{DropoutBehaviour::Neutralise};
};
struct DeviceHealthMetrics {
    std::optional<double> lastReport, lastValidStickReport, lastValidThrottleReport;
    std::uint64_t reportsReceived{}, readFailures{}, suspectedDropouts{}, confirmedDropouts{}, successfulRecoveries{};
};
class StickHealthMonitor final {
public:
    StickHealth stick{StickHealth::Unknown};
    DeviceHealthMetrics metrics;
    void Observe(double now, bool valid) { metrics.lastReport = now; ++metrics.reportsReceived; if (!valid) ++metrics.readFailures; }
    // No unverified PS28 status bits or motion-based failure heuristic.
    void AnnotateDropout() { stick = StickHealth::SuspectedDropout; ++metrics.suspectedDropouts; }
};
class X52RecoveryManager final {
public:
    RecoverySettings settings;
    StickHealthMonitor health;
    RecoveryState state{RecoveryState::WaitingForHardware};
    bool transportAvailable{};
    bool stickSuspended{};
    std::optional<double> lastInterruptionMs;
    void TransportLost(double now, bool disconnected);
    void Reopening() { state = RecoveryState::ReopeningDevice; }
    void SuspendStick(double now);
    void ReturnStick(double now);
    void Tick(double now);
    X52State Process(const X52State& physical, const Assignments& assignments,
        std::uint8_t reportId, bool valid, double now);
    [[nodiscard]] X52State SafeState(const X52State& physical, const Assignments& assignments, double now) const;
private:
    unsigned validCount_{};
    bool transportValidated_{};
    bool validatingStick_{};
    double blendStart_{};
    std::optional<double> failureStart_;
    X52State lastGood_, blendFrom_;
    std::map<std::uint8_t, unsigned> reportValidation_;
    std::map<std::uint8_t, unsigned> stickValidation_;
};
inline const char* RecoveryName(RecoveryState state)
{
    constexpr std::array names{"Normal", "Dropout suspected", "Dropout confirmed", "Neutralising",
        "Waiting for hardware", "Reopening input", "Validating reports", "Restoring"};
    return names.at(static_cast<std::size_t>(state));
}
inline const char* HealthName(StickHealth health)
{
    constexpr std::array names{"Unknown", "Healthy", "Suspected dropout (user marked)",
        "Disconnected", "Recovering", "Recovered (user marked)"};
    return names.at(static_cast<std::size_t>(health));
}
class IX52HardwareRecovery {
public:
    virtual ~IX52HardwareRecovery() = default;
    virtual bool IsSupported() const noexcept = 0;
    virtual bool AttemptRecovery() = 0;
};
class UnsupportedHardwareRecovery final : public IX52HardwareRecovery {
public:
    bool IsSupported() const noexcept override { return false; }
    bool AttemptRecovery() override { return false; }
};
}
