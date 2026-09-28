#include <genomes/infantry/RigBuilder.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

namespace genomes::infantry {

namespace {

using foundation::Vec3;

[[nodiscard]] Vec3 add(Vec3 left, Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] Vec3 subtract(Vec3 left, Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] Vec3 multiply(Vec3 value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] float dot(Vec3 left, Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] Vec3 cross(Vec3 left, Vec3 right) noexcept {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

[[nodiscard]] float lengthSquared(Vec3 value) noexcept { return dot(value, value); }

[[nodiscard]] float length(Vec3 value) noexcept { return std::sqrt(lengthSquared(value)); }

[[nodiscard]] Vec3 normalized(Vec3 value, Vec3 fallback = {0.0F, 1.0F, 0.0F}) noexcept {
    const float magnitude = length(value);
    return magnitude > 1.0e-6F && std::isfinite(magnitude) ? multiply(value, 1.0F / magnitude)
                                                            : fallback;
}

[[nodiscard]] Vec3 mix(Vec3 left, Vec3 right, float amount) noexcept {
    return add(left, multiply(subtract(right, left), amount));
}

[[nodiscard]] bool finite(Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] RigQuaternion normalize(RigQuaternion value) noexcept {
    const float magnitude = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z +
                                      value.w * value.w);
    if (!(magnitude > 1.0e-6F) || !std::isfinite(magnitude)) {
        return RigQuaternion::identity();
    }
    const float inverse = 1.0F / magnitude;
    return {value.x * inverse, value.y * inverse, value.z * inverse, value.w * inverse};
}

[[nodiscard]] RigQuaternion conjugate(RigQuaternion value) noexcept {
    return {-value.x, -value.y, -value.z, value.w};
}

[[nodiscard]] RigQuaternion multiply(RigQuaternion left, RigQuaternion right) noexcept {
    return {left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
            left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
            left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
            left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z};
}

[[nodiscard]] Vec3 rotate(RigQuaternion rotation, Vec3 value) noexcept {
    const RigQuaternion vector{value.x, value.y, value.z, 0.0F};
    const RigQuaternion result = multiply(multiply(normalize(rotation), vector),
                                          conjugate(normalize(rotation)));
    return {result.x, result.y, result.z};
}

[[nodiscard]] RigTransform compose(const RigTransform& parent,
                                   const RigTransform& local) noexcept {
    RigTransform result{};
    result.scale = {parent.scale.x * local.scale.x, parent.scale.y * local.scale.y,
                    parent.scale.z * local.scale.z};
    result.rotation = normalize(multiply(parent.rotation, local.rotation));
    const Vec3 scaled_local{local.translation.x * parent.scale.x,
                            local.translation.y * parent.scale.y,
                            local.translation.z * parent.scale.z};
    result.translation = add(parent.translation, rotate(parent.rotation, scaled_local));
    return result;
}

[[nodiscard]] RigTransform inverse(const RigTransform& transform) noexcept {
    RigTransform result{};
    result.scale = {1.0F / transform.scale.x, 1.0F / transform.scale.y,
                    1.0F / transform.scale.z};
    result.rotation = conjugate(normalize(transform.rotation));
    const Vec3 translated = multiply(transform.translation, -1.0F);
    result.translation = rotate(result.rotation,
                                {translated.x * result.scale.x, translated.y * result.scale.y,
                                 translated.z * result.scale.z});
    return result;
}

[[nodiscard]] foundation::StableId hashInput(const BodyPhenotype& body,
                                              const FacePhenotype& face) noexcept {
    foundation::StableId hash = foundation::stable_id("infantry.rig.v1");
    const auto addFloat = [&hash](float value) {
        hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(value));
    };
    addFloat(body.height);
    addFloat(body.shoulder_width);
    addFloat(body.chest_depth);
    addFloat(body.hip_width);
    addFloat(body.arm_length);
    addFloat(body.leg_length);
    addFloat(body.pelvis.x);
    addFloat(body.pelvis.y);
    addFloat(body.pelvis.z);
    addFloat(body.chest.x);
    addFloat(body.chest.y);
    addFloat(body.chest.z);
    addFloat(body.head.x);
    addFloat(body.head.y);
    addFloat(body.head.z);
    addFloat(body.left_hand.x);
    addFloat(body.left_hand.y);
    addFloat(body.left_hand.z);
    addFloat(body.right_hand.x);
    addFloat(body.right_hand.y);
    addFloat(body.right_hand.z);
    addFloat(body.left_foot.x);
    addFloat(body.left_foot.y);
    addFloat(body.left_foot.z);
    addFloat(body.right_foot.x);
    addFloat(body.right_foot.y);
    addFloat(body.right_foot.z);
    addFloat(face.eye_spacing);
    addFloat(face.eye_radius);
    addFloat(face.brow_y);
    addFloat(face.eye_y);
    addFloat(face.nose_y);
    addFloat(face.nose_length);
    addFloat(face.nose_width);
    addFloat(face.mouth_y);
    addFloat(face.mouth_width);
    addFloat(face.jaw_width);
    addFloat(face.hairline_y);
    return hash;
}

void setPoint(std::array<Vec3, kRigBoneCount>& points, BoneId id, Vec3 point) noexcept {
    points[boneIndex(id)] = point;
}

void buildFinger(std::array<Vec3, kRigBoneCount>& points,
                 BoneId first,
                 Vec3 hand,
                 Vec3 forearm,
                 Vec3 lateral,
                 float lateral_offset,
                 bool thumb,
                 float height) noexcept {
    const Vec3 direction = normalized(subtract(hand, forearm), {0.0F, -1.0F, 0.0F});
    const Vec3 side = normalized(cross(direction, {0.0F, 0.0F, 1.0F}), lateral);
    Vec3 axis = direction;
    if (thumb) {
        axis = normalized(add(add(direction, multiply({0.0F, 0.0F, 1.0F}, 0.22F)),
                             multiply(side, lateral_offset < 0.0F ? -0.75F : 0.75F)),
                          direction);
    }
    const Vec3 start = add(add(hand, multiply(direction, height * (thumb ? 0.024F : 0.052F))),
                           multiply(side, height * lateral_offset));
    const float segment_length = height * (thumb ? 0.034F : 0.030F);
    const std::size_t first_index = boneIndex(first);
    setPoint(points, static_cast<BoneId>(first_index), start);
    setPoint(points, static_cast<BoneId>(first_index + 1U),
             add(start, multiply(axis, segment_length * 0.32F)));
    setPoint(points, static_cast<BoneId>(first_index + 2U),
             add(start, multiply(axis, segment_length * 0.65F)));
}

} // namespace

