#include <genomes/infantry/AnimationSystem.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <mutex>

namespace genomes::infantry {

namespace {

[[nodiscard]] bool finite(float value) noexcept { return std::isfinite(value); }

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

[[nodiscard]] float mix(float a, float b, float alpha) noexcept {
    return a + (b - a) * alpha;
}

[[nodiscard]] RigQuaternion mixQuaternion(RigQuaternion a,
                                           RigQuaternion b,
                                           float alpha) noexcept {
    RigQuaternion result{mix(a.x, b.x, alpha), mix(a.y, b.y, alpha), mix(a.z, b.z, alpha),
                         mix(a.w, b.w, alpha)};
    const float length = std::sqrt(result.x * result.x + result.y * result.y +
                                   result.z * result.z + result.w * result.w);
    if (length > 1.0e-6F && finite(length)) {
        result.x /= length;
        result.y /= length;
        result.z /= length;
        result.w /= length;
    } else {
        result = RigQuaternion::identity();
    }
    return result;
}

[[nodiscard]] RigQuaternion multiply(RigQuaternion left, RigQuaternion right) noexcept {
    return {left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
            left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
            left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
            left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z};
}

[[nodiscard]] RigQuaternion axisAngle(foundation::Vec3 axis, float angle) noexcept {
    const float half = angle * 0.5F;
    const float sine = std::sin(half);
    return {axis.x * sine, axis.y * sine, axis.z * sine, std::cos(half)};
}

void rotateBone(std::array<RigTransform, kRigBoneCount>& bones,
                BoneId id,
                foundation::Vec3 axis,
                float angle) noexcept {
    if (std::abs(angle) <= 1.0e-6F) {
        return;
    }
    const std::size_t index = boneIndex(id);
    bones[index].rotation = multiply(axisAngle(axis, angle), bones[index].rotation);
}

void applyLocomotionPose(std::array<RigTransform, kRigBoneCount>& bones,
                         const LocomotionState& state,
                         const PostureSample& posture) noexcept {
    constexpr float pi = 3.14159265358979323846F;
    const float phase = state.phase * 2.0F * pi;
    const float stride = std::clamp(state.actual_speed_mps / 3.25F, 0.0F, 1.0F);
    const float leg_swing = std::sin(phase) * (0.42F + 0.22F * state.run_weight) * stride;
    const float opposite_swing = std::sin(phase + pi) * (0.42F + 0.22F * state.run_weight) * stride;
    const float arm_swing = std::sin(phase + pi) * 0.28F * stride;
    rotateBone(bones, BoneId::ThighL, {1.0F, 0.0F, 0.0F}, leg_swing);
    rotateBone(bones, BoneId::ThighR, {1.0F, 0.0F, 0.0F}, opposite_swing);
    rotateBone(bones, BoneId::ShinL, {1.0F, 0.0F, 0.0F}, -leg_swing * 0.45F - posture.knee_bend);
    rotateBone(bones, BoneId::ShinR, {1.0F, 0.0F, 0.0F}, -opposite_swing * 0.45F - posture.knee_bend);
    rotateBone(bones, BoneId::UpperArmL, {1.0F, 0.0F, 0.0F}, arm_swing);
    rotateBone(bones, BoneId::UpperArmR, {1.0F, 0.0F, 0.0F}, -arm_swing);
    rotateBone(bones, BoneId::ForeArmL, {1.0F, 0.0F, 0.0F}, posture.arm_relax * 0.35F);
    rotateBone(bones, BoneId::ForeArmR, {1.0F, 0.0F, 0.0F}, posture.arm_relax * 0.35F);
    rotateBone(bones, BoneId::SpineLower, {1.0F, 0.0F, 0.0F}, posture.torso_pitch);
    rotateBone(bones, BoneId::SpineUpper, {1.0F, 0.0F, 0.0F}, posture.torso_pitch * 0.65F);
    rotateBone(bones, BoneId::Chest, {1.0F, 0.0F, 0.0F}, posture.torso_pitch * 0.35F);
}

void applyFacePose(std::array<RigTransform, kRigBoneCount>& bones,
                   const FaceOutput& face) noexcept {
    rotateBone(bones, BoneId::Head, {0.0F, 1.0F, 0.0F}, face.head_yaw);
    rotateBone(bones, BoneId::Head, {1.0F, 0.0F, 0.0F}, face.head_pitch);
    rotateBone(bones, BoneId::Jaw, {1.0F, 0.0F, 0.0F}, face.jaw_rotation);
    rotateBone(bones, BoneId::EyeL, {0.0F, 1.0F, 0.0F}, face.eye_yaw);
    rotateBone(bones, BoneId::EyeR, {0.0F, 1.0F, 0.0F}, face.eye_yaw);
    rotateBone(bones, BoneId::EyeL, {1.0F, 0.0F, 0.0F}, face.eye_pitch);
    rotateBone(bones, BoneId::EyeR, {1.0F, 0.0F, 0.0F}, face.eye_pitch);
}

[[nodiscard]] foundation::Error invalidEntityError() noexcept {
    return {foundation::ErrorCode::InvalidArgument, "invalid animation entity contract"};
}

} // namespace

bool AnimationEntity::valid() const noexcept {
    return skeleton != nullptr && skeleton->valid() && locomotion != nullptr &&
           locomotion_state != nullptr && locomotion_state->valid() && finite(root_position) &&
           (!look_target.has_value() || finite(look_target.value())) && lod.spec().valid();
}

bool AnimationPose::valid() const noexcept {
    if (!finite(root_position) || !finite(locomotion_phase) || locomotion_phase < 0.0F ||
        locomotion_phase >= 1.0F || !face.valid()) {
        return false;
    }
    for (const RigTransform& bone : bones) {
        if (!finite(bone.translation) || !finite(bone.scale) || !finite(bone.rotation.x) ||
            !finite(bone.rotation.y) || !finite(bone.rotation.z) || !finite(bone.rotation.w)) {
            return false;
        }
    }
    return true;
}

foundation::Result<AnimationSystem, foundation::Error> AnimationSystem::create(
    std::size_t chunk_size) {
    if (chunk_size == 0U || chunk_size > 1U << 20U) {
        return foundation::Result<AnimationSystem, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid animation chunk size"});
    }
    return foundation::Result<AnimationSystem, foundation::Error>::success(
        AnimationSystem(chunk_size));
}

foundation::Result<void, foundation::Error> AnimationSystem::evaluate(
    std::span<AnimationEntity> entities,
    std::uint64_t simulation_tick,
    float fixed_dt_seconds,
    jobs::JobSystem* jobs) {
    if (!finite(fixed_dt_seconds) || fixed_dt_seconds <= 0.0F || fixed_dt_seconds > 0.25F) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid animation fixed interval"});
    }
    for (const AnimationEntity& entity : entities) {
        if (!entity.valid()) {
            return foundation::Result<void, foundation::Error>::failure(invalidEntityError());
        }
    }

