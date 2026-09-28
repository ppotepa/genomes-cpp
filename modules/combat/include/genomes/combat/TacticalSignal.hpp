#pragma once

#include <genomes/foundation/Time.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/simulation/Entity.hpp>

#include <cstdint>
#include <optional>

namespace genomes::combat {

using SquadId = std::uint32_t;

enum class TacticalSignalType : std::uint8_t {
    Contact,
    Threat,
    Order
};

enum class TacticalSignalChannel : std::uint8_t {
    Local,
    Radio
};

struct TacticalSignal final {
    simulation::EntityId source{};
    SquadId squad{0U};
    std::optional<simulation::EntityId> subject;
    foundation::Vec3 location{};
    foundation::SimulationTick created_tick{};
    foundation::SimulationTick delivery_tick{};
    std::uint32_t ttl_ticks{0U};
    std::uint16_t hops{0U};
    float confidence{0.0F};
    float range_m{0.0F};
    TacticalSignalType type{TacticalSignalType::Contact};
    TacticalSignalChannel channel{TacticalSignalChannel::Local};
    std::uint64_t stable_sequence{0U};

    [[nodiscard]] bool valid() const noexcept;
};

struct SignalPropagationPolicy final {
    bool enabled{true};
    std::uint32_t communication_delay_ticks{3U};
    std::uint16_t max_hops{1U};
    std::uint32_t max_pending_signals{4096U};
    float max_range_m{150.0F};
    float confidence_decay_per_tick{0.995F};

    [[nodiscard]] bool valid() const noexcept;
};

} // namespace genomes::combat