const BoneRecord* SkeletonData::find(BoneId id) const noexcept {
    const std::size_t index = boneIndex(id);
    if (index >= bones_.size() || bones_[index].id != id) {
        return nullptr;
    }
    return &bones_[index];
}

const AttachmentPoint* SkeletonData::findAttachment(AttachmentPointId id) const noexcept {
    for (const AttachmentPoint& attachment : attachments_) {
        if (attachment.id == id) {
            return &attachment;
        }
    }
    return nullptr;
}

bool SkeletonData::valid() const noexcept {
    if (schema_version_ != RigSchemaVersion || cache_key_ == 0 || bones_.size() != kRigBoneCount) {
        return false;
    }
    std::size_t body_count = 0U;
    std::size_t face_count = 0U;
    std::size_t finger_count = 0U;
    std::size_t roots = 0U;
    for (std::size_t index = 0U; index < bones_.size(); ++index) {
        const BoneRecord& bone = bones_[index];
        const BoneSchema& schema = kCanonicalRigSchema[index];
        if (bone.id != schema.id || bone.name != schema.name || bone.parent != schema.parent ||
            bone.category != schema.category) {
            return false;
        }
        if (bone.parent == kInvalidBoneIndex) {
            ++roots;
        } else if (bone.parent >= index || bone.parent >= bones_.size()) {
            return false;
        }
        const auto finiteTransform = [](const RigTransform& transform) {
            return finite(transform.translation) && finite(transform.scale) &&
                   std::isfinite(transform.rotation.x) && std::isfinite(transform.rotation.y) &&
                   std::isfinite(transform.rotation.z) && std::isfinite(transform.rotation.w) &&
                   transform.scale.x > 0.0F && transform.scale.y > 0.0F &&
                   transform.scale.z > 0.0F;
        };
        if (!finiteTransform(bone.local_bind) || !finiteTransform(bone.world_bind) ||
            !finiteTransform(bone.inverse_bind) ||
            (bone.parent != kInvalidBoneIndex && !(bone.reference_length > 1.0e-6F))) {
            return false;
        }
        switch (bone.category) {
        case BoneCategory::Body:
            ++body_count;
            break;
        case BoneCategory::FaceController:
            ++face_count;
            break;
        case BoneCategory::Finger:
            ++finger_count;
            break;
        }
    }
    if (roots != 1U || body_count != kBodyBoneCount || face_count != kFaceBoneCount ||
        finger_count != kFingerBoneCount) {
        return false;
    }
    for (const AttachmentPoint& attachment : attachments_) {
        if (find(attachment.bone) == nullptr || !finite(attachment.local) ||
            !finite(attachment.world)) {
            return false;
        }
    }
    return true;
}

