#pragma once

#include <genomes/simulation/FixedStepClock.hpp>

#include <cmath>
#include <cstdint>

namespace genomes::simulation {

inline constexpr std::uint32_t SessionSimulationTickRateHz = 60U;

struct TickContext final {
    foundation::SimulationTick tick{};
    double fixed_dt_seconds{1.0 / static_cast<double>(SessionSimulationTickRateHz)};
    std::uint32_t tick_rate_hz{SessionSimulationTickRateHz};
};

[[nodiscard]] inline std::uint64_t secondsToNextTick(double seconds,
                                                       std::uint32_t tick_rate_hz =
                                                           SessionSimulationTickRateHz) noexcept {
    if (!std::isfinite(seconds) || seconds <= 0.0 || tick_rate_hz == 0U) return 0U;
    return static_cast<std::uint64_t>(std::ceil(seconds * static_cast<double>(tick_rate_hz)));
}

class SessionSimulationClock final {
public:
    explicit SessionSimulationClock(FixedStepConfig config = {}) noexcept : clock_(config) {}

    template <class Callback>
    [[nodiscard]] FixedStepAdvanceResult advanceBy(foundation::Nanoseconds frame_delta,
                                                    Callback&& callback) noexcept {
        return clock_.advanceBy(frame_delta, [&callback](double fixed_dt, foundation::SimulationTick tick) {
            // FixedStepClock invokes the callback before advancing its
            // internal cursor.  A session context names the tick being
            // executed, so expose the one-based next tick to consumers.
            tick.increment();
            callback(TickContext{tick, fixed_dt, SessionSimulationTickRateHz});
        });
    }

    [[nodiscard]] foundation::SimulationTick currentTick() const noexcept {
        return clock_.currentTick();
    }

    void resetAll() noexcept { clock_.resetAll(); }

private:
    FixedStepClock clock_;
};

} // namespace genomes::simulation
