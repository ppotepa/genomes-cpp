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

void spring(float& value, float& velocity, float target, float omega, float dt) noexcept {
    if (!(dt > 0.0F)) return;
    const float offset=value-target;
    const float t=(velocity+omega*offset)*dt;
    const float e=std::exp(-omega*dt);
    value=target+(offset+t)*e;
    velocity=(velocity-omega*t)*e;
}

void spring(double& value, double& velocity, double target, double omega, double dt) noexcept {
    if (!(dt > 0.0)) return;
    const double offset=value-target,t=(velocity+omega*offset)*dt,e=std::exp(-omega*dt);
    value=target+(offset+t)*e;velocity=(velocity-omega*t)*e;
}

} // namespace

bool LocomotionState::valid() const noexcept {
    const auto finite = [](float value) { return std::isfinite(value); };
    return finite(requested_crouch) && finite(requested_speed_mps) && finite(target_crouch) &&
           finite(actual_crouch) && finite(crouch_velocity) && finite(target_speed_mps) && finite(actual_speed_mps) &&
           finite(run_weight) && finite(sprint_weight) && finite(cycle_m) && finite(duty) &&
           finite(lift_m) && finite(amplitude) && finite(cadence) &&
           finite(cycle_velocity) && finite(duty_velocity) && finite(run_velocity) &&
           finite(sprint_velocity) && finite(lift_velocity) && finite(phase) &&
           requested_crouch >= 0.0F && requested_crouch <= 1.0F && target_crouch >= 0.0F &&
           target_crouch <= 1.0F && actual_crouch >= 0.0F && actual_crouch <= 1.0F &&
           requested_speed_mps >= 0.0F && target_speed_mps >= 0.0F && actual_speed_mps >= 0.0F &&
           run_weight >= 0.0F && run_weight <= 1.0F && sprint_weight >= 0.0F &&
           sprint_weight <= 1.0F && cycle_m >= 0.0F && duty >= 0.0F && duty <= 1.0F &&
           lift_m >= 0.0F && amplitude >= 0.0F && amplitude <= 1.0F && cadence >= 0.0F &&
           phase >= 0.0F && phase < 1.0F;
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
    LocomotionLimits limits{};
    limits.walk_speed_mps = body.walk_speed;
    limits.run_speed_mps = body.run_speed;
    limits.sprint_speed_mps = body.run_speed;
    limits.walk_cycle_seconds = body.anatomy_leg_length * 1.50F / body.walk_speed;
    limits.run_cycle_seconds = body.anatomy_leg_length * 2.50F / body.run_speed;
    limits.sprint_cycle_seconds = limits.run_cycle_seconds;
    const float bulky_legs=std::max(0.0F,body.leg_thickness_scale-1.13F);
    const float bulky_waist=std::max(0.0F,body.waist_depth_scale-1.13F);
    limits.max_crouch=std::clamp(1.0F-bulky_legs*.22F-bulky_waist*.16F,.88F,1.0F);
    return foundation::Result<LocomotionController, foundation::Error>::success(
        LocomotionController(limits, body));
}

LocomotionState LocomotionController::initialState() const noexcept {
    LocomotionState state{};
    const auto gait = PostureProfile::gait(0.0F, 0.0F, body_);
    state.cycle_m = gait.cycle_m;
    state.duty = gait.duty;
    state.lift_m = gait.lift_m;
    state.amplitude = gait.amplitude;
    state.cadence = gait.cadence;
    return state;
}

