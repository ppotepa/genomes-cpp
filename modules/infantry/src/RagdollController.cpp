#include <genomes/infantry/RagdollSchema.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace genomes::infantry {

namespace {

constexpr std::array<BoneId, 16> kRagdollBones{{
    BoneId::Hips, BoneId::SpineLower, BoneId::Chest, BoneId::Head,
    BoneId::UpperArmL, BoneId::ForeArmL, BoneId::HandL,
    BoneId::UpperArmR, BoneId::ForeArmR, BoneId::HandR,
    BoneId::ThighL, BoneId::ShinL, BoneId::FootL,
    BoneId::ThighR, BoneId::ShinR, BoneId::FootR,
}};

[[nodiscard]] bool finite(float value) noexcept { return std::isfinite(value); }

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

[[nodiscard]] foundation::Vec3 add(foundation::Vec3 a, foundation::Vec3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] foundation::Vec3 subtract(foundation::Vec3 a, foundation::Vec3 b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

[[nodiscard]] foundation::Vec3 divide(foundation::Vec3 value, float scalar) noexcept {
    return {value.x / scalar, value.y / scalar, value.z / scalar};
}

[[nodiscard]] bool containsBone(std::span<const RagdollBody> bodies, BoneId bone) noexcept {
    for (const RagdollBody& body : bodies) {
        if (body.bone == bone) {
            return true;
        }
    }
    return false;
}

} // namespace

bool RagdollSchema::valid() const noexcept {
    if (cache_key_ == 0U || bodies_.empty()) {
        return false;
    }
    for (const RagdollBody& body : bodies_) {
        if (!body.shape.valid() || !finite(body.local_offset) || !finite(body.mass) ||
            body.mass <= 0.0F || !containsBone(bodies_, body.bone)) {
            return false;
        }
    }
    for (const RagdollConstraint& constraint : constraints_) {
        if (constraint.parent == constraint.child || !containsBone(bodies_, constraint.parent) ||
            !containsBone(bodies_, constraint.child) || !finite(constraint.swing_limit_radians) ||
            !finite(constraint.twist_limit_radians) || constraint.swing_limit_radians <= 0.0F ||
            constraint.twist_limit_radians <= 0.0F) {
            return false;
        }
    }
    return true;
}

foundation::Result<RagdollSchema, foundation::Error> RagdollSchema::build(
    const BodyPhenotype& body, const SkeletonData& skeleton) {
    if (!body.valid() || !skeleton.valid()) {
        return foundation::Result<RagdollSchema, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid ragdoll phenotype or skeleton"});
    }
    RagdollSchema schema;
    schema.cache_key_ = foundation::stableHashCombine(skeleton.cacheKey(),
                                                       foundation::stableHashFloat(body.height));
    schema.cache_key_ = foundation::stableHashCombine(schema.cache_key_,
                                                       foundation::stableHashFloat(body.arm_length));
    schema.cache_key_ = foundation::stableHashCombine(schema.cache_key_,
                                                       foundation::stableHashFloat(body.leg_length));
    schema.bodies_.reserve(kRagdollBones.size());
    for (const BoneId bone : kRagdollBones) {
        const BoneRecord* record = skeleton.find(bone);
        if (record == nullptr || (record->reference_length <= 0.0F && bone != BoneId::Hips)) {
            return foundation::Result<RagdollSchema, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState, "ragdoll bone missing or has no length"});
        }
        const float length = std::max(0.08F, record->reference_length);
        RagdollBody ragdoll_body{};
        ragdoll_body.bone = bone;
        ragdoll_body.shape.kind = physics::ShapeKind::Box;
        ragdoll_body.shape.half_extent = {length * 0.42F, length * 0.5F, length * 0.42F};
        ragdoll_body.mass = std::clamp(length * 8.0F, 0.4F, 12.0F);
        schema.bodies_.push_back(ragdoll_body);
    }
    const auto rig = skeleton.bones();
    for (const RagdollBody& child : schema.bodies_) {
        const BoneRecord* child_record = skeleton.find(child.bone);
        if (child_record == nullptr || child_record->parent == kInvalidBoneIndex ||
            child_record->parent >= rig.size()) {
            continue;
        }
        const BoneId parent = rig[child_record->parent].id;
        if (containsBone(schema.bodies_, parent)) {
            schema.constraints_.push_back({parent, child.bone, 0.75F, 0.5F});
        }
    }
    return schema.valid()
               ? foundation::Result<RagdollSchema, foundation::Error>::success(std::move(schema))
               : foundation::Result<RagdollSchema, foundation::Error>::failure(
                     {foundation::ErrorCode::InvalidState, "ragdoll schema validation failed"});
}

