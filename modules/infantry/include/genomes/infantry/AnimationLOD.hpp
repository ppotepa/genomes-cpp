#pragma once

#include <cstdint>
#include <limits>

namespace genomes::infantry {

enum class AnimationLOD : std::uint8_t {
    Near,
    Mid,
    Far,
    Offscreen,
};

struct AnimationLODSpec final {
    std::uint32_t cadence_ticks{1};
    bool evaluate_contacts{true};
    bool evaluate_face{true};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return cadence_ticks > 0U;
    }
};

[[nodiscard]] constexpr AnimationLODSpec animationLODSpec(AnimationLOD tier) noexcept {
    switch (tier) {
    case AnimationLOD::Near:
        return {1U, true, true};
    case AnimationLOD::Mid:
        return {2U, true, true};
    case AnimationLOD::Far:
        return {4U, false, true};
    case AnimationLOD::Offscreen:
        return {8U, false, false};
    }
    return {1U, true, true};
}

// Scheduler state owns only presentation cadence. It never owns or resets the
// authoritative locomotion phase, face identity or simulation timers.
class AnimationLODState final {
public:
    static constexpr std::uint64_t InvalidTick = std::numeric_limits<std::uint64_t>::max();

    [[nodiscard]] AnimationLOD tier() const noexcept { return tier_; }
    void setTier(AnimationLOD tier) noexcept {
        tier_ = tier;
        // A tier transition gets one resynchronization sample on the next
        // evaluate call without changing last_evaluated_tick_.
        next_due_tick_ = 0U;
    }

    [[nodiscard]] AnimationLODSpec spec() const noexcept { return animationLODSpec(tier_); }
    [[nodiscard]] std::uint64_t lastEvaluatedTick() const noexcept {
        return last_evaluated_tick_;
    }
    [[nodiscard]] std::uint64_t evaluationCount() const noexcept { return evaluation_count_; }

    [[nodiscard]] bool due(std::uint64_t simulation_tick) const noexcept {
        return last_evaluated_tick_ == InvalidTick || simulation_tick >= next_due_tick_;
    }

    [[nodiscard]] std::uint64_t elapsedTicks(std::uint64_t simulation_tick) const noexcept {
        if (last_evaluated_tick_ == InvalidTick || simulation_tick <= last_evaluated_tick_) {
            return 1U;
        }
        return simulation_tick - last_evaluated_tick_;
    }

    void markEvaluated(std::uint64_t simulation_tick) noexcept {
        last_evaluated_tick_ = simulation_tick;
        const std::uint64_t cadence = static_cast<std::uint64_t>(spec().cadence_ticks);
        next_due_tick_ = simulation_tick > InvalidTick - cadence
                             ? InvalidTick
                             : simulation_tick + cadence;
        ++evaluation_count_;
    }

private:
    AnimationLOD tier_{AnimationLOD::Near};
    std::uint64_t last_evaluated_tick_{InvalidTick};
    std::uint64_t next_due_tick_{0U};
    std::uint64_t evaluation_count_{0U};
};

} // namespace genomes::infantry
