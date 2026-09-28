#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/infantry/AnimationLOD.hpp>
#include <genomes/infantry/FaceAnimation.hpp>
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

struct AnimationEntity final {
    foundation::StableId semantic_id{0};
    const SkeletonData* skeleton{nullptr};
    const LocomotionController* locomotion{nullptr};
    LocomotionState* locomotion_state{nullptr};
    FaceAnimator* face{nullptr};
    foundation::Vec3 root_position{};
    std::optional<foundation::Vec3> look_target;
    AnimationLODState lod{};

    [[nodiscard]] bool valid() const noexcept;
};

struct AnimationPose final {
    foundation::StableId semantic_id{0};
    foundation::Vec3 root_position{};
    PostureSample posture{};
    float locomotion_phase{0.0F};
    FaceOutput face{};
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