    previous_ = std::move(current_);
    current_ = {};
    current_.simulation_tick = simulation_tick;
    current_.previous_simulation_tick = previous_.simulation_tick;
    current_.pose_revision = previous_.pose_revision + 1U;
    current_.poses = previous_.poses;
    current_.poses.resize(entities.size());
    for (std::size_t index = 0U; index < entities.size(); ++index) {
        if (current_.poses[index].semantic_id != entities[index].semantic_id) {
            current_.poses[index] = {};
            current_.poses[index].semantic_id = entities[index].semantic_id;
        }
    }

    struct WorkItem final {
        bool due{false};
        std::uint64_t elapsed_ticks{1U};
    };
    std::vector<WorkItem> work(entities.size());
    std::uint32_t due_count = 0U;
    for (std::size_t index = 0U; index < entities.size(); ++index) {
        work[index].due = entities[index].lod.due(simulation_tick);
        work[index].elapsed_ticks = entities[index].lod.elapsedTicks(simulation_tick);
        due_count += work[index].due ? 1U : 0U;
    }

    const std::size_t chunk_count =
        entities.empty() ? 0U : (entities.size() + chunk_size_ - 1U) / chunk_size_;
    std::atomic<bool> failed{false};
    std::mutex error_mutex;
    foundation::Error first_error{};

    const auto processRange = [&](std::size_t begin, std::size_t end) noexcept {
        for (std::size_t index = begin; index < end; ++index) {
            if (!work[index].due || failed.load(std::memory_order_acquire)) {
                continue;
            }
            AnimationEntity& entity = entities[index];
            AnimationPose& pose = current_.poses[index];
            const PostureSample posture = entity.locomotion->posture(*entity.locomotion_state);
            pose.semantic_id = entity.semantic_id;
            pose.root_position = entity.root_position;
            pose.posture = posture;
            pose.locomotion_phase = entity.locomotion_state->phase;
            pose.revision = current_.pose_revision;
            pose.evaluated = true;

            const auto bones = entity.skeleton->bones();
            if (bones.size() != kRigBoneCount) {
                std::lock_guard lock(error_mutex);
                if (!failed.exchange(true, std::memory_order_acq_rel)) {
                    first_error = {foundation::ErrorCode::InvalidState,
                                   "animation skeleton does not match rig schema"};
                }
                continue;
            }
            for (std::size_t bone = 0U; bone < kRigBoneCount; ++bone) {
                pose.bones[bone] = bones[bone].local_bind;
            }

            applyLocomotionPose(pose.bones, *entity.locomotion_state, posture);

            const AnimationLODSpec spec = entity.lod.spec();
            if (entity.face != nullptr && spec.evaluate_face) {
                if (!entity.face->setLookTarget(entity.look_target, entity.root_position)) {
                    std::lock_guard lock(error_mutex);
                    if (!failed.exchange(true, std::memory_order_acq_rel)) {
                        first_error = {foundation::ErrorCode::InvalidState,
                                       "face look target rejected by animation stage"};
                    }
                    continue;
                }
                float remaining = fixed_dt_seconds *
                                  static_cast<float>(std::min<std::uint64_t>(
                                      work[index].elapsed_ticks, 1024U));
                while (remaining > 0.0F) {
                    const float step = std::min(remaining, 0.25F);
                    if (!entity.face->step(step)) {
                        std::lock_guard lock(error_mutex);
                        if (!failed.exchange(true, std::memory_order_acq_rel)) {
                            first_error = {foundation::ErrorCode::InvalidState,
                                           "face animation stage rejected fixed interval"};
                        }
                        break;
                    }
                    remaining -= step;
                }
                pose.face = entity.face->output();
            }
            applyFacePose(pose.bones, pose.face);
            entity.lod.markEvaluated(simulation_tick);
        }
    };

