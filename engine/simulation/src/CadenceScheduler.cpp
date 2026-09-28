#include <genomes/simulation/Cadence.hpp>

#include <algorithm>
#include <limits>

namespace genomes::simulation {

namespace {

[[nodiscard]] constexpr std::uint32_t clampPeriod(std::uint32_t period) noexcept {
    return std::max<std::uint32_t>(1, period);
}

[[nodiscard]] constexpr std::size_t tierIndex(CadenceTier tier) noexcept {
    return static_cast<std::size_t>(tier);
}

[[nodiscard]] constexpr std::uint32_t rationalPeriod(const CadencePolicy& policy) noexcept {
    if (policy.frequency_numerator == 0 || policy.frequency_denominator == 0) {
        return std::numeric_limits<std::uint32_t>::max();
    }
    constexpr std::uint64_t fixed_hz = 60;
    const std::uint64_t numerator =
        fixed_hz * static_cast<std::uint64_t>(policy.frequency_denominator);
    const std::uint64_t denominator = policy.frequency_numerator;
    const std::uint64_t period = (numerator + denominator - 1) / denominator;
    return clampPeriod(period >= std::numeric_limits<std::uint32_t>::max()
                           ? std::numeric_limits<std::uint32_t>::max()
                           : static_cast<std::uint32_t>(period));
}

[[nodiscard]] constexpr std::uint64_t mix(std::uint64_t value) noexcept {
    value += 0x9E3779B97F4A7C15ull;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
    return value ^ (value >> 31);
}

} // namespace

bool CadencePolicy::valid() const noexcept {
    if (kind == CadenceKind::EveryNTicks && period_ticks == 0) {
        return false;
    }
    if (kind == CadenceKind::RationalFrequency &&
        (frequency_numerator == 0 || frequency_denominator == 0)) {
        return false;
    }
    if (kind == CadenceKind::AdaptiveTier) {
        for (const std::uint32_t period : tier_period_ticks) {
            if (period == 0) {
                return false;
            }
        }
    }
    return true;
}

std::uint32_t cadencePhase(foundation::StableId stable_id,
                           std::uint32_t period_ticks) noexcept {
    if (period_ticks <= 1) {
        return 0;
    }
    return static_cast<std::uint32_t>(mix(stable_id) % period_ticks);
}

std::uint32_t cadencePeriod(const CadencePolicy& policy, CadenceTier tier) noexcept {
    switch (policy.kind) {
    case CadenceKind::EveryTick:
        return 1;
    case CadenceKind::EveryNTicks:
        return clampPeriod(policy.period_ticks);
    case CadenceKind::RationalFrequency:
        return rationalPeriod(policy);
    case CadenceKind::AdaptiveTier:
        return clampPeriod(policy.tier_period_ticks[tierIndex(tier)]);
    }
    return 1;
}

CadenceDecision evaluateCadence(const CadencePolicy& policy,
                                CadenceState& state,
                                foundation::StableId stable_id,
                                foundation::SimulationTick tick) noexcept {
    if (!policy.valid() || (state.initialized && state.has_run && tick < state.last_run_tick)) {
        return {};
    }

    const std::uint32_t period = cadencePeriod(policy, state.tier);
    if (!state.initialized) {
        const std::uint32_t phase = cadencePhase(stable_id, period);
        const std::uint32_t remainder =
            static_cast<std::uint32_t>(tick.value % static_cast<std::uint64_t>(period));
        const std::uint32_t offset = (phase + period - remainder) % period;
        state.next_due_tick = foundation::SimulationTick{tick.value + offset};
        state.initialized = true;
    }

    if (tick < state.next_due_tick) {
        return {};
    }

    const std::uint64_t elapsed = state.has_run ? tick.value - state.last_run_tick.value : 0;
    state.last_run_tick = tick;
    state.has_run = true;
    state.next_due_tick = foundation::SimulationTick{tick.value + period};
    return {true, elapsed};
}

void setCadenceTier(CadenceState& state,
                    CadenceTier tier,
                    foundation::SimulationTick tick) noexcept {
    if (state.tier == tier) {
        return;
    }
    state.tier = tier;
    state.next_due_tick = tick;
    state.initialized = true;
}

} // namespace genomes::simulation
