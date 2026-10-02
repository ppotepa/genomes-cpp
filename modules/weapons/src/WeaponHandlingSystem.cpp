#include <genomes/weapons/WeaponHandlingSystem.hpp>

#include <algorithm>
#include <cmath>

namespace genomes::weapons {

namespace {

[[nodiscard]] bool finite(float value) noexcept { return std::isfinite(value); }

[[nodiscard]] bool finite_vec3(foundation::Vec3 value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

[[nodiscard]] foundation::Vec3 add(foundation::Vec3 a, foundation::Vec3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] foundation::Vec3 subtract(foundation::Vec3 a, foundation::Vec3 b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

[[nodiscard]] foundation::Vec3 normalize(foundation::Vec3 value) noexcept {
    const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    if (!(length > 1.0e-6F) || !finite(length)) {
        return {0.0F, 0.0F, 1.0F};
    }
    return {value.x / length, value.y / length, value.z / length};
}

[[nodiscard]] foundation::Vec3 aimDirection(float yaw, float pitch) noexcept {
    const float horizontal = std::cos(pitch);
    return normalize({std::sin(yaw) * horizontal, std::sin(pitch), std::cos(yaw) * horizontal});
}

} // namespace

bool WeaponRuntimeState::valid() const noexcept {
    return (selected_weapon == 0U || WeaponCatalog::find(selected_weapon) != nullptr) &&
           (active_weapon == 0U || WeaponCatalog::find(active_weapon) != nullptr) &&
           finite(transition_seconds) && transition_seconds >= 0.0F &&
           finite(readiness) && readiness >= 0.0F && readiness <= 1.0F &&
           finite(requested_readiness) && requested_readiness >= 0.0F && requested_readiness <= 1.0F &&
           finite(support_weight) && support_weight >= 0.0F && support_weight <= 1.0F &&
           finite(recoil_offset) && recoil_offset >= 0.0F && finite(aim_yaw) &&
           aim_yaw >= -1.5F && aim_yaw <= 1.5F && finite(aim_pitch) && aim_pitch >= -1.0F &&
           aim_pitch <= 1.0F;
}

bool HandPoseTask::valid() const noexcept {
    return finite_vec3(target) && finite(weight) && weight >= 0.0F && weight <= 1.0F && finite(curl) &&
           curl >= 0.0F && curl <= 1.0F;
}

bool WeaponPoseTasks::valid() const noexcept {
    return (weapon_id == 0U || WeaponCatalog::find(weapon_id) != nullptr) && primary.valid() &&
           support.valid() && finite_vec3(muzzle) && finite_vec3(aim_direction) &&
           finite(readiness) && readiness >= 0.0F && readiness <= 1.0F && finite(recoil) &&
           recoil >= 0.0F;
}

bool FireIntent::valid() const noexcept {
    return entity != 0U && weapon_id != 0U && ammunition_id != 0U && shot_sequence > 0U &&
           finite_vec3(origin) && finite_vec3(direction) &&
           std::abs(std::sqrt(direction.x * direction.x + direction.y * direction.y +
                               direction.z * direction.z) - 1.0F) < 1.0e-3F;
}

bool WeaponLocomotionView::valid() const noexcept {
    return finite(speed_mps) && speed_mps >= 0.0F;
}

foundation::Result<void, foundation::Error> WeaponHandlingSystem::select(
    WeaponRuntimeState& state, WeaponId weapon) const noexcept {
    if (!state.valid() || (weapon != 0U && WeaponCatalog::find(weapon) == nullptr)) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid weapon selection"});
    }
    if (state.selected_weapon == weapon) {
        return foundation::Result<void, foundation::Error>::success();
    }
    state.selected_weapon = weapon;
    if (state.active_weapon != 0U) {
        state.handling = WeaponHandlingState::Holstering;
        state.transition_seconds = 0.0F;
    } else if (weapon != 0U) {
        state.handling = WeaponHandlingState::Drawing;
        state.transition_seconds = 0.0F;
    } else {
        state.handling = WeaponHandlingState::Stowed;
    }
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> WeaponHandlingSystem::requestReadiness(
    WeaponRuntimeState& state, float readiness) const noexcept {
    if (!state.valid() || !finite(readiness)) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid weapon readiness"});
    }
    state.requested_readiness = std::clamp(readiness, 0.0F, 1.0F);
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> WeaponHandlingSystem::step(
    WeaponRuntimeState& state, const WeaponHandlingInput& input, foundation::SimulationTick tick,
    float fixed_dt_seconds, WeaponStepOutput& output) const noexcept {
    if (!state.valid() || input.entity == 0U || input.definition == nullptr ||
        input.artifact == nullptr || !input.definition->valid() ||
        input.artifact->weapon_id != input.definition->id || !input.artifact->valid(*input.definition) ||
        !finite_vec3(input.root_position) || !input.locomotion.valid() ||
        (input.world_aim_target.has_value() && !finite_vec3(input.world_aim_target.value())) ||
        !finite(fixed_dt_seconds) || fixed_dt_seconds <= 0.0F || fixed_dt_seconds > 0.25F) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid weapon handling input"});
    }
    if (state.selected_weapon == 0U) {
        state.selected_weapon = input.definition->id;
        if (state.active_weapon == 0U) {
            state.handling = WeaponHandlingState::Drawing;
            state.transition_seconds = 0.0F;
        }
    }
    if (state.active_weapon != 0U && state.active_weapon != state.selected_weapon &&
        state.handling == WeaponHandlingState::Held) {
        state.handling = WeaponHandlingState::Holstering;
        state.transition_seconds = 0.0F;
    }

    state.transition_seconds += fixed_dt_seconds;
    if (state.handling == WeaponHandlingState::Drawing &&
        state.transition_seconds >= draw_seconds_) {
        state.active_weapon = state.selected_weapon;
        state.handling = WeaponHandlingState::Held;
        state.transition_seconds = 0.0F;
    } else if (state.handling == WeaponHandlingState::Holstering &&
               state.transition_seconds >= holster_seconds_) {
        state.active_weapon = 0U;
        state.handling = WeaponHandlingState::Stowed;
        state.transition_seconds = 0.0F;
        if (state.selected_weapon != 0U) {
            state.handling = WeaponHandlingState::Drawing;
        }
    }

    float yaw = state.aim_yaw;
    float pitch = state.aim_pitch;
    if (input.world_aim_target.has_value()) {
        const foundation::Vec3 direction = subtract(input.world_aim_target.value(), input.root_position);
        const float horizontal = std::sqrt(direction.x * direction.x + direction.z * direction.z);
        if (horizontal > 1.0e-5F) {
            yaw = std::clamp(std::atan2(direction.x, direction.z), -1.25F, 1.25F);
            pitch = std::clamp(std::atan2(direction.y, horizontal), -0.75F, 0.75F);
        }
    }
    state.aim_yaw = yaw;
    state.aim_pitch = pitch;
    const bool body_forbids_ready = input.locomotion.sprinting || input.locomotion.prone ||
                                    input.locomotion.resting || state.handling != WeaponHandlingState::Held;
    const float readiness_target = body_forbids_ready ? 0.0F : state.requested_readiness;
    const float readiness_alpha = 1.0F - std::exp(-12.0F * fixed_dt_seconds);
    state.readiness += (readiness_target - state.readiness) * readiness_alpha;
    const bool two_handed = input.definition->grip != WeaponGrip::OneHanded;
    const float support_target = state.handling == WeaponHandlingState::Held && two_handed &&
                                         !input.locomotion.sprinting
                                     ? 1.0F
                                     : 0.0F;
    state.support_weight += (support_target - state.support_weight) * readiness_alpha;
    state.recoil_offset *= std::exp(-10.0F * fixed_dt_seconds);

    output = {};
    output.pose.weapon_id = input.definition->id;
    output.pose.primary = {HandOwnership::Primary,
                           add(input.root_position, input.artifact->primary_grip.local_position),
                           state.readiness, 1.0F, state.handling == WeaponHandlingState::Held};
    output.pose.support = {two_handed ? HandOwnership::Support : HandOwnership::Free,
                           add(input.root_position, input.artifact->support_grip.local_position),
                           state.support_weight, 1.0F, two_handed && state.handling == WeaponHandlingState::Held};
    output.pose.muzzle = add(input.root_position, input.artifact->muzzle.local_position);
    output.pose.aim_direction = aimDirection(state.aim_yaw, state.aim_pitch);
    output.pose.readiness = state.readiness;
    output.pose.recoil = state.recoil_offset;

    if (input.request_fire && input.definition->firearm && state.handling == WeaponHandlingState::Held &&
        state.readiness >= 0.72F && !input.locomotion.sprinting && !input.locomotion.prone &&
        tick >= state.next_fire) {
        const std::uint64_t cadence = std::max<std::uint64_t>(
            1U, static_cast<std::uint64_t>(std::ceil(60.0 / input.definition->rounds_per_second)));
        state.next_fire = foundation::SimulationTick{tick.value + cadence};
        ++state.shot_sequence;
        state.recoil_offset = std::min(1.0F, state.recoil_offset + 0.34F);
        output.pose.recoil = state.recoil_offset;
        output.fire = FireIntent{input.entity, input.definition->id, input.definition->ammunition_id,
                                 state.shot_sequence, output.pose.muzzle, output.pose.aim_direction, tick};
    }
    return output.pose.valid() && (!output.fire.has_value() || output.fire->valid()) && state.valid()
               ? foundation::Result<void, foundation::Error>::success()
               : foundation::Result<void, foundation::Error>::failure(
                     {foundation::ErrorCode::InvalidState, "weapon handling output is invalid"});
}

} // namespace genomes::weapons
