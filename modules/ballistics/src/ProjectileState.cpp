#include <genomes/ballistics/ProjectileState.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace genomes::ballistics {

namespace {

constexpr float kEpsilon = 1.0e-6F;

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] float dot(foundation::Vec3 left, foundation::Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] foundation::Vec3 cross(foundation::Vec3 left,
                                     foundation::Vec3 right) noexcept {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

[[nodiscard]] foundation::Vec3 add(foundation::Vec3 left,
                                   foundation::Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] foundation::Vec3 multiply(foundation::Vec3 value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] float length_squared(foundation::Vec3 value) noexcept { return dot(value, value); }

[[nodiscard]] foundation::Vec3 normalized(foundation::Vec3 value) noexcept {
    const float length = std::sqrt(std::max(kEpsilon, length_squared(value)));
    return multiply(value, 1.0F / length);
}

[[nodiscard]] Quaternion rotation_from_forward(foundation::Vec3 direction) noexcept {
    const foundation::Vec3 forward{0.0F, 0.0F, 1.0F};
    const foundation::Vec3 target = normalized(direction);
    const float cosine = std::clamp(dot(forward, target), -1.0F, 1.0F);
    if (cosine > 1.0F - kEpsilon) {
        return {};
    }
    if (cosine < -1.0F + kEpsilon) {
        return {0.0F, 0.0F, 1.0F, 0.0F};
    }
    const foundation::Vec3 axis = cross(forward, target);
    const float scale = std::sqrt(2.0F * (1.0F + cosine));
    return Quaternion{scale * 0.5F, axis.x / scale, axis.y / scale, axis.z / scale}
        .normalized();
}

[[nodiscard]] Quaternion multiply(Quaternion left, Quaternion right) noexcept {
    return {left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z,
            left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
            left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
            left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w};
}

[[nodiscard]] foundation::Vec3 rotate(Quaternion orientation,
                                      foundation::Vec3 value) noexcept {
    const Quaternion vector_quaternion{0.0F, value.x, value.y, value.z};
    const Quaternion conjugate{orientation.w, -orientation.x, -orientation.y, -orientation.z};
    const Quaternion result = multiply(multiply(orientation.normalized(), vector_quaternion),
                                       conjugate);
    return {result.x, result.y, result.z};
}

[[nodiscard]] bool finite(float value) noexcept { return std::isfinite(value); }

} // namespace

bool Quaternion::valid() const noexcept {
    return finite(w) && finite(x) && finite(y) && finite(z) &&
           (w * w + x * x + y * y + z * z) > kEpsilon * kEpsilon;
}

Quaternion Quaternion::normalized() const noexcept {
    if (!valid()) {
        return {};
    }
    const float length = std::sqrt(w * w + x * x + y * y + z * z);
    return {w / length, x / length, y / length, z / length};
}

bool ProjectileEnergyLedger::valid() const noexcept {
    return finite(material_work) && finite(contact_loss) && finite(target_work) &&
           finite(flight_work) && finite(fragment_energy) && finite(unrepresented_energy) &&
           finite(rotation_work) && finite(explosive_energy) && finite(blast_energy) &&
           material_work >= 0.0F && contact_loss >= 0.0F && fragment_energy >= 0.0F &&
           unrepresented_energy >= 0.0F && blast_energy >= 0.0F;
}

bool ProjectileState::valid() const noexcept {
    return version == ProjectileStateVersion && projectile_id.isValid() && shot_id.isValid() &&
           trace_id.isValid() && ammunition_id.isValid() && strategy_id.isValid() &&
           finite(position) && finite(velocity) && orientation.valid() && finite(body_forward) &&
           finite(angular_velocity) && finite(inertia) && finite(inverse_inertia) &&
           finite(mass_kg) && mass_kg > 0.0F && finite(diameter_m) && diameter_m > 0.0F &&
           finite(drag_diameter_m) && drag_diameter_m > 0.0F && finite(integrity) &&
           finite(deformation) && finite(stability) && integrity >= 0.0F && integrity <= 1.0F &&
           deformation >= 0.0F && deformation <= 1.0F && stability >= 0.0F && stability <= 1.0F &&
           finite(age_seconds) && age_seconds >= 0.0F && finite(travel_distance_m) &&
           travel_distance_m >= 0.0F && energy.valid();
}

