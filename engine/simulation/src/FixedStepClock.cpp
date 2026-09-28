#include <genomes/simulation/FixedStepClock.hpp>

#include <cassert>
#include <algorithm>
#include <limits>

namespace genomes::simulation {

namespace {

[[nodiscard]] foundation::Nanoseconds positiveDuration(foundation::Nanoseconds value) noexcept {
    return value.count() > 0 ? value : foundation::Nanoseconds::zero();
}

} // namespace

FixedStepClock::FixedStepClock(FixedStepConfig config) noexcept : config_(config) {
    assert(config_.max_steps > 0);
    assert(config_.max_frame_delta > foundation::Nanoseconds::zero());
    if (config_.max_steps == 0) {
        config_.max_steps = 1;
    }
    if (config_.max_frame_delta <= foundation::Nanoseconds::zero()) {
        config_.max_frame_delta = std::chrono::milliseconds(250);
    }
}

FixedStepAdvanceResult FixedStepClock::advance(TimePoint now,
                                                StepCallback callback,
                                                const void* user_data) noexcept {
    if (!last_wall_time_.has_value()) {
        last_wall_time_ = now;
        return {0, 1.0, {}, {}, current_tick_, current_tick_};
    }

    const TimePoint previous = *last_wall_time_;
    last_wall_time_ = now;
    const foundation::Nanoseconds raw_delta =
        now >= previous ? std::chrono::duration_cast<foundation::Nanoseconds>(now - previous)
                        : foundation::Nanoseconds::zero();
    return advanceBy(raw_delta, callback, user_data);
}

FixedStepAdvanceResult FixedStepClock::advanceBy(foundation::Nanoseconds frame_delta,
                                                 StepCallback callback,
                                                 const void* user_data) noexcept {
    const foundation::Nanoseconds raw_delta = positiveDuration(frame_delta);
    const foundation::Nanoseconds consumed =
        std::min(raw_delta, config_.max_frame_delta);
    std::uint64_t dropped_nanoseconds =
        static_cast<std::uint64_t>((raw_delta - consumed).count());

    // phase_units is nanoseconds multiplied by 60. A threshold of 1e9 is one
    // exact rational 1/60 simulation step, avoiding a rounded 16,666,667 ns
    // accumulator.
    phase_units_ += static_cast<std::uint64_t>(consumed.count()) * FixedStepDenominator;

    const foundation::SimulationTick first_tick = current_tick_;
    std::uint32_t steps = 0;
    while (phase_units_ >= PhaseThreshold && steps < config_.max_steps) {
        phase_units_ -= PhaseThreshold;
        if (callback != nullptr) {
            callback(1.0 / static_cast<double>(FixedStepDenominator), current_tick_, user_data);
        }
        current_tick_.increment();
        ++steps;
    }

    if (phase_units_ >= PhaseThreshold) {
        const std::uint64_t excess_steps = phase_units_ / PhaseThreshold;
        phase_units_ %= PhaseThreshold;
        dropped_nanoseconds += (excess_steps * 1'000'000'000ull) / FixedStepDenominator;
    }

    const auto dropped = foundation::Nanoseconds(
        static_cast<foundation::Nanoseconds::rep>(
            std::min<std::uint64_t>(dropped_nanoseconds,
                                    static_cast<std::uint64_t>(
                                        std::numeric_limits<foundation::Nanoseconds::rep>::max()))));
    dropped_time_total_ += dropped;
    return {steps,
            static_cast<double>(phase_units_) / static_cast<double>(PhaseThreshold),
            dropped,
            consumed,
            first_tick,
            current_tick_};
}

void FixedStepClock::resetTiming() noexcept {
    last_wall_time_.reset();
    phase_units_ = 0;
}

void FixedStepClock::resetAll() noexcept {
    resetTiming();
    dropped_time_total_ = {};
    current_tick_ = {};
}

} // namespace genomes::simulation
