#include <genomes/infantry/FaceAnimation.hpp>

#include <genomes/proc/RandomStream.hpp>
#include <genomes/proc/SeedPath.hpp>

#include <algorithm>
#include <cmath>
#include <string_view>
#include <utility>

namespace genomes::infantry {

namespace {

[[nodiscard]] bool finite(float value) noexcept { return std::isfinite(value); }

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

[[nodiscard]] float eventValue(proc::Seed seed,
                               std::string_view event_kind,
                               std::uint64_t event_index,
                               float minimum,
                               float maximum) noexcept {
    const proc::SeedPath root(seed);
    const auto path = root.childStableId("face-event", foundation::stable_id(event_kind))
                          .child("index", event_index);
    proc::RandomStream random(path);
    return static_cast<float>(random.uniformRange(minimum, maximum));
}

} // namespace

ExpressionProfile ExpressionProfile::forExpression(FaceExpression expression) noexcept {
    ExpressionProfile profile{};
    profile.channels[static_cast<std::size_t>(FaceChannel::EyeOpen)] = 1.0F;
    switch (expression) {
    case FaceExpression::Neutral:
        break;
    case FaceExpression::Alert:
        profile.channels[static_cast<std::size_t>(FaceChannel::EyeOpen)] = 1.0F;
        profile.channels[static_cast<std::size_t>(FaceChannel::BrowOuterUp)] = 0.24F;
        break;
    case FaceExpression::Fear:
        profile.channels[static_cast<std::size_t>(FaceChannel::EyeOpen)] = 1.0F;
        profile.channels[static_cast<std::size_t>(FaceChannel::BrowInnerUp)] = 0.58F;
        profile.channels[static_cast<std::size_t>(FaceChannel::BrowOuterUp)] = 0.31F;
        profile.channels[static_cast<std::size_t>(FaceChannel::JawOpen)] = 0.18F;
        profile.channels[static_cast<std::size_t>(FaceChannel::MouthOpen)] = 0.36F;
        profile.channels[static_cast<std::size_t>(FaceChannel::MouthStretch)] = 0.15F;
        break;
    case FaceExpression::Anger:
        profile.channels[static_cast<std::size_t>(FaceChannel::EyeSquint)] = 0.34F;
        profile.channels[static_cast<std::size_t>(FaceChannel::BrowDown)] = 0.58F;
        profile.channels[static_cast<std::size_t>(FaceChannel::LipPress)] = 0.62F;
        profile.channels[static_cast<std::size_t>(FaceChannel::MouthCornerDown)] = 0.18F;
        break;
    case FaceExpression::Pain:
        profile.channels[static_cast<std::size_t>(FaceChannel::EyeSquint)] = 0.64F;
        profile.channels[static_cast<std::size_t>(FaceChannel::BrowInnerUp)] = 0.43F;
        profile.channels[static_cast<std::size_t>(FaceChannel::BrowDown)] = 0.14F;
        profile.channels[static_cast<std::size_t>(FaceChannel::JawOpen)] = 0.12F;
        profile.channels[static_cast<std::size_t>(FaceChannel::MouthOpen)] = 0.17F;
        profile.channels[static_cast<std::size_t>(FaceChannel::MouthStretch)] = 0.13F;
        profile.channels[static_cast<std::size_t>(FaceChannel::MouthCornerDown)] = 0.38F;
        profile.channels[static_cast<std::size_t>(FaceChannel::CheekRaise)] = 0.24F;
        break;
    case FaceExpression::Fatigue:
        profile.channels[static_cast<std::size_t>(FaceChannel::EyeOpen)] = 0.28F;
        profile.channels[static_cast<std::size_t>(FaceChannel::EyeSquint)] = 0.16F;
        profile.channels[static_cast<std::size_t>(FaceChannel::MouthOpen)] = 0.07F;
        profile.channels[static_cast<std::size_t>(FaceChannel::MouthCornerDown)] = 0.13F;
        break;
    case FaceExpression::EyesClosed:
        profile.channels[static_cast<std::size_t>(FaceChannel::EyeOpen)] = 0.0F;
        profile.channels[static_cast<std::size_t>(FaceChannel::EyeSquint)] = 1.0F;
        break;
    }
    return profile;
}

bool FaceState::valid() const noexcept {
    if (!finite(look_target) || !finite(root_position) || !finite(saccade_offset) ||
        !finite(blink_timer) || !finite(next_blink_seconds) || !finite(blink_time) ||
        !finite(saccade_timer) || !finite(next_saccade_seconds) || blink_timer < 0.0F ||
        next_blink_seconds <= 0.0F || saccade_timer < 0.0F || next_saccade_seconds <= 0.0F ||
        blink_time < -1.0F || blink_time > 1.0F) {
        return false;
    }
    for (const float value : expression_weights) {
        if (!finite(value) || value < 0.0F || value > 1.0F) {
            return false;
        }
    }
    for (const float value : channels) {
        if (!finite(value)) {
            return false;
        }
    }
    return true;
}

bool FaceOutput::valid() const noexcept {
    const auto finiteValue = [](float value) { return std::isfinite(value); };
    if (!finiteValue(eyelids_close) || !finiteValue(eyelids_arc) ||
        !finiteValue(neck_flex) || !finiteValue(hands_relax) ||
        !finiteValue(jaw_rotation) ||
        !finiteValue(head_yaw) || !finiteValue(head_pitch) || !finiteValue(eye_yaw) ||
        !finiteValue(eye_pitch)) {
        return false;
    }
    for (const float value : channels) {
        if (!finiteValue(value)) {
            return false;
        }
    }
    return true;
}

foundation::Result<FaceAnimator, foundation::Error> FaceAnimator::create(
    proc::Seed unit_seed, const FacePhenotype& identity) {
    if (!identity.valid()) {
        return foundation::Result<FaceAnimator, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid face phenotype identity"});
    }
    FaceAnimator animator(unit_seed, identity);
    animator.state_.next_blink_seconds = 2.0F +
                                         eventValue(unit_seed, "blink", 0U, 0.5F, 2.5F);
    animator.state_.next_saccade_seconds =
        0.35F + eventValue(unit_seed, "saccade", 0U, 0.1F, 0.8F);
    animator.evaluateOutput(0.0F);
    return foundation::Result<FaceAnimator, foundation::Error>::success(std::move(animator));
}

foundation::Result<void, foundation::Error> FaceAnimator::setExpression(
    FaceExpression expression, float weight) noexcept {
    if (!finite(weight)) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "nonfinite face expression weight"});
    }
    state_.expression_weights[static_cast<std::size_t>(expression)] = std::clamp(weight, 0.0F, 1.0F);
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> FaceAnimator::setLookTarget(
    std::optional<foundation::Vec3> world_target, foundation::Vec3 root_position) noexcept {
    if (!finite(root_position) || (world_target.has_value() && !finite(world_target.value()))) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "nonfinite face look target"});
    }
    state_.root_position = root_position;
    state_.has_look_target = world_target.has_value();
    if (world_target.has_value()) {
        state_.look_target = world_target.value();
    }
    return foundation::Result<void, foundation::Error>::success();
}

