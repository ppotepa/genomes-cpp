#pragma once

#include <genomes/destruction/Material.hpp>
#include <genomes/foundation/Types.hpp>

#include <cstdint>

namespace genomes::destruction {

inline constexpr std::uint32_t ImpactSolverVersion = 1;

enum class ImpactOutcome : std::uint8_t {
    Contact,
    Penetrated,
    Stopped,
    BudgetExceeded,
};

struct ImpactEnergyLedger final {
    float initial_translational{0.0F};
    float initial_rotational{0.0F};
    float material_work{0.0F};
    float contact_loss{0.0F};
    float target_work{0.0F};
    float final_translational{0.0F};
    float final_rotational{0.0F};
    float balance_error{0.0F};
};

struct ImpactResult final {
    std::uint32_t version{ImpactSolverVersion};
    ImpactOutcome outcome{ImpactOutcome::Contact};
    foundation::Vec3 relative_incoming_velocity{};
    foundation::Vec3 outgoing_velocity{};
    foundation::Vec3 outgoing_angular_velocity{};
    foundation::Vec3 projectile_impulse{};
    foundation::Vec3 target_impulse{};
    foundation::Vec3 target_torque{};
    ImpactEnergyLedger energy{};
    float residual_relative_energy{0.0F};
    float traversed_distance{0.0F};
    std::uint32_t intervals_visited{0};
    std::uint32_t interfaces_visited{0};
    bool budget_exhausted{false};
};

} // namespace genomes::destruction