    if (jobs != nullptr && chunk_count > 1U) {
        std::vector<jobs::JobHandle> handles;
        handles.reserve(chunk_count);
        for (std::size_t chunk = 0U; chunk < chunk_count; ++chunk) {
            const std::size_t begin = chunk * chunk_size_;
            const std::size_t end = std::min(begin + chunk_size_, entities.size());
            handles.push_back(jobs->submit([&, begin, end](jobs::JobContext&) {
                processRange(begin, end);
            }));
        }
        for (const jobs::JobHandle& handle : handles) {
            jobs->wait(handle);
            if (handle.failed()) {
                failed.store(true, std::memory_order_release);
            }
        }
    } else {
        processRange(0U, entities.size());
    }

    if (failed.load(std::memory_order_acquire)) {
        return foundation::Result<void, foundation::Error>::failure(
            first_error.code == foundation::ErrorCode::None
                ? foundation::Error{foundation::ErrorCode::Internal,
                                    "animation batch job failed"}
                : first_error);
    }

    stats_ = {simulation_tick,
              static_cast<std::uint32_t>(entities.size()),
              due_count,
              due_count,
              static_cast<std::uint32_t>(chunk_count),
              current_.pose_revision};
    return foundation::Result<void, foundation::Error>::success();
}

AnimationPose AnimationSystem::interpolate(const AnimationPose& previous,
                                           const AnimationPose& current,
                                           float alpha) noexcept {
    const float t = std::clamp(finite(alpha) ? alpha : 0.0F, 0.0F, 1.0F);
    AnimationPose result = current;
    result.semantic_id = current.semantic_id != 0U ? current.semantic_id : previous.semantic_id;
    result.root_position = {mix(previous.root_position.x, current.root_position.x, t),
                            mix(previous.root_position.y, current.root_position.y, t),
                            mix(previous.root_position.z, current.root_position.z, t)};
    result.posture.crouch = mix(previous.posture.crouch, current.posture.crouch, t);
    result.posture.hip_height = mix(previous.posture.hip_height, current.posture.hip_height, t);
    result.posture.torso_pitch = mix(previous.posture.torso_pitch, current.posture.torso_pitch, t);
    result.posture.knee_bend = mix(previous.posture.knee_bend, current.posture.knee_bend, t);
    result.posture.ankle_pitch = mix(previous.posture.ankle_pitch, current.posture.ankle_pitch, t);
    result.posture.arm_relax = mix(previous.posture.arm_relax, current.posture.arm_relax, t);
    result.locomotion_phase = mix(previous.locomotion_phase, current.locomotion_phase, t);
    for (std::size_t channel = 0U; channel < kFaceChannelCount; ++channel) {
        result.face.channels[channel] = mix(previous.face.channels[channel], current.face.channels[channel], t);
    }
    result.face.eyelids_close = mix(previous.face.eyelids_close, current.face.eyelids_close, t);
    result.face.eyelids_arc = mix(previous.face.eyelids_arc, current.face.eyelids_arc, t);
    result.face.jaw_rotation = mix(previous.face.jaw_rotation, current.face.jaw_rotation, t);
    result.face.head_yaw = mix(previous.face.head_yaw, current.face.head_yaw, t);
    result.face.head_pitch = mix(previous.face.head_pitch, current.face.head_pitch, t);
    result.face.eye_yaw = mix(previous.face.eye_yaw, current.face.eye_yaw, t);
    result.face.eye_pitch = mix(previous.face.eye_pitch, current.face.eye_pitch, t);
    for (std::size_t bone = 0U; bone < kRigBoneCount; ++bone) {
        result.bones[bone].translation = {
            mix(previous.bones[bone].translation.x, current.bones[bone].translation.x, t),
            mix(previous.bones[bone].translation.y, current.bones[bone].translation.y, t),
            mix(previous.bones[bone].translation.z, current.bones[bone].translation.z, t)};
        result.bones[bone].rotation =
            mixQuaternion(previous.bones[bone].rotation, current.bones[bone].rotation, t);
        result.bones[bone].scale = {
            mix(previous.bones[bone].scale.x, current.bones[bone].scale.x, t),
            mix(previous.bones[bone].scale.y, current.bones[bone].scale.y, t),
            mix(previous.bones[bone].scale.z, current.bones[bone].scale.z, t)};
    }
    result.evaluated = previous.evaluated || current.evaluated;
    return result;
}

} // namespace genomes::infantry