double LocomotionController::proneCycleMeters() const noexcept {
    const double speed_gene=std::clamp((static_cast<double>(body_.speed_multiplier)-.88)/.24,0.0,1.0);
    return .56*static_cast<double>(body_.anatomy_leg_length)*(.97+.06*speed_gene);
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
    LocomotionState& state, BipedPreset preset, bool immediate) const noexcept {
    if (!state.valid()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "invalid locomotion state"});
    }
    state.family = LocomotionFamily::Biped;
    state.family_moving = false;
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
        state.requested_crouch = 0.60F;
        state.requested_speed_mps = 0.0F;
        break;
    case BipedPreset::CrouchWalk:
        state.requested_crouch = 0.60F;
        state.requested_speed_mps = 0.75F * (limits_.walk_speed_mps / 1.4F);
        break;
    }
    if (immediate) {
        state.target_crouch = std::min(state.requested_crouch, limits_.max_crouch);
        state.actual_crouch = state.target_crouch;
        state.crouch_velocity = 0.0F;
        state.actual_speed_mps = std::min(
            state.requested_speed_mps,
            PostureProfile::speedLimit(state.actual_crouch, body_));
        state.target_speed_mps = state.actual_speed_mps;
        return sampleGait(state, state.actual_speed_mps, 0.0F, true);
    }
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> LocomotionController::sampleGait(
    LocomotionState& state, float actual_speed_mps, float fixed_dt_seconds,
    bool immediate) const noexcept {
    if (!state.valid() || !std::isfinite(actual_speed_mps) || actual_speed_mps < 0.0F ||
        !std::isfinite(fixed_dt_seconds) || fixed_dt_seconds < 0.0F ||
        fixed_dt_seconds > 0.25F) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid gait sample input"});
    }
    state.actual_speed_mps = actual_speed_mps;
    const auto target = PostureProfile::gait(state.actual_crouch, actual_speed_mps, body_);
    if (immediate) {
        state.cycle_m = target.cycle_m;
        state.duty = target.duty;
        state.run_weight = target.run;
        state.sprint_weight = target.sprint;
        state.lift_m = target.lift_m;
        state.cycle_velocity = state.duty_velocity = state.run_velocity = 0.0F;
        state.sprint_velocity = state.lift_velocity = 0.0F;
    } else {
        spring(state.cycle_m, state.cycle_velocity, target.cycle_m, 10.0F, fixed_dt_seconds);
        spring(state.duty, state.duty_velocity, target.duty, 8.0F, fixed_dt_seconds);
        spring(state.run_weight, state.run_velocity, target.run, 8.0F, fixed_dt_seconds);
        spring(state.sprint_weight, state.sprint_velocity, target.sprint, 10.0F,
               fixed_dt_seconds);
        spring(state.lift_m, state.lift_velocity, target.lift_m, 10.0F, fixed_dt_seconds);
    }
    state.duty = std::clamp(state.duty, 0.36F, 0.78F);
    state.run_weight = std::clamp(state.run_weight, 0.0F, 1.0F);
    state.sprint_weight = std::clamp(state.sprint_weight, 0.0F, 1.0F);
    state.lift_m = std::max(0.0F, state.lift_m);
    state.amplitude = target.amplitude;
    state.cadence = actual_speed_mps / state.cycle_m;
    return state.valid()
        ? foundation::Result<void, foundation::Error>::success()
        : foundation::Result<void, foundation::Error>::failure(
              {foundation::ErrorCode::InvalidState, "gait sample produced invalid state"});
}

foundation::Result<void, foundation::Error> LocomotionController::setFamily(
    LocomotionState& state, LocomotionFamily family, bool moving) const noexcept {
    if (!state.valid()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "invalid locomotion state"});
    }
    state.family = family;
    state.family_moving = moving && family == LocomotionFamily::Prone;
    if (family != LocomotionFamily::Biped) {
        state.preset = BipedPreset::Idle;
        state.requested_crouch = 0.0F;
        state.requested_speed_mps = state.family_moving ? body_.prone_speed : 0.0F;
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
    const bool moving_family = state.family == LocomotionFamily::Biped ||
        (state.family == LocomotionFamily::Prone && state.family_moving);
    if (!moving_family) {
        state.limit_reason = LocomotionLimitReason::FamilyDoesNotMove;
    }
    const float requested_speed = std::min(state.requested_speed_mps, limits_.sprint_speed_mps);
    if (requested_speed != state.requested_speed_mps && state.limit_reason == LocomotionLimitReason::None) {
        state.limit_reason = LocomotionLimitReason::RequestedSpeedClamped;
    }
    const float wanted=state.target_crouch;
    const float depth_target=wanted>state.actual_crouch
        ? std::min(wanted, std::max(state.actual_crouch,
                                   PostureProfile::depthAtSpeed(state.actual_speed_mps,body_)))
        : wanted;
    spring(state.actual_crouch,state.crouch_velocity,depth_target,9.0F,fixed_dt_seconds);
    state.target_speed_mps = state.family == LocomotionFamily::Biped
        ? std::min(requested_speed,
                   std::min(PostureProfile::speedLimit(wanted,body_),
                            PostureProfile::speedLimit(state.actual_crouch,body_)))
        : (moving_family ? requested_speed : 0.0F);
    state.actual_speed_mps = approach(state.actual_speed_mps, state.target_speed_mps, fixed_dt_seconds, 12.0F);
    if(state.family==LocomotionFamily::Biped){
        const auto sampled=sampleGait(state,state.actual_speed_mps,fixed_dt_seconds,false);
        if(!sampled)return sampled;
    }
    if (state.actual_speed_mps > 1.0e-4F && moving_family) {
        const double cycle=state.family==LocomotionFamily::Prone?proneCycleMeters():state.cycle_m;
        state.phase += state.actual_speed_mps*fixed_dt_seconds/std::max(0.01,cycle);
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
    return PostureProfile::sample(state.actual_crouch, body_);
}

} // namespace genomes::infantry