foundation::Result<SkeletonData, foundation::Error> RigBuilder::build(
    const BodyPhenotype& body, const FacePhenotype& face) {
    if (!body.valid() || !face.valid()) {
        return foundation::Result<SkeletonData, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid infantry rig phenotype"});
    }

    std::array<Vec3, kRigBoneCount> points{};
    setPoint(points, BoneId::Hips, body.pelvis);
    setPoint(points, BoneId::SpineLower, mix(body.pelvis, body.chest, 0.25F));
    setPoint(points, BoneId::SpineUpper, mix(body.pelvis, body.chest, 0.56F));
    setPoint(points, BoneId::Chest, body.chest);
    const Vec3 neck = mix(body.chest, body.head, 0.63F);
    setPoint(points, BoneId::Neck, neck);
    setPoint(points, BoneId::Head, body.head);

    const float shoulder_half = std::max(0.02F, body.shoulder_width * 0.5F);
    const float clavicle_half = std::max(0.01F, shoulder_half * 0.20F);
    const float shoulder_y = body.chest.y + (neck.y - body.chest.y) * 0.34F;
    setPoint(points, BoneId::ClavicleL, {-clavicle_half, shoulder_y, body.chest.z});
    setPoint(points, BoneId::UpperArmL, {-shoulder_half, shoulder_y, body.chest.z});
    setPoint(points, BoneId::ForeArmL,
             mix(points[boneIndex(BoneId::UpperArmL)], body.left_hand, 0.5F));
    setPoint(points, BoneId::HandL, body.left_hand);
    setPoint(points, BoneId::ClavicleR, {clavicle_half, shoulder_y, body.chest.z});
    setPoint(points, BoneId::UpperArmR, {shoulder_half, shoulder_y, body.chest.z});
    setPoint(points, BoneId::ForeArmR,
             mix(points[boneIndex(BoneId::UpperArmR)], body.right_hand, 0.5F));
    setPoint(points, BoneId::HandR, body.right_hand);

    const Vec3 left_thigh{body.left_foot.x, body.pelvis.y - body.height * 0.012F, body.pelvis.z};
    const Vec3 right_thigh{body.right_foot.x, body.pelvis.y - body.height * 0.012F, body.pelvis.z};
    setPoint(points, BoneId::ThighL, left_thigh);
    setPoint(points, BoneId::ShinL, mix(left_thigh, body.left_foot, 0.5F));
    setPoint(points, BoneId::FootL, body.left_foot);
    setPoint(points, BoneId::ToesL,
             add(body.left_foot, {0.0F, -body.height * 0.012F, body.height * 0.048F}));
    setPoint(points, BoneId::ThighR, right_thigh);
    setPoint(points, BoneId::ShinR, mix(right_thigh, body.right_foot, 0.5F));
    setPoint(points, BoneId::FootR, body.right_foot);
    setPoint(points, BoneId::ToesR,
             add(body.right_foot, {0.0F, -body.height * 0.012F, body.height * 0.048F}));

    const float jaw_y = std::max(face.mouth_y + 0.022F, body.head.y - body.height * 0.055F);
    setPoint(points, BoneId::Jaw, {0.0F, jaw_y, face.frontZ(jaw_y)});
    setPoint(points, BoneId::MouthUpper,
             {0.0F, face.mouth_y + 0.0012F, face.frontZ(face.mouth_y) + 0.0009F});
    setPoint(points, BoneId::MouthLower,
             {0.0F, face.mouth_y - 0.0012F, face.frontZ(face.mouth_y) + 0.0009F});

    const auto setFaceSide = [&](bool left, BoneId eye, BoneId lid_upper, BoneId lid_lower,
                                 BoneId brow_inner, BoneId brow_outer, BoneId mouth_corner,
                                 BoneId cheek) {
        const float side = left ? -1.0F : 1.0F;
        const float eye_x = side * face.eye_spacing;
        setPoint(points, eye, {eye_x, face.eye_y, face.frontZ(face.eye_y) - face.eye_radius * 0.8F});
        setPoint(points, lid_upper,
                 {eye_x, face.eye_y + face.eye_radius, face.frontZ(face.eye_y + face.eye_radius)});
        setPoint(points, lid_lower,
                 {eye_x, face.eye_y - face.eye_radius, face.frontZ(face.eye_y - face.eye_radius)});
        const float inner_x = side * std::max(0.006F, face.eye_spacing - 0.007F);
        const float outer_x = side * (face.eye_spacing + 0.008F);
        setPoint(points, brow_inner, {inner_x, face.brow_y, face.frontZ(face.brow_y) + 0.0012F});
        setPoint(points, brow_outer,
                 {outer_x, face.brow_y, face.frontZ(face.brow_y) + 0.0012F});
        setPoint(points, mouth_corner,
                 {side * face.mouth_width * 0.5F, face.mouth_y,
                  face.frontZ(face.mouth_y) + 0.001F});
        const float cheek_x = side * (face.eye_spacing + 0.006F);
        const float cheek_y = face.eye_y - 0.016F;
        setPoint(points, cheek, {cheek_x, cheek_y, face.frontZ(cheek_y)});
    };
    setFaceSide(true, BoneId::EyeL, BoneId::LidUpperL, BoneId::LidLowerL,
                BoneId::BrowInnerL, BoneId::BrowOuterL, BoneId::MouthCornerL, BoneId::CheekL);
    setFaceSide(false, BoneId::EyeR, BoneId::LidUpperR, BoneId::LidLowerR,
                BoneId::BrowInnerR, BoneId::BrowOuterR, BoneId::MouthCornerR, BoneId::CheekR);

    const std::array<BoneId, 5U> left_first{
        BoneId::FingerLLittle0, BoneId::FingerLRing0, BoneId::FingerLMiddle0,
        BoneId::FingerLIndex0, BoneId::FingerLThumb0};
    const std::array<BoneId, 5U> right_first{
        BoneId::FingerRIndex0, BoneId::FingerRMiddle0, BoneId::FingerRRing0,
        BoneId::FingerRLittle0, BoneId::FingerRThumb0};
    const std::array<float, 4U> left_offsets{-1.5F, -0.5F, 0.5F, 1.5F};
    const std::array<float, 4U> right_offsets{1.5F, 0.5F, -0.5F, -1.5F};
    for (std::size_t index = 0U; index < 5U; ++index) {
        buildFinger(points, left_first[index], body.left_hand, points[boneIndex(BoneId::ForeArmL)],
                    {-1.0F, 0.0F, 0.0F}, index == 4U ? -0.017F : left_offsets[index] * 0.0101F,
                    index == 4U, body.height);
        buildFinger(points, right_first[index], body.right_hand,
                    points[boneIndex(BoneId::ForeArmR)], {1.0F, 0.0F, 0.0F},
                    index == 4U ? 0.017F : right_offsets[index] * 0.0101F, index == 4U,
                    body.height);
    }

    SkeletonData result{};
    result.schema_version_ = RigSchemaVersion;
    result.cache_key_ = hashInput(body, face);
    result.bones_.reserve(kRigBoneCount);
    for (std::size_t index = 0U; index < kRigBoneCount; ++index) {
        const BoneSchema& schema = kCanonicalRigSchema[index];
        BoneRecord record{};
        record.id = schema.id;
        record.name = schema.name;
        record.parent = schema.parent;
        record.category = schema.category;
        record.local_bind.translation = schema.parent == kInvalidBoneIndex
                                            ? points[index]
                                            : subtract(points[index], points[schema.parent]);
        record.local_bind.rotation = RigQuaternion::identity();
        record.local_bind.scale = {1.0F, 1.0F, 1.0F};
        record.world_bind = schema.parent == kInvalidBoneIndex
                                ? record.local_bind
                                : compose(result.bones_[schema.parent].world_bind, record.local_bind);
        record.inverse_bind = inverse(record.world_bind);
        record.reference_length = schema.parent == kInvalidBoneIndex
                                      ? 0.0F
                                      : length(record.local_bind.translation);
        result.bones_.push_back(record);
    }

    const auto addAttachment = [&result, &points](AttachmentPointId id, BoneId bone) {
        result.attachments_.push_back({id, bone, {}, points[boneIndex(bone)]});
    };
    addAttachment(AttachmentPointId::Pelvis, BoneId::Hips);
    addAttachment(AttachmentPointId::Chest, BoneId::Chest);
    addAttachment(AttachmentPointId::Head, BoneId::Head);
    addAttachment(AttachmentPointId::Jaw, BoneId::Jaw);
    addAttachment(AttachmentPointId::LeftHand, BoneId::HandL);
    addAttachment(AttachmentPointId::RightHand, BoneId::HandR);
    addAttachment(AttachmentPointId::LeftFoot, BoneId::FootL);
    addAttachment(AttachmentPointId::RightFoot, BoneId::FootR);
    addAttachment(AttachmentPointId::LeftEye, BoneId::EyeL);
    addAttachment(AttachmentPointId::RightEye, BoneId::EyeR);
    addAttachment(AttachmentPointId::Mouth, BoneId::MouthUpper);

    return result.valid()
               ? foundation::Result<SkeletonData, foundation::Error>::success(std::move(result))
               : foundation::Result<SkeletonData, foundation::Error>::failure(
                     {foundation::ErrorCode::InvalidState, "infantry rig constraints are infeasible"});
}

} // namespace genomes::infantry
