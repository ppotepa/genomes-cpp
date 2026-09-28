#pragma once

#include <genomes/foundation/Time.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/simulation/Entity.hpp>

#include <cstdint>
#include <vector>

namespace genomes::combat {

enum class DamageType : std::uint8_t {
    Kinetic,
    Explosive,
};

enum class ImpactTargetKind : std::uint8_t {
    Infantry,
    Structural,
    Generic,
};

struct ImpactEvent final {
    simulation::EntityId source{};
    simulation::EntityId target{};
    ImpactTargetKind target_kind{ImpactTargetKind::Generic};
    foundation::StableId semantic_part{0};
    foundation::Vec3 point{};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
    float energy{0.0F};
    foundation::SimulationTick tick{};
    std::uint64_t sequence{0};
};

struct DamageCommand final {
    simulation::EntityId source{};
    simulation::EntityId target{};
    float amount{0.0F};
    DamageType type{DamageType::Kinetic};
    foundation::SimulationTick tick{};
    std::uint64_t sequence{0};
};

struct CombatPresentationEvent final {
    foundation::StableId source{0};
    foundation::StableId weapon_id{0};
    foundation::Vec3 origin{};
    foundation::Vec3 direction{0.0F, 0.0F, 1.0F};
    foundation::SimulationTick tick{};
    std::uint64_t sequence{0};
};

} // namespace genomes::combat
