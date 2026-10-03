#pragma once

#include <genomes/simulation/FixedStepClock.hpp>

#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>

namespace genomes::simulation {

inline constexpr std::uint32_t SessionSimulationTickRateHz = 60U;

struct TickContext final {
    foundation::SimulationTick tick{};
    double fixed_dt_seconds{1.0 / static_cast<double>(SessionSimulationTickRateHz)};
    std::uint32_t tick_rate_hz{SessionSimulationTickRateHz};
};

enum class TickSchedulingMode : std::uint8_t {
    Interactive,
    DeterministicCapture,
    Headless,
};

struct SessionTickPlan final {
    FixedStepAdvanceResult advance{};
    std::array<TickContext, 8> ticks{};
    std::uint32_t tick_count{0};
    TickSchedulingMode mode{TickSchedulingMode::Interactive};

    [[nodiscard]] bool requiresCompletionBeforePublish() const noexcept {
        return mode != TickSchedulingMode::Interactive;
    }
};

[[nodiscard]] inline std::uint64_t secondsToNextTick(double seconds,
                                                       std::uint32_t tick_rate_hz =
                                                           SessionSimulationTickRateHz) noexcept {
    if (!std::isfinite(seconds) || seconds <= 0.0 || tick_rate_hz == 0U) return 0U;
    return static_cast<std::uint64_t>(std::ceil(seconds * static_cast<double>(tick_rate_hz)));
}

class SessionSimulationClock final {
public:
    explicit SessionSimulationClock(FixedStepConfig config = {}) noexcept
        : clock_(normalize(config)) {}

    [[nodiscard]] SessionTickPlan planBy(
        foundation::Nanoseconds frame_delta,
        TickSchedulingMode mode = TickSchedulingMode::Interactive) noexcept {
        SessionTickPlan plan;
        plan.mode = mode;
        plan.advance = clock_.advanceBy(
            frame_delta, [&plan](double fixed_dt, foundation::SimulationTick tick) noexcept {
                tick.increment();
                if (plan.tick_count < plan.ticks.size()) {
                    plan.ticks[plan.tick_count++] =
                        TickContext{tick, fixed_dt, SessionSimulationTickRateHz};
                }
            });
        return plan;
    }

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
    [[nodiscard]] static FixedStepConfig normalize(FixedStepConfig config) noexcept {
        config.max_steps = std::min<std::uint32_t>(config.max_steps, 8U);
        return config;
    }

    FixedStepClock clock_;
};

} // namespace genomes::simulation