void FaceAnimator::clearExpressions() noexcept {
    state_.expression_weights.fill(0.0F);
    state_.expression_weights[static_cast<std::size_t>(FaceExpression::Neutral)] = 1.0F;
}

void FaceAnimator::updateBlink(float fixed_dt_seconds) noexcept {
    state_.blink_timer += fixed_dt_seconds;
    if (state_.blink_time >= 0.0F) {
        state_.blink_time += fixed_dt_seconds;
        if (state_.blink_time >= 0.14F) {
            state_.blink_time = -1.0F;
        }
    }
    if (state_.blink_time < 0.0F && state_.blink_timer >= state_.next_blink_seconds) {
        state_.blink_timer -= state_.next_blink_seconds;
        state_.blink_time = 0.0F;
        ++state_.blink_event_index;
        state_.next_blink_seconds = 2.0F + eventValue(state_.unit_seed, "blink",
                                                       state_.blink_event_index, 0.5F, 2.5F);
    }
}

void FaceAnimator::updateSaccade(float fixed_dt_seconds) noexcept {
    state_.saccade_timer += fixed_dt_seconds;
    if (state_.saccade_timer >= state_.next_saccade_seconds) {
        state_.saccade_timer -= state_.next_saccade_seconds;
        ++state_.saccade_event_index;
        state_.saccade_offset.x = eventValue(state_.unit_seed, "saccade-yaw",
                                             state_.saccade_event_index, -0.045F, 0.045F);
        state_.saccade_offset.y = eventValue(state_.unit_seed, "saccade-pitch",
                                             state_.saccade_event_index, -0.025F, 0.025F);
        state_.next_saccade_seconds =
            0.35F + eventValue(state_.unit_seed, "saccade", state_.saccade_event_index, 0.1F, 0.8F);
    }
    const float decay = 1.0F - std::exp(-8.0F * fixed_dt_seconds);
    state_.saccade_offset.x *= 1.0F - decay;
    state_.saccade_offset.y *= 1.0F - decay;
}

