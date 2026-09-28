#pragma once

#include <genomes/ballistics/AmmunitionCatalog.hpp>
#include <genomes/ballistics/ProjectileState.hpp>
#include <genomes/destruction/ImpactSolver.hpp>
#include <genomes/destruction/Material.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <cstdint>

namespace genomes::ballistics {

enum class ContactOutcome : std::uint8_t {
    Penetrated,
    Stopped,
    Ricocheted,
    Glanced,
    Deflected,
};

struct ContactCandidate final {
    foundation::StableId contact_id{0};
    foundation::Vec3 point{};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
    foundation::Vec3 entry_point{};
    foundation::Vec3 exit_point{};
    foundation::Vec3 target_linear_velocity{};
    foundation::Vec3 target_angular_velocity{};
    foundation::Vec3 target_center_of_mass{};
    const destruction::MaterialAssembly* assembly{nullptr};
    destruction::ImpactLimits limits{};

    [[nodiscard]] bool valid() const noexcept;
};

struct ContactResolution final {
    ContactOutcome outcome{ContactOutcome::Stopped};
    destruction::ImpactResult impact{};
    ImpactTransition transition{};
    foundation::Vec3 outgoing_velocity{};
    foundation::Vec3 outgoing_angular_velocity{};
    foundation::Vec3 target_impulse{};
    foundation::Vec3 target_torque{};
    foundation::StableId impulse_token{0};
    bool continue_flight{false};
};

class ContactResolver final {
public:
    [[nodiscard]] static foundation::Result<ContactResolution, foundation::Error> resolve(
        const ProjectileState&,
        const ContactCandidate&,
        const AmmunitionCatalog&,
        const destruction::MaterialCatalog&) noexcept;
};

} // namespace genomes::ballistics
