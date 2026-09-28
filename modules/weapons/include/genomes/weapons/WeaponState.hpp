#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/foundation/Time.hpp>
#include <genomes/weapons/WeaponCatalog.hpp>

#include <cstdint>

namespace genomes::weapons {

enum class WeaponHandlingState : std::uint8_t {
    Stowed,
    Drawing,
    Held,
    Holstering,
};

enum class HandOwnership : std::uint8_t {
    Free,
    Primary,
    Support,
};

struct WeaponRuntimeState final {
    WeaponId selected_weapon{0};
    WeaponId active_weapon{0};
    WeaponHandlingState handling{WeaponHandlingState::Stowed};
    HandOwnership primary_hand{HandOwnership::Free};
    HandOwnership support_hand{HandOwnership::Free};
    float transition_seconds{0.0F};
    float readiness{0.0F};
    float requested_readiness{0.0F};
    float support_weight{0.0F};
    float recoil_offset{0.0F};
    float aim_yaw{0.0F};
    float aim_pitch{0.0F};
    std::uint64_t shot_sequence{0};
    foundation::SimulationTick next_fire{};

    [[nodiscard]] bool valid() const noexcept;
};

} // namespace genomes::weapons