foundation::Result<ProjectileState, foundation::Error> ProjectileState::create(
    const FireRequest& request, const AmmunitionCatalog& catalog) {
    if (!request.projectile_id.isValid() || !request.shot_id.isValid() ||
        !request.trace_id.isValid() || !request.ammunition_id.isValid() || !finite(request.position) ||
        !finite(request.direction) || length_squared(request.direction) <= kEpsilon * kEpsilon) {
        return foundation::Result<ProjectileState, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid projectile fire request"});
    }
    if (!catalog.frozen()) {
        return foundation::Result<ProjectileState, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "ammunition catalog is not frozen"});
    }
    const AmmunitionDefinition* ammunition = catalog.find(request.ammunition_id);
    if (ammunition == nullptr) {
        return foundation::Result<ProjectileState, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "ammunition definition not found"});
    }
    const AmmunitionStrategy* strategy = catalog.strategy(ammunition->strategy_id_value);
    if (strategy == nullptr) {
        return foundation::Result<ProjectileState, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "ammunition strategy not found"});
    }

    ProjectileState state{};
    state.projectile_id = request.projectile_id;
    state.shot_id = request.shot_id;
    state.trace_id = request.trace_id;
    state.parent_trace_id = request.parent_trace_id;
    state.ammunition_id = ammunition->id;
    state.strategy_id = strategy->id;
    state.seed = request.seed;
    state.fragment = request.fragment;
    state.position = request.position;
    state.body_forward = normalized(request.direction);
    state.orientation = rotation_from_forward(state.body_forward);
    state.velocity = request.velocity_override.has_value()
                         ? request.velocity_override.value()
                         : multiply(state.body_forward, ammunition->muzzle_velocity_mps);
    state.mass_kg = request.mass_override_kg.value_or(ammunition->mass_kg);
    state.diameter_m = request.diameter_override_m.value_or(ammunition->diameter_m);
    state.drag_diameter_m = request.drag_diameter_override_m.value_or(
        request.diameter_override_m.value_or(ammunition->drag_diameter_m));
    if (!finite(state.velocity) || length_squared(state.velocity) <= kEpsilon * kEpsilon) {
        return foundation::Result<ProjectileState, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid projectile velocity override"});
    }
    state.refreshInertia();
    if (!state.valid()) {
        return foundation::Result<ProjectileState, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "ammunition produced invalid projectile state"});
    }
    return foundation::Result<ProjectileState, foundation::Error>::success(std::move(state));
}

foundation::Result<void, foundation::Error> ProjectileState::apply(
    const ImpactTransition& transition) {
    if (!finite(transition.velocity_delta) || !finite(transition.angular_impulse) ||
        !finite(transition.integrity_delta) || !finite(transition.deformation_delta) ||
        !finite(transition.stability_delta) || !finite(transition.mass_scale) ||
        !finite(transition.diameter_scale) || !finite(transition.drag_diameter_scale) ||
        !finite(transition.material_work) || !finite(transition.contact_loss) ||
        !finite(transition.target_work) || !finite(transition.fragment_energy) ||
        !finite(transition.unrepresented_energy) || transition.mass_scale <= 0.0F ||
        transition.diameter_scale <= 0.0F || transition.drag_diameter_scale <= 0.0F ||
        transition.material_work < 0.0F || transition.contact_loss < 0.0F ||
        transition.fragment_energy < 0.0F || transition.unrepresented_energy < 0.0F) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid projectile impact transition"});
    }
    const float previous_rotational_energy = rotationalEnergy();
    mass_kg *= transition.mass_scale;
    diameter_m *= transition.diameter_scale;
    drag_diameter_m *= transition.drag_diameter_scale;
    integrity = std::clamp(integrity + transition.integrity_delta, 0.0F, 1.0F);
    deformation = std::clamp(deformation + transition.deformation_delta, 0.0F, 1.0F);
    stability = std::clamp(stability + transition.stability_delta, 0.0F, 1.0F);
    velocity = add(velocity, transition.velocity_delta);
    refreshInertia();
    if (previous_rotational_energy > kEpsilon) {
        const float new_rotational = rotationalEnergy();
        if (new_rotational > kEpsilon) {
            angular_velocity = multiply(angular_velocity,
                                        std::sqrt(previous_rotational_energy / new_rotational));
        }
    }
    angular_velocity = add(angular_velocity,
                           {transition.angular_impulse.x * inverse_inertia.x,
                            transition.angular_impulse.y * inverse_inertia.y,
                            transition.angular_impulse.z * inverse_inertia.z});
    energy.material_work += transition.material_work;
    energy.contact_loss += transition.contact_loss;
    energy.target_work += transition.target_work;
    energy.fragment_energy += transition.fragment_energy;
    energy.unrepresented_energy += transition.unrepresented_energy;
    ricochet_count += transition.ricochet_count_delta;
    ++impact_index;
    body_forward = normalized(rotate(orientation, {0.0F, 0.0F, 1.0F}));
    return valid() ? foundation::Result<void, foundation::Error>::success()
                   : foundation::Result<void, foundation::Error>::failure(
                         {foundation::ErrorCode::InvalidState,
                          "projectile impact transition produced invalid state"});
}

