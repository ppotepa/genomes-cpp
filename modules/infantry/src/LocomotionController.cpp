#include <genomes/infantry/LocomotionController.hpp>

#include <algorithm>
#include <cmath>

namespace genomes::infantry {

namespace {

[[nodiscard]] float clampFinite(float value, float minimum, float maximum) noexcept {
    return std::clamp(value, minimum, maximum);
}

[[nodiscard]] float approach(float current, float target, float dt, float rate) noexcept {
    const float alpha = 1.0F - std::exp(-rate * dt);
    return current + (target - current) * alpha;
}

[[nodiscard]] float smoothstep(float value) noexcept {
    const float clamped = std::clamp(value, 0.0F, 1.0F);
    return clamped * clamped * (3.0F - 2.0F * clamped);
}

} // namespace

bool LocomotionState::valid() const noexcept {
    const auto finite = [](float value) { return std::isfinite(value); };
    return finite(requested_crouch) && finite(requested_speed_mps) && finite(target_crouch) &&
           finite(actual_crouch) && finite(target_speed_mps) && finite(actual_speed_mps) &&
           finite(run_weight) && finite(sprint_weight) && finite(phase) &&
           requested_crouch >= 0.0F && requested_crouch <= 1.0F && target_crouch >= 0.0F &&
           target_crouch <= 1.0F && actual_crouch >= 0.0F && actual_crouch <= 1.0F &&
           requested_speed_mps >= 0.0F && target_speed_mps >= 0.0F && actual_speed_mps >= 0.0F &&
           run_weight >= 0.0F && run_weight <= 1.0F && sprint_weight >= 0.0F &&
           sprint_weight <= 1.0F && phase >= 0.0F && phase < 1.0F;
}

bool LocomotionLimits::valid() const noexcept {
    return std::isfinite(walk_speed_mps) && std::isfinite(run_speed_mps) &&
           std::isfinite(sprint_speed_mps) && std::isfinite(walk_cycle_seconds) &&
           std::isfinite(run_cycle_seconds) && std::isfinite(sprint_cycle_seconds) &&
           std::isfinite(max_crouch) && walk_speed_mps > 0.0F && run_speed_mps > walk_speed_mps &&
           sprint_speed_mps >= run_speed_mps && walk_cycle_seconds > 0.0F &&
           run_cycle_seconds > 0.0F && sprint_cycle_seconds > 0.0F && max_crouch > 0.0F &&
           max_crouch <= 1.0F;
}

foundation::Result<LocomotionController, foundation::Error> LocomotionController::create(
    const BodyPhenotype& body) {
    if (!body.valid()) {
        return foundation::Result<LocomotionController, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid body phenotype for locomotion"});
    }
    const float height_scale = body.height / 1.75F;
    LocomotionLimits limits{};
    limits.walk_speed_mps = 1.55F * height_scale;
    limits.run_speed_mps = 3.25F * height_scale;
    limits.sprint_speed_mps = 4.65F * height_scale;
    limits.walk_cycle_seconds = 0.82F / height_scale;
    limits.run_cycle_seconds = 0.56F / height_scale;
    limits.sprint_cycle_seconds = 0.43F / height_scale;
    limits.max_crouch = std::clamp(0.72F + body.leg_length * 0.08F, 0.68F, 0.86F);
    return foundation::Result<LocomotionController, foundation::Error>::success(
        LocomotionController(limits));
}

foundation::Result<void, foundation::Error> LocomotionController::setRequested(
    LocomotionState& state, const LocomotionRequest& request) const noexcept {
    if (!state.valid()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "invalid locomotion state"});
    }
    if (request.crouch.has_value() && !std::isfinite(request.crouch.value())) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "nonfinite requested crouch"});
    }
    if (request.speed_mps.has_value() && !std::isfinite(request.speed_mps.value())) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "nonfinite requested speed"});
    }
    if (request.crouch.has_value()) {
        state.requested_crouch = clampFinite(request.crouch.value(), 0.0F, 1.0F);
    }
    if (request.speed_mps.has_value()) {
        state.requested_speed_mps = clampFinite(request.speed_mps.value(), 0.0F,
                                                limits_.sprint_speed_mps);
    }
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> LocomotionController::setPreset(
    LocomotionState& state, BipedPreset preset) const noexcept {
    if (!state.valid()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "invalid locomotion state"});
    }
    state.family = LocomotionFamily::Biped;
    state.preset = preset;
    switch (preset) {
    case BipedPreset::Idle:
        state.requested_crouch = 0.0F;
        state.requested_speed_mps = 0.0F;
        break;
    case BipedPreset::Walk:
        state.requested_crouch = 0.0F;
        state.requested_speed_mps = limits_.walk_speed_mps;
        break;
    case BipedPreset::Run:
        state.requested_crouch = 0.0F;
        state.requested_speed_mps = limits_.run_speed_mps;
        break;
    case BipedPreset::Crouch:
        state.requested_crouch = limits_.max_crouch;
        state.requested_speed_mps = 0.0F;
        break;
    case BipedPreset::CrouchWalk:
        state.requested_crouch = limits_.max_crouch;
        state.requested_speed_mps = limits_.walk_speed_mps * 0.58F;
        break;
    }
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> LocomotionController::setFamily(
    LocomotionState& state, LocomotionFamily family) const noexcept {
    if (!state.valid()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "invalid locomotion state"});
    }
    state.family = family;
    if (family != LocomotionFamily::Biped) {
        state.preset = BipedPreset::Idle;
    }
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> LocomotionController::step(
    LocomotionState& state, float fixed_dt_seconds) const noexcept {
    if (!state.valid() || !std::isfinite(fixed_dt_seconds) || fixed_dt_seconds < 0.0F ||
        fixed_dt_seconds > 0.25F) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid fixed locomotion interval"});
    }
    state.limit_reason = LocomotionLimitReason::None;
    const float requested_crouch = std::min(state.requested_crouch, limits_.max_crouch);
    if (requested_crouch != state.requested_crouch) {
        state.limit_reason = LocomotionLimitReason::RequestedCrouchClamped;
    }
    state.target_crouch = state.family == LocomotionFamily::Biped ? requested_crouch
                                                                    : (state.family == LocomotionFamily::Prone ? 1.0F : 0.35F);
    const bool moving_family = state.family == LocomotionFamily::Biped;
    if (!moving_family) {
        state.limit_reason = LocomotionLimitReason::FamilyDoesNotMove;
    }
    const float requested_speed = std::min(state.requested_speed_mps, limits_.sprint_speed_mps);
    if (requested_speed != state.requested_speed_mps && state.limit_reason == LocomotionLimitReason::None) {
        state.limit_reason = LocomotionLimitReason::RequestedSpeedClamped;
    }
    state.target_speed_mps = moving_family ? requested_speed * (1.0F - state.target_crouch * 0.38F) : 0.0F;
    state.actual_crouch = approach(state.actual_crouch, state.target_crouch, fixed_dt_seconds, 10.0F);
    state.actual_speed_mps = approach(state.actual_speed_mps, state.target_speed_mps, fixed_dt_seconds, 12.0F);
    const float walk_t = (state.actual_speed_mps - limits_.walk_speed_mps * 0.35F) /
                         std::max(0.01F, limits_.run_speed_mps - limits_.walk_speed_mps * 0.35F);
    const float sprint_t = (state.actual_speed_mps - limits_.run_speed_mps) /
                           std::max(0.01F, limits_.sprint_speed_mps - limits_.run_speed_mps);
    state.run_weight = smoothstep(walk_t);
    state.sprint_weight = smoothstep(sprint_t);
    if (state.actual_speed_mps > 1.0e-4F && moving_family) {
        const float cycle = limits_.walk_cycle_seconds * (1.0F - state.run_weight) +
                             limits_.run_cycle_seconds * state.run_weight;
        const float blended_cycle = cycle * (1.0F - state.sprint_weight) +
                                    limits_.sprint_cycle_seconds * state.sprint_weight;
        state.phase += fixed_dt_seconds / std::max(0.01F, blended_cycle);
        state.phase -= std::floor(state.phase);
    }
    state.actual_crouch = std::clamp(state.actual_crouch, 0.0F, 1.0F);
    state.actual_speed_mps = std::max(0.0F, state.actual_speed_mps);
    return state.valid()
               ? foundation::Result<void, foundation::Error>::success()
               : foundation::Result<void, foundation::Error>::failure(
                     {foundation::ErrorCode::InvalidState, "locomotion step produced invalid state"});
}

PostureSample LocomotionController::posture(const LocomotionState& state) const noexcept {
    return PostureProfile::sample(state.actual_crouch);
}

} // namespace genomes::infantry