const RagdollSchema* RagdollCache::find(foundation::StableId key) const noexcept {
    for (const RagdollSchema& schema : schemas_) {
        if (schema.cacheKey() == key) {
            return &schema;
        }
    }
    return nullptr;
}

void RagdollCache::store(RagdollSchema schema) {
    for (RagdollSchema& existing : schemas_) {
        if (existing.cacheKey() == schema.cacheKey()) {
            existing = std::move(schema);
            return;
        }
    }
    schemas_.push_back(std::move(schema));
}

foundation::Result<void, foundation::Error> RagdollController::enter(
    physics::PhysicsWorld& world, const RagdollSchema& schema,
    const AnimationPose& previous_pose, const AnimationPose& current_pose,
    float fixed_dt_seconds, std::uint64_t simulation_tick) {
    if (state_.active || !schema.valid() || !previous_pose.valid() || !current_pose.valid() ||
        !finite(fixed_dt_seconds) || fixed_dt_seconds <= 0.0F || fixed_dt_seconds > 0.25F) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid ragdoll transition"});
    }
    RagdollHandoff next{};
    next.simulation_tick = simulation_tick;
    next.bodies.reserve(schema.bodies().size());
    next.linear_velocities.reserve(schema.bodies().size());
    next.angular_velocities.reserve(schema.bodies().size());
    physics::PhysicsCommandBuffer velocities;
    for (const RagdollBody& description : schema.bodies()) {
        const std::size_t index = boneIndex(description.bone);
        if (index >= current_pose.bones.size()) {
            for (const physics::BodyHandle handle : next.bodies) {
                world.destroyBody(handle);
            }
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState, "ragdoll pose does not contain body bone"});
        }
        const foundation::Vec3 current_position =
            add(add(current_pose.root_position, current_pose.bones[index].translation),
                description.local_offset);
        const foundation::Vec3 previous_position =
            add(add(previous_pose.root_position, previous_pose.bones[index].translation),
                description.local_offset);
        const foundation::Vec3 velocity = divide(subtract(current_position, previous_position),
                                                  fixed_dt_seconds);
        physics::BodyDesc body{};
        body.type = physics::BodyType::Dynamic;
        body.shape = description.shape;
        body.position = current_position;
        body.linear_velocity = velocity;
        body.mass = description.mass;
        body.collision_layer = description.collision_layer;
        body.collision_mask = description.collision_mask;
        const auto created = world.createBody(body);
        if (!created) {
            for (const physics::BodyHandle handle : next.bodies) {
                world.destroyBody(handle);
            }
            return foundation::Result<void, foundation::Error>::failure(created.error());
        }
        next.bodies.push_back(created.value());
        next.linear_velocities.push_back(velocity);
        next.angular_velocities.push_back({});
        velocities.setLinearVelocity(created.value(), velocity);
    }
    world.apply(velocities);
    next.active = true;
    state_ = std::move(next);
    return foundation::Result<void, foundation::Error>::success();
}

void RagdollController::dispose(physics::PhysicsWorld& world) noexcept {
    if (!state_.active) {
        return;
    }
    physics::PhysicsCommandBuffer commands;
    for (const physics::BodyHandle handle : state_.bodies) {
        commands.destroy(handle);
    }
    world.apply(commands);
    state_ = {};
}

} // namespace genomes::infantry
