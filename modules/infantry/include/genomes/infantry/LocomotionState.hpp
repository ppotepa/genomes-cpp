#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace genomes::infantry {

enum class LocomotionFamily : std::uint8_t {
    Biped,
    Prone,
    Seated,
    Rest,
};

enum class BipedPreset : std::uint8_t {
    Idle,
    Walk,
    Run,
    Crouch,
    CrouchWalk,
};

// BipedPreset remains the compatibility shorthand. The reference animator
// uses the continuous request and these nine states as its public vocabulary.
enum class AnimationState : std::uint8_t {
    REST,
    IDLE,
    WALK,
    RUN,
    CROUCH,
    CROUCH_WALK,
    SITTING,
    PRONE,
    PRONE_MOVE,
};

using InfantryAnimationState = AnimationState;

enum class AnimationTransitionStage : std::uint8_t {
    None,
    Crouch,
    Support,
    Target,
};

struct AnimationTransitionProfile final {
    float locomotion_seconds{0.20F};
    float crouch_seconds{0.36F};
    float crouch_walk_seconds{0.40F};
    float sitting_seconds{0.60F};
    float prone_crouch_seconds{0.26F};
    float prone_support_seconds{0.38F};
    float prone_seconds{0.44F};
    float prone_exit_support_seconds{0.40F};
    float prone_exit_crouch_seconds{0.36F};
    float prone_exit_seconds{0.28F};

    [[nodiscard]] bool valid() const noexcept {
        const auto nonnegative = [](float value) {
            return std::isfinite(value) && value >= 0.0F;
        };
        return nonnegative(locomotion_seconds) && nonnegative(crouch_seconds) &&
               nonnegative(crouch_walk_seconds) && nonnegative(sitting_seconds) &&
               nonnegative(prone_crouch_seconds) && nonnegative(prone_support_seconds) &&
               nonnegative(prone_seconds) && nonnegative(prone_exit_support_seconds) &&
               nonnegative(prone_exit_crouch_seconds) && nonnegative(prone_exit_seconds);
    }
};

struct LocomotionMotionContext final {
    float speed_mps{0.0F};
    float move_angle{0.0F};
    float turn_rate{0.0F};
    bool turning{false};
    bool treadmill{false};
    float distance_m{0.0F};

    [[nodiscard]] bool valid() const noexcept {
        return std::isfinite(speed_mps) && speed_mps >= 0.0F &&
               std::isfinite(move_angle) && std::isfinite(turn_rate) &&
               std::isfinite(distance_m) && distance_m >= 0.0F;
    }
};

enum class LocomotionLimitReason : std::uint8_t {
    None,
    RequestedCrouchClamped,
    RequestedSpeedClamped,
    FamilyDoesNotMove,
};

struct LocomotionState final {
    LocomotionFamily family{LocomotionFamily::Biped};
    BipedPreset preset{BipedPreset::Idle};
    bool family_moving{false};
    float requested_crouch{0.0F};
    float requested_speed_mps{0.0F};
    float target_crouch{0.0F};
    float actual_crouch{0.0F};
    float crouch_velocity{0.0F};
    float target_speed_mps{0.0F};
    float actual_speed_mps{0.0F};
    float run_weight{0.0F};
    float sprint_weight{0.0F};
    double cycle_m{0.0};
    float duty{0.62F};
    float lift_m{0.0F};
    float amplitude{0.0F};
    float cadence{0.0F};
    double cycle_velocity{0.0};
    float duty_velocity{0.0F};
    float run_velocity{0.0F};
    float sprint_velocity{0.0F};
    float lift_velocity{0.0F};
    double phase{0.0};
    LocomotionLimitReason limit_reason{LocomotionLimitReason::None};

    AnimationState requested_state{AnimationState::IDLE};
    AnimationState active_state{AnimationState::IDLE};
    AnimationTransitionStage transition_stage{AnimationTransitionStage::None};
    float transition_progress{1.0F};
    float transition_stage_progress{1.0F};
    bool transition_active{false};
    bool animation_snap_requested{true};
    std::uint64_t animation_request_revision{1U};
    AnimationTransitionProfile transition_profile{};
    // Snapshot used by the currently pending request. The editable profile
    // may change while a transition is running, but that must not retime the
    // transition already in flight.
    AnimationTransitionProfile transition_request_profile{};

    float move_angle{0.0F};
    float turn_rate{0.0F};
    float frame_distance_m{0.0F};
    double distance_m{0.0};
    bool turning{false};
    bool treadmill{false};
    bool has_motion_context{false};
    // A stopped biped may finish the current late half-step so the authored
    // pose settles on a repeatable double-support phase.
    bool settling{false};

    [[nodiscard]] bool valid() const noexcept;
};

struct LocomotionRequest final {
    std::optional<float> crouch;
    std::optional<float> speed_mps;
    std::optional<LocomotionMotionContext> motion;
};

} // namespace genomes::infantry