foundation::Result<void, foundation::Error> ProjectileState::advanceAge(
    std::uint64_t ticks, float seconds, float distance_m) noexcept {
    if (!finite(seconds) || !finite(distance_m) || seconds < 0.0F || distance_m < 0.0F) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid projectile age advance"});
    }
    const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
    age_ticks = ticks > maximum - age_ticks ? maximum : age_ticks + ticks;
    age_seconds += seconds;
    travel_distance_m += distance_m;
    return valid() ? foundation::Result<void, foundation::Error>::success()
                   : foundation::Result<void, foundation::Error>::failure(
                         {foundation::ErrorCode::InvalidState,
                          "projectile age advance produced invalid state"});
}

foundation::Result<void, foundation::Error> ProjectileState::setVelocity(
    foundation::Vec3 next_velocity) noexcept {
    if (!finite(next_velocity) || length_squared(next_velocity) <= kEpsilon * kEpsilon) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid projectile outgoing velocity"});
    }
    velocity = next_velocity;
    body_forward = normalized(next_velocity);
    orientation = rotation_from_forward(body_forward);
    return foundation::Result<void, foundation::Error>::success();
}

float ProjectileState::rotationalEnergy() const noexcept {
    return 0.5F * (inertia.x * angular_velocity.x * angular_velocity.x +
                   inertia.y * angular_velocity.y * angular_velocity.y +
                   inertia.z * angular_velocity.z * angular_velocity.z);
}

float ProjectileState::translationalEnergy() const noexcept {
    return 0.5F * mass_kg * length_squared(velocity);
}

float ProjectileState::energyBalanceError(float initial_translational,
                                          float initial_rotational) const noexcept {
    return initial_translational + initial_rotational + energy.flight_work +
           energy.rotation_work + energy.target_work + energy.explosive_energy -
           translationalEnergy() - rotationalEnergy() - energy.material_work -
           energy.contact_loss - energy.fragment_energy - energy.unrepresented_energy -
           energy.blast_energy;
}

void ProjectileState::refreshInertia() noexcept {
    const float radius = std::max(kEpsilon, diameter_m * 0.5F);
    const float scalar = std::max(kEpsilon, mass_kg * radius * radius * 0.25F);
    inertia = {scalar, scalar, scalar};
    inverse_inertia = {1.0F / scalar, 1.0F / scalar, 1.0F / scalar};
}

bool AmmunitionStrategy::valid() const noexcept {
    return id.isValid() && caliber_id.isValid() && variant_id.isValid() && finite(drag_coefficient) &&
           finite(contact_work_scale) && finite(ricochet_threshold) &&
           finite(breakup_energy_threshold) && finite(fragment_mass_fraction) &&
           drag_coefficient >= 0.0F && contact_work_scale > 0.0F && ricochet_threshold >= 0.0F &&
           breakup_energy_threshold >= 0.0F && fragment_mass_fraction >= 0.0F &&
           fragment_mass_fraction <= 1.0F && max_fragments <= 1024U &&
           std::isfinite(nose_crush_work_j) && nose_crush_work_j >= 0.0F &&
           fragmentation.valid() && version > 0;
}

