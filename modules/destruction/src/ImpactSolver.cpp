#include <genomes/destruction/ImpactSolver.hpp>

#include <algorithm>
#include <cmath>

namespace genomes::destruction {

namespace {

constexpr float kPi = 3.14159265358979323846F;

[[nodiscard]] float dot(foundation::Vec3 left, foundation::Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] foundation::Vec3 add(foundation::Vec3 left,
                                   foundation::Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] foundation::Vec3 subtract(foundation::Vec3 left,
                                         foundation::Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] foundation::Vec3 scale(foundation::Vec3 value, float factor) noexcept {
    return {value.x * factor, value.y * factor, value.z * factor};
}

[[nodiscard]] foundation::Vec3 cross(foundation::Vec3 left,
                                     foundation::Vec3 right) noexcept {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

[[nodiscard]] float length_squared(foundation::Vec3 value) noexcept {
    return dot(value, value);
}

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] foundation::Vec3 normalized(foundation::Vec3 value) noexcept {
    const float length = std::sqrt(std::max(1.0e-12F, length_squared(value)));
    return scale(value, 1.0F / length);
}

[[nodiscard]] float resistanceScale(ImpactStrategy strategy,
                                    MaterialId material) noexcept {
    const MaterialId steel = MaterialId::fromName("steel");
    const MaterialId armor = MaterialId::fromName("armor");
    const MaterialId brick = MaterialId::fromName("brick");
    switch (strategy) {
    case ImpactStrategy::Ball:
        return 1.0F;
    case ImpactStrategy::ArmorPiercing:
        if (material == steel || material == armor) {
            return 0.68F;
        }
        if (material == brick) {
            return 0.50F;
        }
        return 0.78F;
    case ImpactStrategy::HighExplosive:
        return 1.10F;
    case ImpactStrategy::Fragment:
        return 1.20F;
    }
    return 1.0F;
}

[[nodiscard]] float rotationalEnergy(foundation::Vec3 angular,
                                     foundation::Vec3 inertia,
                                     const ImpactOrientation& orientation) noexcept {
    const foundation::Vec3 local = orientation.toLocal(angular);
    return 0.5F * (inertia.x * local.x * local.x + inertia.y * local.y * local.y +
                   inertia.z * local.z * local.z);
}

[[nodiscard]] foundation::Vec3 angularAfterImpulse(
    foundation::Vec3 angular,
    foundation::Vec3 torque,
    foundation::Vec3 inertia,
    const ImpactOrientation& orientation) noexcept {
    const foundation::Vec3 local_angular = orientation.toLocal(angular);
    const foundation::Vec3 local_torque = orientation.toLocal(torque);
    const foundation::Vec3 local_result{
        local_angular.x + local_torque.x / inertia.x,
        local_angular.y + local_torque.y / inertia.y,
        local_angular.z + local_torque.z / inertia.z};
    return orientation.toWorld(local_result);
}

[[nodiscard]] foundation::Result<ImpactResult, foundation::Error> invalid(
    std::string_view message) noexcept {
    return foundation::Result<ImpactResult, foundation::Error>::failure(
        {foundation::ErrorCode::InvalidArgument, message});
}

} // namespace

bool ImpactOrientation::valid(float tolerance) const noexcept {
    if (!finite(right) || !finite(up) || !finite(forward) || !std::isfinite(tolerance) ||
        tolerance <= 0.0F) {
        return false;
    }
    const auto near = [tolerance](float value) { return std::abs(value) <= tolerance; };
    return near(length_squared(right) - 1.0F) && near(length_squared(up) - 1.0F) &&
           near(length_squared(forward) - 1.0F) && near(dot(right, up)) &&
           near(dot(right, forward)) && near(dot(up, forward));
}

foundation::Vec3 ImpactOrientation::toLocal(foundation::Vec3 world) const noexcept {
    return {dot(world, right), dot(world, up), dot(world, forward)};
}

foundation::Vec3 ImpactOrientation::toWorld(foundation::Vec3 local) const noexcept {
    return {right.x * local.x + up.x * local.y + forward.x * local.z,
            right.y * local.x + up.y * local.y + forward.y * local.z,
            right.z * local.x + up.z * local.y + forward.z * local.z};
}

foundation::Result<ImpactResult, foundation::Error> ImpactSolver::solve(
    const ImpactInput& input, const MaterialCatalog& catalog) noexcept {
    if (!std::isfinite(input.mass_kg) || input.mass_kg <= 0.0F ||
        !std::isfinite(input.diameter_m) || input.diameter_m <= 0.0F ||
        !finite(input.projectile_velocity) || !finite(input.projectile_angular_velocity) ||
        !input.projectile_orientation.valid() || !finite(input.projectile_center) ||
        !finite(input.contact_point) || !finite(input.contact_normal) ||
        !finite(input.target_linear_velocity) || !finite(input.target_angular_velocity) ||
        !finite(input.target_center_of_mass) || input.assembly == nullptr ||
        !finite(input.entry_point) || !finite(input.exit_point) ||
        !std::isfinite(input.limits.stop_energy) || input.limits.stop_energy < 0.0F ||
        input.limits.max_intervals == 0 || input.limits.max_interfaces == 0 ||
        !static_cast<bool>(input.strategy_id)) {
        return invalid("invalid impact input");
    }
    if (!input.assembly->frame().valid()) {
        return invalid("impact assembly frame is invalid");
    }
    if (length_squared(input.contact_normal) <= 1.0e-12F) {
        return invalid("impact contact normal is zero");
    }
    const auto assembly_validation = input.assembly->validate(catalog);
    if (!assembly_validation) {
        return foundation::Result<ImpactResult, foundation::Error>::failure(
            assembly_validation.error());
    }

    ImpactResult result{};
    const foundation::Vec3 target_contact_velocity = add(
        input.target_linear_velocity,
        cross(input.target_angular_velocity,
              subtract(input.contact_point, input.target_center_of_mass)));
    const foundation::Vec3 relative_velocity =
        subtract(input.projectile_velocity, target_contact_velocity);
    const float relative_speed_squared = length_squared(relative_velocity);
    if (!std::isfinite(relative_speed_squared) || relative_speed_squared <= 1.0e-12F) {
        return invalid("impact relative velocity is zero");
    }
    result.relative_incoming_velocity = relative_velocity;

    const foundation::Vec3 projectile_direction = normalized(relative_velocity);
    const foundation::Vec3 contact_normal = normalized(input.contact_normal);
    const float path_length = std::sqrt(length_squared(subtract(input.exit_point,
                                                                input.entry_point)));
    if (!std::isfinite(path_length) || path_length <= 1.0e-6F) {
        return invalid("impact path is empty");
    }

    constexpr foundation::Vec3 inertia{0.25F, 0.25F, 0.25F};
    const float initial_translational = 0.5F * input.mass_kg * relative_speed_squared;
    const float initial_rotational = rotationalEnergy(input.projectile_angular_velocity,
                                                      inertia, input.projectile_orientation);
    float remaining_energy = initial_translational;
    result.energy.initial_translational = initial_translational;
    result.energy.initial_rotational = initial_rotational;

    const foundation::Vec3 local_entry = input.assembly->frame().worldToMaterial(input.entry_point);
    const foundation::Vec3 local_exit = input.assembly->frame().worldToMaterial(input.exit_point);
    const float normal_delta = local_exit.y - local_entry.y;
    const float normal_span = std::abs(normal_delta);
    std::uint64_t previous_solid = 0;
    bool has_previous_solid = false;
    const auto process_layer = [&](const Layer& layer, float interval_path) {
        if (interval_path <= 0.0F) {
            return;
        }
        ++result.intervals_visited;
        result.traversed_distance += interval_path;
        if (layer.isVoid()) {
            return;
        }
        if (!has_previous_solid || previous_solid != layer.physical_solid.value) {
            ++result.interfaces_visited;
            previous_solid = layer.physical_solid.value;
            has_previous_solid = true;
        }
        const MaterialDefinition* material = catalog.find(layer.material);
        if (material == nullptr) {
            remaining_energy = -1.0F;
            return;
        }
        constexpr float area_epsilon = 1.0e-8F;
        const float area = std::max(area_epsilon, kPi * input.diameter_m * input.diameter_m * 0.25F);
        const float pressure = material->penetration_work_j_m3 *
                               resistanceScale(input.strategy, material->id);
        const float work = std::max(0.0F, pressure * area * interval_path);
        const float consumed = std::min(remaining_energy, work);
        remaining_energy -= consumed;
        result.energy.material_work += consumed;
    };

    if (normal_span <= 1.0e-6F) {
        const auto layer_index = input.assembly->layerIndexAt(local_entry.y);
        if (layer_index.has_value()) {
            process_layer(input.assembly->layers()[*layer_index], path_length);
        } else {
            result.traversed_distance = path_length;
        }
    } else {
        const float low = std::min(local_entry.y, local_exit.y);
        const float high = std::max(local_entry.y, local_exit.y);
        const float direction = normal_delta >= 0.0F ? 1.0F : -1.0F;
        const auto& layers = input.assembly->layers();
        const auto visit = [&](std::size_t index) {
            const Layer& layer = layers[index];
            const float overlap_start = std::max(low, layer.start);
            const float overlap_end = std::min(high, layer.end);
            if (overlap_end <= overlap_start + 1.0e-6F) {
                return;
            }
            const float interval_path = (overlap_end - overlap_start) / normal_span * path_length;
            process_layer(layer, interval_path);
        };
        if (direction > 0.0F) {
            for (std::size_t index = 0; index < layers.size(); ++index) {
                if (result.intervals_visited >= input.limits.max_intervals ||
                    result.interfaces_visited > input.limits.max_interfaces || remaining_energy < 0.0F ||
                    remaining_energy <= input.limits.stop_energy) {
                    break;
                }
                visit(index);
            }
        } else {
            for (std::size_t index = layers.size(); index > 0; --index) {
                if (result.intervals_visited >= input.limits.max_intervals ||
                    result.interfaces_visited > input.limits.max_interfaces || remaining_energy < 0.0F ||
                    remaining_energy <= input.limits.stop_energy) {
                    break;
                }
                visit(index - 1);
            }
        }
    }

    if (remaining_energy < 0.0F) {
        return foundation::Result<ImpactResult, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "impact layer material is not in catalog"});
    }
    const bool budget_exhausted = result.intervals_visited >= input.limits.max_intervals ||
                                  result.interfaces_visited > input.limits.max_interfaces;
    result.budget_exhausted = budget_exhausted;
    if (budget_exhausted) {
        result.outcome = ImpactOutcome::BudgetExceeded;
    } else if (remaining_energy <= input.limits.stop_energy) {
        result.outcome = ImpactOutcome::Stopped;
        remaining_energy = 0.0F;
    } else if (result.energy.material_work > 0.0F) {
        result.outcome = ImpactOutcome::Penetrated;
    }

    result.residual_relative_energy = remaining_energy;
    const foundation::Vec3 outgoing_relative =
        scale(projectile_direction, std::sqrt(std::max(0.0F, 2.0F * remaining_energy /
                                                            input.mass_kg)));
    result.outgoing_velocity = add(outgoing_relative, target_contact_velocity);
    result.projectile_impulse = scale(subtract(outgoing_relative, relative_velocity), input.mass_kg);
    result.target_impulse = scale(result.projectile_impulse, -1.0F);
    const foundation::Vec3 projectile_lever =
        subtract(input.contact_point, input.projectile_center);
    const foundation::Vec3 projectile_torque = cross(projectile_lever, result.projectile_impulse);
    result.target_torque = cross(subtract(input.contact_point, input.target_center_of_mass),
                                 result.target_impulse);
    result.outgoing_angular_velocity = angularAfterImpulse(
        input.projectile_angular_velocity, projectile_torque, inertia,
        input.projectile_orientation);
    result.energy.final_translational = 0.5F * input.mass_kg * length_squared(outgoing_relative);
    result.energy.final_rotational = rotationalEnergy(result.outgoing_angular_velocity, inertia,
                                                      input.projectile_orientation);
    result.energy.target_work = dot(target_contact_velocity, result.projectile_impulse);
    result.energy.balance_error = result.energy.initial_translational +
                                  result.energy.initial_rotational + result.energy.target_work -
                                  result.energy.final_translational -
                                  result.energy.final_rotational - result.energy.material_work -
                                  result.energy.contact_loss;
    (void)contact_normal;
    return foundation::Result<ImpactResult, foundation::Error>::success(std::move(result));
}

} // namespace genomes::destruction
