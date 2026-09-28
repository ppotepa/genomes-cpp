#pragma once

#include <genomes/foundation/Time.hpp>
#include <genomes/foundation/Types.hpp>

#include <array>
#include <cstdint>

namespace genomes::simulation {

// Cadence is expressed in simulation ticks rather than wall-clock time.  This
// keeps expensive systems deterministic and makes a skipped update explicit to
// the system consuming CadenceDecision::elapsed_ticks.
enum class CadenceKind : std::uint8_t {
    EveryTick,
    EveryNTicks,
    RationalFrequency,
    AdaptiveTier,
};

enum class CadenceTier : std::uint8_t {
    Combat = 0,
    Active = 1,
    Dormant = 2,
};

struct CadencePolicy final {
    CadenceKind kind{CadenceKind::EveryTick};
    std::uint32_t period_ticks{1};

    // RationalFrequency is expressed as updates per second.  The simulation
    // clock currently has a 60 Hz fixed step; conversion is integer-only.
    std::uint32_t frequency_numerator{60};
    std::uint32_t frequency_denominator{1};

    // Periods for Combat, Active and Dormant tiers respectively.
    std::array<std::uint32_t, 3> tier_period_ticks{1, 6, 30};

    [[nodiscard]] bool valid() const noexcept;
};

struct CadenceState final {
    foundation::SimulationTick last_run_tick{};
    foundation::SimulationTick next_due_tick{};
    CadenceTier tier{CadenceTier::Active};
    bool initialized{false};
    bool has_run{false};
};

struct CadenceDecision final {
    bool due{false};
    // Number of simulation ticks since the previous execution.  The first
    // execution returns zero because no prior execution exists.
    std::uint64_t elapsed_ticks{0};
};

// Stable phase prevents a large population from running an expensive system
// on one tick.  It is derived only from semantic identity and period.
[[nodiscard]] std::uint32_t cadencePhase(foundation::StableId stable_id,
                                          std::uint32_t period_ticks) noexcept;

[[nodiscard]] std::uint32_t cadencePeriod(const CadencePolicy& policy,
                                          CadenceTier tier = CadenceTier::Active) noexcept;

// Evaluate and advance one cadence state.  Calls with an older tick are
// ignored, which makes stale asynchronous work unable to move the schedule
// backwards.
[[nodiscard]] CadenceDecision evaluateCadence(const CadencePolicy& policy,
                                              CadenceState& state,
                                              foundation::StableId stable_id,
                                              foundation::SimulationTick tick) noexcept;

// Relevance is authoritative simulation state.  Changing tier makes the next
// update due immediately; render-camera visibility must not call this API.
void setCadenceTier(CadenceState& state,
                    CadenceTier tier,
                    foundation::SimulationTick tick) noexcept;

} // namespace genomes::simulation
