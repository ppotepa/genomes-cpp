#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/infantry/LocomotionState.hpp>
#include <genomes/infantry/PostureProfile.hpp>
#include <genomes/infantry/SkeletonData.hpp>

#include <array>
#include <cstdint>

namespace genomes::infantry {

// The body-only value transported by the transition graph. Face and weapon
// overlays deliberately live outside this value so a posture request cannot
// blend either overlay with data from an earlier frame.
struct AnimationBodyPose final {
    PostureSample posture{};
    std::array<RigTransform, kRigBoneCount> bones{};
    std::array<foundation::Vec3, 2U> foot_targets{};
    std::array<foundation::Vec3, 2U> knee_targets{};
    std::array<foundation::Vec3, 2U> hand_targets{};
    std::array<foundation::Vec3, 2U> elbow_targets{};
    std::array<float, 2U> foot_plant{};
    std::array<float, 2U> foot_support{};
    std::array<float, 2U> foot_pitch{};
    std::array<float, 2U> toe_pitch{};
    std::array<float, 2U> foot_yaw{};
    std::array<float, 2U> hand_plant{};
    std::array<float, 2U> hand_lift{};
    std::array<float, 2U> foot_relative{};
    std::array<float, 2U> ankle_pitch{};
    std::array<float, 2U> ankle_yaw{};
    float prone_weight{0.0F};
    float hand_ik_weight{0.0F};
    float gait_weight{0.0F};
    float hand_curl{0.0F};
};

struct AnimationTransitionStep final {
    AnimationTransitionStage stage{AnimationTransitionStage::None};
    AnimationState state{AnimationState::IDLE};
    float duration{0.0F};
};

struct AnimationTransitionRuntime final {
    AnimationBodyPose current{};
    AnimationBodyPose stage_from{};
    AnimationBodyPose stage_target{};
    std::array<AnimationTransitionStep, 3U> schedule{};
    AnimationTransitionProfile profile{};
    AnimationState settled_state{AnimationState::IDLE};
    AnimationState requested_state{AnimationState::IDLE};
    std::uint64_t handled_request_revision{0U};
    std::uint8_t stage_count{0U};
    std::uint8_t stage_index{0U};
    float stage_elapsed{0.0F};
    float total_elapsed{0.0F};
    float total_duration{0.0F};
    bool initialized{false};
    bool active{false};

    void reset() noexcept { *this = {}; }
};

} // namespace genomes::infantry