BreakupPlan AmmunitionStrategy::breakupPlan(foundation::StableId projectile_id,
                                             std::uint32_t impact_index,
                                             float available_energy) const {
    BreakupPlan result{};
    if (!explosive || max_fragments == 0 || available_energy < breakup_energy_threshold ||
        projectile_id == 0) {
        return result;
    }
    result.detonated = true;
    const std::uint32_t count = std::min<std::uint32_t>(max_fragments, 8U);
    result.pieces.reserve(count);
    const std::uint64_t base = foundation::stableHashCombine(
        foundation::stableHashCombine(projectile_id, impact_index), id.value());
    const float mass_fraction = fragment_mass_fraction / static_cast<float>(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint64_t hash = foundation::stableHashCombine(base, index + 1U);
        const float x = static_cast<float>(static_cast<std::int32_t>(hash & 0xFFFFU) - 32768) /
                        32768.0F;
        const float y = static_cast<float>(static_cast<std::int32_t>((hash >> 16U) & 0xFFFFU) -
                                           32768) /
                        32768.0F;
        const float z = static_cast<float>(static_cast<std::int32_t>((hash >> 32U) & 0xFFFFU) -
                                           32768) /
                        32768.0F;
        result.pieces.push_back(
            {foundation::stableHashCombine(projectile_id, index + 1U),
             mass_fraction,
             mass_fraction,
             {x * 0.15F, y * 0.15F, z * 0.15F}});
    }
    return result;
}

bool AmmunitionDefinition::valid() const noexcept {
    return id.isValid() && strategy_id_value.isValid() && caliber_id_value.isValid() &&
           variant_id_value.isValid() && finite(mass_kg) && finite(diameter_m) &&
           finite(explosive_energy_j) &&
           finite(drag_diameter_m) && finite(muzzle_velocity_mps) && finite(inertia_factor) &&
           mass_kg > 0.0F && diameter_m > 0.0F && drag_diameter_m > 0.0F &&
           muzzle_velocity_mps > 0.0F && inertia_factor > 0.0F && explosive_energy_j >= 0.0F &&
           version > 0;
}

foundation::Result<void, foundation::Error> AmmunitionCatalog::add(
    AmmunitionDefinition definition, AmmunitionStrategy strategy) {
    if (frozen_) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "ammunition catalog is frozen"});
    }
    if (!definition.valid() || !strategy.valid() || definition.strategy_id_value != strategy.id ||
        definition.caliber_id_value != strategy.caliber_id ||
        definition.variant_id_value != strategy.variant_id) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid ammunition catalog entry"});
    }
    for (const Entry& entry : entries_) {
        if (entry.definition.id == definition.id || entry.strategy.id == strategy.id) {
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument, "duplicate ammunition catalog ID"});
        }
    }
    entries_.push_back({definition, strategy});
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> AmmunitionCatalog::freeze() {
    if (entries_.empty()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "cannot freeze empty ammunition catalog"});
    }
    std::sort(entries_.begin(), entries_.end(), [](const Entry& left, const Entry& right) {
        return left.definition.id < right.definition.id;
    });
    frozen_ = true;
    return foundation::Result<void, foundation::Error>::success();
}

const AmmunitionDefinition* AmmunitionCatalog::find(AmmunitionId id) const noexcept {
    const auto iterator = std::lower_bound(
        entries_.begin(), entries_.end(), id, [](const Entry& entry, AmmunitionId value) {
            return entry.definition.id < value;
        });
    return iterator != entries_.end() && iterator->definition.id == id ? &iterator->definition
                                                                         : nullptr;
}

const AmmunitionStrategy* AmmunitionCatalog::strategy(StrategyId id) const noexcept {
    const auto iterator = std::find_if(entries_.begin(), entries_.end(), [id](const Entry& entry) {
        return entry.strategy.id == id;
    });
    return iterator == entries_.end() ? nullptr : &iterator->strategy;
}

} // namespace genomes::ballistics
