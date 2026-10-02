#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/infantry/AnimationLOD.hpp>
#include <genomes/infantry/AnimationRuntime.hpp>
#include <genomes/infantry/AnimationWeaponOverlay.hpp>
#include <genomes/infantry/AppearanceArtifact.hpp>
#include <genomes/infantry/FaceAnimation.hpp>
#include <genomes/infantry/GroundContact.hpp>
#include <genomes/infantry/LocomotionController.hpp>
#include <genomes/infantry/RigSchema.hpp>
#include <genomes/infantry/SkeletonData.hpp>
#include <genomes/jobs/JobSystem.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace genomes::infantry {

struct GearArtifact;

struct AnimationEntity final {
    foundation::StableId semantic_id{0};
    const SkeletonData* skeleton{nullptr};
    const LocomotionController* locomotion{nullptr};
    LocomotionState* locomotion_state{nullptr};
    AnimationTransitionRuntime* transition_runtime{nullptr};
    FaceAnimator* face{nullptr};
    foundation::Vec3 root_position{};
    std::optional<foundation::Vec3> look_target;
    // World-space seat point used by the authored seated sampler. It is an
    // input anchor, not a persistent physics constraint.
    std::optional<foundation::Vec3> seat_anchor;
    AnimationLODState lod{};
    const AppearanceMesh* surface{nullptr};
    const GearArtifact* gear{nullptr};
    const AnimationWeaponOverlay* weapon_overlay{nullptr};
    GroundSurfaceQuery ground_surface{};
    GroundContactRuntime* ground_runtime{nullptr};

    [[nodiscard]] bool valid() const noexcept;
};

struct AnimationPose final {
    foundation::StableId semantic_id{0};
    foundation::Vec3 root_position{};
    PostureSample posture{};
    float locomotion_phase{0.0F};
    std::array<foundation::Vec3, 2U> foot_targets{};
    // Final contact-adjusted ankle goals used by the second IK iteration.
    std::array<foundation::Vec3, 2U> foot_goals{};
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
    float target_hand_curl{0.0F};
    std::array<AnimationHandOwner, 2U> hand_owners{
        AnimationHandOwner::Free, AnimationHandOwner::Free};
    float weapon_readiness{0.0F};
    float weapon_recoil{0.0F};
    foundation::Vec3 weapon_aim_direction{0.0F, 0.0F, 1.0F};
    AnimationState requested_state{AnimationState::IDLE};
    AnimationState active_state{AnimationState::IDLE};
    AnimationTransitionStage transition_stage{AnimationTransitionStage::None};
    float transition_progress{1.0F};
    float transition_stage_progress{1.0F};
    FaceOutput face{};
    // Undamped sampler output retained for fixture diagnostics; `bones` is the
    // published pose after IK/contact placement.
    std::array<RigTransform, kRigBoneCount> target_bones{};
    std::array<RigTransform, kRigBoneCount> damped_bones{};
    std::array<RigTransform, kRigBoneCount> bones{};
    std::uint64_t revision{0};
    bool evaluated{false};

    [[nodiscard]] bool valid() const noexcept;
};

struct AnimationSnapshot final {
    std::uint64_t simulation_tick{0};
    std::uint64_t previous_simulation_tick{0};
    std::uint64_t pose_revision{0};
    std::vector<AnimationPose> poses;
};

struct AnimationEvaluationStats final {
    std::uint64_t simulation_tick{0};
    std::uint32_t entity_count{0};
    std::uint32_t due_count{0};
    std::uint32_t evaluated_count{0};
    std::uint32_t chunk_count{0};
    std::uint64_t pose_revision{0};
};

class AnimationSystem final {
public:
    [[nodiscard]] static foundation::Result<AnimationSystem, foundation::Error> create(
        std::size_t chunk_size = 64U);

    [[nodiscard]] foundation::Result<void, foundation::Error> evaluate(
        std::span<AnimationEntity> entities,
        std::uint64_t simulation_tick,
        float fixed_dt_seconds,
        jobs::JobSystem* jobs = nullptr);

    [[nodiscard]] const AnimationSnapshot& previousSnapshot() const noexcept {
        return previous_;
    }
    [[nodiscard]] const AnimationSnapshot& currentSnapshot() const noexcept { return current_; }
    [[nodiscard]] const AnimationEvaluationStats& lastStats() const noexcept { return stats_; }

    [[nodiscard]] static AnimationPose interpolate(const AnimationPose& previous,
                                                   const AnimationPose& current,
                                                   float alpha) noexcept;

private:
    explicit AnimationSystem(std::size_t chunk_size) noexcept : chunk_size_(chunk_size) {}

    std::size_t chunk_size_{64U};
    AnimationSnapshot previous_{};
    AnimationSnapshot current_{};
    AnimationEvaluationStats stats_{};
};

} // namespace genomes::infantry
