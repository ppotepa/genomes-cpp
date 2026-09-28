#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/infantry/AnimationSystem.hpp>
#include <genomes/infantry/BodyPhenotype.hpp>
#include <genomes/infantry/SkeletonData.hpp>
#include <genomes/physics/PhysicsWorld.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace genomes::infantry {

struct RagdollBody final {
    BoneId bone{BoneId::Hips};
    physics::ShapeDesc shape{};
    foundation::Vec3 local_offset{};
    float mass{1.0F};
    std::uint32_t collision_layer{2U};
    std::uint32_t collision_mask{0xFFFF'FFFFu};
};

struct RagdollConstraint final {
    BoneId parent{BoneId::Hips};
    BoneId child{BoneId::SpineLower};
    float swing_limit_radians{0.6F};
    float twist_limit_radians{0.4F};
};

class RagdollSchema final {
public:
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] foundation::StableId cacheKey() const noexcept { return cache_key_; }
    [[nodiscard]] std::span<const RagdollBody> bodies() const noexcept { return bodies_; }
    [[nodiscard]] std::span<const RagdollConstraint> constraints() const noexcept {
        return constraints_;
    }
    [[nodiscard]] static foundation::Result<RagdollSchema, foundation::Error> build(
        const BodyPhenotype&, const SkeletonData&);

private:
    friend class RagdollCache;
    foundation::StableId cache_key_{0};
    std::vector<RagdollBody> bodies_;
    std::vector<RagdollConstraint> constraints_;
};

class RagdollCache final {
public:
    [[nodiscard]] const RagdollSchema* find(foundation::StableId key) const noexcept;
    void store(RagdollSchema schema);
    [[nodiscard]] std::size_t size() const noexcept { return schemas_.size(); }

private:
    std::vector<RagdollSchema> schemas_;
};

struct RagdollHandoff final {
    std::uint64_t simulation_tick{0};
    std::vector<physics::BodyHandle> bodies;
    std::vector<foundation::Vec3> linear_velocities;
    std::vector<foundation::Vec3> angular_velocities;
    bool active{false};
};

class RagdollController final {
public:
    [[nodiscard]] foundation::Result<void, foundation::Error> enter(
        physics::PhysicsWorld& world,
        const RagdollSchema& schema,
        const AnimationPose& previous_pose,
        const AnimationPose& current_pose,
        float fixed_dt_seconds,
        std::uint64_t simulation_tick);

    void dispose(physics::PhysicsWorld& world) noexcept;

    [[nodiscard]] const RagdollHandoff& state() const noexcept { return state_; }
    [[nodiscard]] bool active() const noexcept { return state_.active; }

private:
    RagdollHandoff state_{};
};

} // namespace genomes::infantry