void FaceAnimator::evaluateOutput(float fixed_dt_seconds) noexcept {
    std::array<float, kFaceChannelCount> target{};
    target[static_cast<std::size_t>(FaceChannel::EyeOpen)] = 1.0F;
    for (std::size_t expression = 0U; expression < kFaceExpressionCount; ++expression) {
        const float expression_weight = state_.expression_weights[expression];
        const auto profile = ExpressionProfile::forExpression(
            static_cast<FaceExpression>(expression));
        for (std::size_t channel = 0U; channel < kFaceChannelCount; ++channel) {
            target[channel] += (profile.channels[channel] -
                                (channel == static_cast<std::size_t>(FaceChannel::EyeOpen) ? 1.0F : 0.0F)) *
                               expression_weight;
        }
    }
    const float blink = state_.blink_time < 0.0F
                            ? 0.0F
                            : std::sin(std::clamp(state_.blink_time / 0.14F, 0.0F, 1.0F) *
                                       3.14159265358979323846F);
    target[static_cast<std::size_t>(FaceChannel::EyeOpen)] =
        std::clamp(target[static_cast<std::size_t>(FaceChannel::EyeOpen)] - blink, 0.0F, 1.0F);
    target[static_cast<std::size_t>(FaceChannel::EyeSquint)] =
        std::clamp(target[static_cast<std::size_t>(FaceChannel::EyeSquint)] + blink, 0.0F, 1.0F);
    const float alpha = fixed_dt_seconds <= 0.0F ? 1.0F : 1.0F - std::exp(-16.0F * fixed_dt_seconds);
    for (std::size_t channel = 0U; channel < kFaceChannelCount; ++channel) {
        state_.channels[channel] += (std::clamp(target[channel], 0.0F, 1.0F) - state_.channels[channel]) * alpha;
        output_.channels[channel] = state_.channels[channel];
    }
    output_.eyelids_close = blink;
    output_.eyelids_arc = 4.0F * blink * (1.0F - blink);
    output_.hands_relax = 0.0F;
    output_.jaw_rotation = state_.channels[static_cast<std::size_t>(FaceChannel::JawOpen)] * 0.26F;

    float yaw = state_.saccade_offset.x;
    float pitch = state_.saccade_offset.y;
    if (state_.has_look_target) {
        const foundation::Vec3 local{state_.look_target.x - state_.root_position.x,
                                     state_.look_target.y - state_.root_position.y,
                                     state_.look_target.z - state_.root_position.z};
        const float horizontal = std::sqrt(local.x * local.x + local.z * local.z);
        if (horizontal > 1.0e-5F) {
            yaw += std::atan2(local.x, local.z);
            pitch += std::atan2(local.y, horizontal);
        }
    }
    yaw = std::clamp(yaw, -0.95F, 0.95F);
    pitch = std::clamp(pitch, -0.60F, 0.60F);
    output_.head_yaw = yaw * 0.35F;
    output_.head_pitch = pitch * 0.30F;
    output_.neck_flex = std::clamp(std::abs(output_.head_pitch) / 0.75F, 0.0F, 1.0F) * 0.35F;
    output_.eye_yaw = yaw * 0.65F;
    output_.eye_pitch = pitch * 0.70F;
}

foundation::Result<void, foundation::Error> FaceAnimator::step(
    float fixed_dt_seconds) noexcept {
    if (!std::isfinite(fixed_dt_seconds) || fixed_dt_seconds < 0.0F || fixed_dt_seconds > 0.25F ||
        !state_.valid()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid fixed face interval"});
    }
    updateBlink(fixed_dt_seconds);
    updateSaccade(fixed_dt_seconds);
    evaluateOutput(fixed_dt_seconds);
    return output_.valid() && state_.valid()
               ? foundation::Result<void, foundation::Error>::success()
               : foundation::Result<void, foundation::Error>::failure(
                     {foundation::ErrorCode::InvalidState, "face animation output is invalid"});
}

} // namespace genomes::infantry
