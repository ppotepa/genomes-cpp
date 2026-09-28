#include <genomes/ballistics/ContactResolver.hpp>

#include <algorithm>
#include <cmath>

namespace genomes::ballistics {

namespace {

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] float dot(foundation::Vec3 left, foundation::Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] foundation::Vec3 multiply(foundation::Vec3 value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] foundation::Vec3 subtract(foundation::Vec3 left,
                                        foundation::Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] float length(foundation::Vec3 value) noexcept {
    return std::sqrt(std::max(0.0F, dot(value, value)));
}

[[nodiscard]] foundation::Vec3 normalized(foundation::Vec3 value) noexcept {
    return multiply(value, 1.0F / std::max(1.0e-6F, length(value)));
}

[[nodiscard]] destruction::ImpactStrategy map_strategy(ConstructionKind construction) noexcept {
    switch (construction) {
    case ConstructionKind::ArmorPiercing:
        return destruction::ImpactStrategy::ArmorPiercing;
    case ConstructionKind::HighExplosive:
        return destruction::ImpactStrategy::HighExplosive;
    case ConstructionKind::Fragmentation:
        return destruction::ImpactStrategy::Fragment;
    case ConstructionKind::FullMetalJacket:
    case ConstructionKind::SoftPoint:
        return destruction::ImpactStrategy::Ball;
    }
    return destruction::ImpactStrategy::Ball;
}

[[nodiscard]] destruction::ImpactOrientation orientation_for(
    foundation::Vec3 forward) noexcept {
    const foundation::Vec3 normalized_forward = normalized(forward);
    const foundation::Vec3 reference_up = std::abs(normalized_forward.y) < 0.9F
                                               ? foundation::Vec3{0.0F, 1.0F, 0.0F}
                                               : foundation::Vec3{1.0F, 0.0F, 0.0F};
    const foundation::Vec3 right = normalized({reference_up.y * normalized_forward.z -
                                                   reference_up.z * normalized_forward.y,
                                               reference_up.z * normalized_forward.x -
                                                   reference_up.x * normalized_forward.z,
                                               reference_up.x * normalized_forward.y -
                                                   reference_up.y * normalized_forward.x});
    const foundation::Vec3 up = {normalized_forward.y * right.z - normalized_forward.z * right.y,
                                 normalized_forward.z * right.x - normalized_forward.x * right.z,
                                 normalized_forward.x * right.y - normalized_forward.y * right.x};
    return {right, up, normalized_forward};
}

} // namespace

bool ContactCandidate::valid() const noexcept {
    return contact_id != 0 && finite(point) && finite(normal) && finite(entry_point) &&
           finite(exit_point) && finite(target_linear_velocity) && finite(target_angular_velocity) &&
           finite(target_center_of_mass) && assembly != nullptr && length(normal) > 1.0e-6F &&
           limits.max_intervals > 0 && limits.max_interfaces > 0 &&
           std::isfinite(limits.stop_energy) && limits.stop_energy >= 0.0F;
}

foundation::Result<ContactResolution, foundation::Error> ContactResolver::resolve(
    const ProjectileState& projectile,
    const ContactCandidate& candidate,
    const AmmunitionCatalog& ammunition,
    const destruction::MaterialCatalog& materials) noexcept {
    if (!projectile.valid() || !candidate.valid()) {
        return foundation::Result<ContactResolution, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid ballistic contact candidate"});
    }
    const AmmunitionStrategy* strategy = ammunition.strategy(projectile.strategy_id);
    if (strategy == nullptr) {
        return foundation::Result<ContactResolution, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "projectile strategy not found"});
    }
    const foundation::Vec3 normal = normalized(candidate.normal);
    const destruction::ImpactInput input{
        projectile.mass_kg,
        projectile.diameter_m,
        projectile.velocity,
        projectile.angular_velocity,
        orientation_for(projectile.body_forward),
        projectile.position,
        candidate.point,
        normal,
        candidate.target_linear_velocity,
        candidate.target_angular_velocity,
        candidate.target_center_of_mass,
        candidate.assembly,
        candidate.entry_point,
        candidate.exit_point,
        map_strategy(strategy->construction),
        strategy->id.value(),
        candidate.limits};
    const auto solved = destruction::ImpactSolver::solve(input, materials);
    if (!solved) {
        return foundation::Result<ContactResolution, foundation::Error>::failure(solved.error());
    }

    ContactResolution result{};
    result.impact = solved.value();
    result.outgoing_velocity = result.impact.outgoing_velocity;
    result.outgoing_angular_velocity = result.impact.outgoing_angular_velocity;
    result.target_impulse = result.impact.target_impulse;
    result.target_torque = result.impact.target_torque;
    result.impulse_token = candidate.contact_id;
    const float speed = std::max(1.0e-6F, length(projectile.velocity));
    const float incidence = std::clamp(
        -dot(multiply(projectile.velocity, 1.0F / speed), normal), 0.0F, 1.0F);
    if (result.impact.outcome == destruction::ImpactOutcome::BudgetExceeded) {
        result.outcome = ContactOutcome::Stopped;
        result.continue_flight = false;
    } else if (incidence < strategy->ricochet_threshold &&
               result.impact.residual_relative_energy > candidate.limits.stop_energy) {
        const foundation::Vec3 reflected = subtract(
            projectile.velocity, multiply(normal, 2.0F * dot(projectile.velocity, normal)));
        const float outgoing_speed = length(result.outgoing_velocity);
        result.outgoing_velocity = multiply(normalized(reflected), outgoing_speed);
        result.outcome = incidence < strategy->ricochet_threshold * 0.5F
                             ? ContactOutcome::Glanced
                             : ContactOutcome::Ricocheted;
        result.continue_flight = true;
    } else if (result.impact.outcome == destruction::ImpactOutcome::Penetrated) {
        result.outcome = ContactOutcome::Penetrated;
        result.continue_flight = true;
    } else {
        result.outcome = ContactOutcome::Stopped;
        result.continue_flight = false;
    }

    result.transition.velocity_delta = subtract(result.outgoing_velocity, projectile.velocity);
    result.transition.material_work = result.impact.energy.material_work;
    result.transition.contact_loss = result.impact.energy.contact_loss;
    result.transition.target_work = result.impact.energy.target_work;
    result.transition.stability_delta = result.continue_flight ? -0.05F : -0.1F;
    result.transition.deformation_delta = std::clamp(
        result.impact.energy.material_work * 0.001F, 0.0F, 0.25F);
    result.transition.ricochet_count_delta =
        (result.outcome == ContactOutcome::Ricocheted || result.outcome == ContactOutcome::Glanced)
            ? 1U
            : 0U;
    return foundation::Result<ContactResolution, foundation::Error>::success(result);
}

} // namespace genomes::ballistics
