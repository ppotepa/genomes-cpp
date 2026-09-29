#include <genomes/infantry/EquipmentCatalog.hpp>
#include <genomes/infantry/EquipmentFit.hpp>
#include <genomes/infantry/FaceAnatomy.hpp>
#include <genomes/infantry/InfantryMaterials.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace genomes::infantry {

namespace {

constexpr std::array<std::array<float, 3U>, 14U> kJacketProfile{{
    {{.504F, .112F, .071F}}, {{.519F, .113F, .071F}},
    {{.552F, .110F, .067F}}, {{.595F, .094F, .056F}},
    {{.640F, .100F, .059F}}, {{.698F, .113F, .066F}},
    {{.735F, .122F, .066F}}, {{.759F, .129F, .064F}},
    {{.785F, .134F, .062F}}, {{.809F, .127F, .057F}},
    {{.825F, .112F, .051F}}, {{.841F, .070F, .040F}},
    {{.852F, .045F, .036F}}, {{.859F, .042F, .034F}},
}};

[[nodiscard]] float mix(float a, float b, float t) noexcept { return a + (b - a) * t; }
[[nodiscard]] float smooth(float value) noexcept {
    const float t = std::clamp(value, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}
[[nodiscard]] double smoothExact(double value) noexcept {
    const double t=std::clamp(value,0.0,1.0);
    return t*t*(3.0-2.0*t);
}

[[nodiscard]] std::uint32_t materialRegionFor(EquipmentSlot slot) noexcept {
    switch (slot) {
    case EquipmentSlot::Feet:
        return static_cast<std::uint32_t>(AppearanceMaterialRegion::BootLeather);
    case EquipmentSlot::PrimaryWeapon:
    case EquipmentSlot::SecondaryWeapon:
    case EquipmentSlot::MeleeWeapon:
    case EquipmentSlot::Throwable:
        return static_cast<std::uint32_t>(AppearanceMaterialRegion::EquipmentMetal);
    case EquipmentSlot::Head:
    case EquipmentSlot::TorsoArmor:
        return static_cast<std::uint32_t>(AppearanceMaterialRegion::EquipmentPaint);
    case EquipmentSlot::ChestRig:
    case EquipmentSlot::Back:
    case EquipmentSlot::LeftHip:
    case EquipmentSlot::RightHip:
    case EquipmentSlot::LeftThigh:
    case EquipmentSlot::RightThigh:
    case EquipmentSlot::Utility1:
    case EquipmentSlot::Utility2:
    case EquipmentSlot::Utility3:
        return static_cast<std::uint32_t>(AppearanceMaterialRegion::EquipmentCloth);
    default:
        return static_cast<std::uint32_t>(AppearanceMaterialRegion::UniformCloth);
    }
}

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool finite(foundation::Color value) noexcept {
    return std::isfinite(value.r) && std::isfinite(value.g) && std::isfinite(value.b) &&
           std::isfinite(value.a);
}

[[nodiscard]] BoneId anchorBone(EquipmentSlot slot) noexcept {
    switch (slot) {
    case EquipmentSlot::Head:
    case EquipmentSlot::Face:
        return BoneId::Head;
    case EquipmentSlot::Neck:
        return BoneId::Neck;
    case EquipmentSlot::TorsoBase:
    case EquipmentSlot::TorsoArmor:
    case EquipmentSlot::ChestRig:
    case EquipmentSlot::Utility3:
        return BoneId::Chest;
    case EquipmentSlot::Legs:
    case EquipmentSlot::Belt:
    case EquipmentSlot::Utility1:
    case EquipmentSlot::Utility2:
        return BoneId::Hips;
    case EquipmentSlot::Feet:
        return BoneId::FootL;
    case EquipmentSlot::Hands:
        return BoneId::HandL;
    case EquipmentSlot::Back:
        return BoneId::Chest;
    case EquipmentSlot::LeftHip:
        return BoneId::ThighL;
    case EquipmentSlot::RightHip:
        return BoneId::ThighR;
    case EquipmentSlot::LeftThigh:
        return BoneId::ThighL;
    case EquipmentSlot::RightThigh:
        return BoneId::ThighR;
    case EquipmentSlot::MeleeWeapon:
    case EquipmentSlot::Throwable:
    case EquipmentSlot::PrimaryWeapon:
    case EquipmentSlot::SecondaryWeapon:
        return BoneId::Chest;
    }
    return BoneId::Hips;
}

[[nodiscard]] std::optional<EquipmentSocketId> socketFor(EquipmentSlot slot) noexcept {
    switch (slot) {
    case EquipmentSlot::Head: return EquipmentSocketId::Head;
    case EquipmentSlot::Face: return EquipmentSocketId::HeadFront;
    case EquipmentSlot::Neck: return EquipmentSocketId::Neck;
    case EquipmentSlot::TorsoArmor:
    case EquipmentSlot::ChestRig: return EquipmentSocketId::ChestCenter;
    case EquipmentSlot::Back: return EquipmentSocketId::BackCenter;
    case EquipmentSlot::Belt: return EquipmentSocketId::Waist;
    case EquipmentSlot::LeftHip: return EquipmentSocketId::HipL;
    case EquipmentSlot::RightHip:
    case EquipmentSlot::MeleeWeapon: return EquipmentSocketId::HipR;
    case EquipmentSlot::LeftThigh: return EquipmentSocketId::ThighL;
    case EquipmentSlot::RightThigh: return EquipmentSocketId::ThighR;
    case EquipmentSlot::Utility1:
    case EquipmentSlot::Throwable: return EquipmentSocketId::WaistFront;
    case EquipmentSlot::Utility2: return EquipmentSocketId::WaistBack;
    case EquipmentSlot::Utility3: return EquipmentSocketId::ChestLeft;
    case EquipmentSlot::PrimaryWeapon: return EquipmentSocketId::WeaponBack;
    case EquipmentSlot::SecondaryWeapon: return EquipmentSocketId::WeaponHip;
    default: return std::nullopt;
    }
}

[[nodiscard]] foundation::Vec3 baseDimensions(EquipmentSlot slot,
                                               const BodyPhenotype& body) noexcept {
    switch (slot) {
    case EquipmentSlot::Head:
        return {body.shoulder_width * 0.30F, body.height * 0.14F, body.chest_depth * 0.55F};
    case EquipmentSlot::Face:
        return {body.shoulder_width * 0.18F, body.height * 0.08F, body.chest_depth * 0.18F};
    case EquipmentSlot::Neck:
        return {body.shoulder_width * 0.16F, body.height * 0.06F, body.chest_depth * 0.28F};
    case EquipmentSlot::TorsoBase:
    case EquipmentSlot::TorsoArmor:
    case EquipmentSlot::ChestRig:
        return {body.shoulder_width * 0.82F, body.height * 0.30F, body.chest_depth * 1.15F};
    case EquipmentSlot::Legs:
        return {body.hip_width * 0.95F, body.height * 0.26F, body.chest_depth * 0.85F};
    case EquipmentSlot::Feet:
        return {body.hip_width * 0.28F, body.height * 0.08F, body.height * 0.13F};
    case EquipmentSlot::Hands:
        return {body.arm_length * 0.10F, body.arm_length * 0.08F, body.arm_length * 0.14F};
    case EquipmentSlot::Back:
        return {body.shoulder_width * 0.52F, body.height * 0.28F, body.chest_depth * 0.75F};
    case EquipmentSlot::Belt:
        return {body.hip_width * 1.05F, body.height * 0.045F, body.chest_depth * 0.32F};
    case EquipmentSlot::LeftHip:
    case EquipmentSlot::RightHip:
    case EquipmentSlot::LeftThigh:
    case EquipmentSlot::RightThigh:
        return {body.hip_width * 0.24F, body.height * 0.12F, body.chest_depth * 0.32F};
    case EquipmentSlot::Utility1:
    case EquipmentSlot::Utility2:
    case EquipmentSlot::Utility3:
        return {body.hip_width * 0.22F, body.height * 0.11F, body.chest_depth * 0.26F};
    case EquipmentSlot::MeleeWeapon:
    case EquipmentSlot::Throwable:
    case EquipmentSlot::SecondaryWeapon:
        return {body.arm_length * 0.12F, body.arm_length * 0.08F, body.arm_length * 0.30F};
    case EquipmentSlot::PrimaryWeapon:
        return {body.arm_length * 0.16F, body.arm_length * 0.11F, body.arm_length * 0.78F};
    }
    return {0.1F, 0.1F, 0.1F};
}

} // namespace

float EquipmentFit::mapTorsoY(float reference_y) const noexcept {
    return mix(hip_y - .036F, .859F,
               std::clamp((reference_y - .504F) / .355F, 0.0F, 1.0F));
}

double EquipmentFit::mapTorsoYExact(double reference_y) const noexcept {
    const double start=reference_hip_y-.036;
    const double t=std::clamp((reference_y-.504)/.355,0.0,1.0);
    return start+(.859-start)*t;
}

foundation::Vec2 EquipmentFit::profile(float y) const noexcept {
    for (std::size_t index = 0U; index + 1U < jacket.size(); ++index) {
        if (y <= jacket[index + 1U].y) {
            const auto& a = jacket[index];
            const auto& b = jacket[index + 1U];
            const float t = std::clamp((y - a.y) / (b.y - a.y), 0.0F, 1.0F);
            return {mix(a.half_width, b.half_width, t),
                    mix(a.half_depth, b.half_depth, t)};
        }
    }
    return {jacket.back().half_width, jacket.back().half_depth};
}

foundation::Vec3 EquipmentFit::front(float x, float y, float gap) const noexcept {
    const auto radii = profile(y);
    const float normalized = radii.x > 0.0F ? x * x / (radii.x * radii.x) : 1.0F;
    return {x, y, radii.y * std::sqrt(std::max(.04F, 1.0F - normalized)) + gap};
}

float EquipmentFit::headBottom(float theta) const noexcept {
    if (!headgear_kind) return std::numeric_limits<float>::infinity();
    const float front_weight = std::max(0.0F, std::cos(theta));
    const float side_weight = std::abs(std::sin(theta));
    if (*headgear_kind == EquipmentKind::Helmet) {
        const float side = headgear_visual.style == "light" ? .014F : .006F;
        return std::max(.926F + front_weight * .031F + side_weight * side,
                        hair_floor * front_weight + .929F * (1.0F - front_weight));
    }
    return std::max(.949F - std::max(0.0F, -std::cos(theta)) * .009F,
                    hair_floor + .001F);
}

foundation::Vec3 EquipmentFit::facePoint(float y, float theta) const noexcept {
    const auto section = FaceAnatomyEvaluator::sectionAt(face_anatomy, y * height);
    const float radius_x = section.half_width / height;
    const float radius_z = section.half_depth / height;
    const float center_z = section.center_z / height;
    const float sx = std::sin(theta);
    const float cz = std::cos(theta);
    const auto bell = [](float a, float b = 0.0F) noexcept {
        return std::exp(-a * a - b * b);
    };
    const float chin_y = face_anatomy.face.chin.y / height;
    const float chin = bell((y - (chin_y + .014F)) / .020F);
    const float side = smooth((std::abs(sx) - .10F) / .72F);
    const float x = radius_x * sx *
        (1.0F + (face.chin_width_scale - 1.0F) * .30F * chin * side);
    float z = center_z + radius_z * cz;
    if (cz > 0.0F && y > .882F) {
        const float front = smooth((cz - .02F) / .48F);
        const float cheek = bell((std::abs(x) - .030F * face.cheekbone_scale) / .014F,
                                 (y - (.925F + face.cheekbone_y)) / .016F);
        z += (.0014F + face.cheek_fullness * .42F +
              (face.cheekbone_scale - 1.0F) * .0018F) * cheek * front;
        z += face.chin_projection * .30F *
             bell(x / .023F, (y - (chin_y + .014F)) / .020F) * front;
        z += face.brow_ridge *
             bell((std::abs(x) - face.eye_spacing_ratio) / .017F,
                  (y - (face.eye_y_ratio + .010F)) / .009F) * cz;
        z += face.midface_projection * .38F *
             bell(x / .034F, (y - .918F) / .024F) * cz;
        z += face.forehead_slope * smooth((y - .947F) / .045F) * cz;
    }
    return {x, y, z};
}

float EquipmentFit::faceFrontZ(float x, float y) const noexcept {
    const auto section = FaceAnatomyEvaluator::sectionAt(face_anatomy, y * height);
    const float radius_x = std::max(.00001F, section.half_width / height);
    const float sx = std::clamp(x / radius_x, -.9999F, .9999F);
    const float cz = std::sqrt(std::max(0.0F, 1.0F - sx * sx));
    const auto bell = [](float a, float b = 0.0F) noexcept {
        return std::exp(-a * a - b * b);
    };
    const float chin_y = face_anatomy.face.chin.y / height;
    const float chin = bell((y - (chin_y + .014F)) / .020F);
    const float side = smooth((std::abs(sx) - .10F) / .72F);
    const float px = radius_x * sx *
        (1.0F + (face.chin_width_scale - 1.0F) * .30F * chin * side);
    float z = section.center_z / height + section.half_depth / height * cz;
    if (cz > 0.0F && y > .882F) {
        const float front = smooth((cz - .02F) / .48F);
        const float cheek = bell((std::abs(px) - .030F * face.cheekbone_scale) / .014F,
                                 (y - (.925F + face.cheekbone_y)) / .016F);
        z += (.0014F + face.cheek_fullness * .42F +
              (face.cheekbone_scale - 1.0F) * .0018F) * cheek * front;
        z += face.chin_projection * .30F *
             bell(px / .023F, (y - (chin_y + .014F)) / .020F) * front;
        z += face.brow_ridge *
             bell((std::abs(px) - face.eye_spacing_ratio) / .017F,
                  (y - (face.eye_y_ratio + .010F)) / .009F) * cz;
        z += face.midface_projection * .38F *
             bell(px / .034F, (y - .918F) / .024F) * cz;
        z += face.forehead_slope * smooth((y - .947F) / .045F) * cz;
    }
    return z;
}

bool EquipmentFit::valid(const SkeletonData& skeleton) const noexcept {
    if (version == 0U || identity == 0) {
        return false;
    }
    for (const EquipmentFitSlot& slot : slots) {
        if (!slot.occupied) {
            continue;
        }
        if (skeleton.find(slot.bone) == nullptr || !finite(slot.anchor) ||
            !finite(slot.dimensions) || !(slot.scale > 0.0F) ||
            !std::isfinite(slot.scale) || !std::isfinite(slot.thickness) ||
            slot.dimensions.x <= 0.0F || slot.dimensions.y <= 0.0F || slot.dimensions.z <= 0.0F) {
            return false;
        }
    }
    return true;
}

foundation::Result<EquipmentFit, foundation::Error> EquipmentFitter::build(
    const EquipmentState& state, const PhenotypeArtifact& phenotype,
    const SkeletonData& skeleton) {
    const BodyPhenotype& body = phenotype.body;
    if (!state.valid() || !phenotype.valid() || !skeleton.valid()) {
        return foundation::Result<EquipmentFit, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid equipment fit input"});
    }
    EquipmentFit result{};
    const auto face_anatomy = FaceAnatomyEvaluator::resolve(phenotype);
    if (!face_anatomy) return foundation::Result<EquipmentFit, foundation::Error>::failure(
        face_anatomy.error());
    result.height = body.height;
    result.reference_height = body.reference_height;
    result.hip_y = body.hip_y;
    result.reference_hip_y = body.reference_hip_y;
    result.body = body;
    result.face = phenotype.face;
    result.face_anatomy = face_anatomy.value();
    const float eye_height = .0034F * phenotype.face.eye_height_scale;
    result.hair_floor = -std::numeric_limits<float>::infinity();
    for (const float sign : {1.0F, -1.0F}) {
        const float eye_y = phenotype.face.eye_y_ratio + sign * phenotype.face.eye_asymmetry * .5F;
        const float brow_y = std::max(
            phenotype.face.brow_y_ratio + sign * phenotype.face.brow_asymmetry * .5F,
            eye_y + eye_height + .006F);
        result.hair_floor = std::max(result.hair_floor,
                                     std::max(brow_y, brow_y + phenotype.face.brow_tilt * .006F));
    }
    result.hair_floor += .006F;
    result.identity = foundation::stableHashCombine(
        foundation::stableHashCombine(
            foundation::stableHashCombine(foundation::stable_id("infantry.equipment.fit.v2"),
                                           state.identity),
            phenotype.cache_key),
        skeleton.cacheKey());
    float shirt_ease = 1.0F;
    if (const auto* shirt = state.item(EquipmentSlot::TorsoBase)) {
        if (const auto* definition = EquipmentCatalog::findItem(shirt->definition_id);
            definition != nullptr && definition->visual.ease > 0.0F)
            shirt_ease = definition->visual.ease;
    }
    if (const auto* armor = state.item(EquipmentSlot::TorsoArmor)) {
        if (const auto* definition = EquipmentCatalog::findItem(armor->definition_id))
            result.armor_thickness = definition->visual.thickness / body.height;
    }
    if (const auto* head = state.item(EquipmentSlot::Head)) {
        if (const auto* definition = EquipmentCatalog::findItem(head->definition_id)) {
            result.headgear_kind = definition->kind;
            result.headgear_visual = definition->visual;
        }
    }
    for (std::size_t index = 0U; index < kJacketProfile.size(); ++index) {
        const auto& source = kJacketProfile[index];
        const double chest_t = smoothExact((source[0] - .60) / .14);
        const double shoulder = std::exp(-std::pow((source[0] - .790) / .060, 2.0));
        double width_scale = static_cast<double>(body.waist_width_scale) * (1.0-chest_t) +
            static_cast<double>(body.chest_width_scale)*chest_t;
        width_scale *= 1.0 + (static_cast<double>(body.shoulder_width_scale) - 1.0) * shoulder * .78;
        const double depth_scale = static_cast<double>(body.waist_depth_scale) * (1.0-chest_t) +
            static_cast<double>(body.chest_depth_scale)*chest_t;
        const float mapped_y = result.mapTorsoY(source[0]);
        double radius_x = static_cast<double>(source[1]) * width_scale * shirt_ease;
        double radius_z = static_cast<double>(source[2]) * depth_scale * shirt_ease;
        if (source[0] >= .841F) {
            const auto neck = FaceAnatomyEvaluator::sectionAt(result.face_anatomy,
                                                               mapped_y * body.height);
            const float t = smooth((source[0] - .825F) / .034F);
            radius_x = radius_x + (static_cast<double>(neck.half_width / body.height + .0025F)-radius_x)*t;
            radius_z = radius_z + (static_cast<double>(neck.half_depth / body.height + .0025F)-radius_z)*t;
        }
        result.jacket[index] = {mapped_y, static_cast<float>(radius_x), static_cast<float>(radius_z), 0.0F,
                                result.mapTorsoYExact(static_cast<double>(source[0])),
                                static_cast<double>(radius_x), static_cast<double>(radius_z)};
    }
    if (const auto* pack = state.item(EquipmentSlot::Back)) {
        if (const auto* definition = EquipmentCatalog::findItem(pack->definition_id);
            definition != nullptr && definition->visual.size.x > 0.0F) {
            const float scale = std::clamp(.97F + .10F * (body.shoulder_width_scale - 1.0F),
                                           .90F, 1.08F) * pack->variant.size;
            result.pack_dimensions = {definition->visual.size.x / body.height * scale,
                                      definition->visual.size.y / body.height * scale,
                                      definition->visual.size.z / body.height};
        }
    }
    const auto normalized_bone = [&skeleton, &body](BoneId bone) {
        const auto* record = skeleton.find(bone);
        return record == nullptr ? foundation::Vec3{} : foundation::Vec3{
            record->world_bind.translation.x / body.height,
            record->world_bind.translation.y / body.height,
            record->world_bind.translation.z / body.height};
    };
    for (const auto& bone : skeleton.bones()) {
        result.bind_points[boneIndex(bone.id)] = {
            bone.world_bind.translation.x / body.height,
            bone.world_bind.translation.y / body.height,
            bone.world_bind.translation.z / body.height};
    }
    const auto set_socket = [&result](EquipmentSocketId id, BoneId bone,
                                      foundation::Vec3 position,
                                      foundation::Vec3 normal = {0.0F, 0.0F, 1.0F}) {
        result.sockets[static_cast<std::size_t>(id)] = {bone, position, normal};
    };
    const float torso_y = result.mapTorsoY(.725F);
    const auto torso_profile = result.profile(torso_y);
    const auto waist_profile = result.profile(result.hip_y + .012F);
    const float eye_y = phenotype.face.eye_y_ratio;
    set_socket(EquipmentSocketId::Head, BoneId::Head, normalized_bone(BoneId::Head));
    set_socket(EquipmentSocketId::HeadFront, BoneId::Head,
               {0.0F, eye_y, result.faceFrontZ(0.0F, eye_y)});
    set_socket(EquipmentSocketId::Neck, BoneId::Neck, {0.0F, .866F, 0.0F});
    set_socket(EquipmentSocketId::ChestCenter, BoneId::Chest,
               result.front(0.0F, torso_y, .003F + result.armor_thickness));
    set_socket(EquipmentSocketId::ChestLeft, BoneId::Chest,
               result.front(torso_profile.x * .48F, result.mapTorsoY(.78F),
                            .004F + result.armor_thickness));
    set_socket(EquipmentSocketId::BackCenter, BoneId::SpineUpper,
               {0.0F, torso_y, -torso_profile.y - .008F - result.armor_thickness},
               {0.0F, 0.0F, -1.0F});
    set_socket(EquipmentSocketId::Waist, BoneId::Hips,
               {0.0F, result.hip_y + .008F, 0.0F});
    set_socket(EquipmentSocketId::WaistFront, BoneId::Hips,
               {0.0F, result.hip_y - .007F, waist_profile.y + .012F});
    set_socket(EquipmentSocketId::WaistBack, BoneId::Hips,
               {0.0F, result.hip_y - .006F, -waist_profile.y - .012F},
               {0.0F, 0.0F, -1.0F});
    const float hip_half = body.hip_width / (2.0F * body.height);
    for (std::size_t side = 0U; side < 2U; ++side) {
        const bool left = side == 0U;
        const float sign = left ? 1.0F : -1.0F;
        const BoneId thigh = left ? BoneId::ThighL : BoneId::ThighR;
        const BoneId hand = left ? BoneId::HandL : BoneId::HandR;
        const BoneId foot = left ? BoneId::FootL : BoneId::FootR;
        set_socket(left ? EquipmentSocketId::HipL : EquipmentSocketId::HipR,
                   BoneId::Hips,
                   {sign * (waist_profile.x + .014F), result.hip_y - .018F, 0.0F},
                   {sign, 0.0F, 0.0F});
        auto thigh_position = normalized_bone(thigh);
        thigh_position.x += sign * (.054F * body.leg_thickness_scale + .013F);
        thigh_position.y -= .062F;
        set_socket(left ? EquipmentSocketId::ThighL : EquipmentSocketId::ThighR,
                   thigh, thigh_position, {sign, 0.0F, 0.0F});
        set_socket(left ? EquipmentSocketId::HandL : EquipmentSocketId::HandR,
                   hand, normalized_bone(hand));
        set_socket(left ? EquipmentSocketId::FootL : EquipmentSocketId::FootR,
                   foot, normalized_bone(foot));
    }
    set_socket(EquipmentSocketId::WeaponBack, BoneId::SpineUpper,
               {-torso_profile.x * .30F, torso_y,
                -torso_profile.y - .025F - result.pack_dimensions.z -
                    result.armor_thickness},
               {0.0F, 0.0F, -1.0F});
    set_socket(EquipmentSocketId::WeaponHip, BoneId::ThighR,
               {-hip_half - .048F * body.leg_thickness_scale,
                result.hip_y - .092F, .040F},
               {-.7F, 0.0F, .7F});
    for (std::size_t index = 0U; index < kEquipmentSlotCount; ++index) {
        const EquipmentSlot slot = static_cast<EquipmentSlot>(index);
        EquipmentFitSlot& output = result.slots[index];
        output.slot = slot;
        const EquipmentItem* item = state.item(slot);
        if (item == nullptr) {
            continue;
        }
        const EquipmentItemDefinition* definition = EquipmentCatalog::findItem(item->definition_id);
        BoneId bone = anchorBone(slot);
        const BoneRecord* anchor = skeleton.find(bone);
        if (definition == nullptr || anchor == nullptr) {
            return foundation::Result<EquipmentFit, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState, "equipment fit anchor is missing"});
        }
        output.occupied = true;
        output.bone = bone;
        output.anchor = anchor->world_bind.translation;
        if (const auto socket_id = socketFor(slot)) {
            const auto& socket = result.socket(*socket_id);
            output.bone = socket.bone;
            output.anchor = socket.position;
        }
        output.scale = definition->fit_scale * item->variant.size;
        if(slot==EquipmentSlot::Legs)result.pants_ease=definition->visual.ease;
        if(slot==EquipmentSlot::Feet){result.boot_width=definition->visual.width;
            result.boot_shaft=definition->visual.shaft;}
        if(slot==EquipmentSlot::Hands){result.gloves_present=!definition->style.empty();
            result.gloves_fingerless=definition->style=="fingerless";}
        output.thickness = definition->visual.thickness > 0.0F
            ? definition->visual.thickness / body.height
            : definition->fit_thickness;
        if (slot == EquipmentSlot::Back && definition->visual.size.x > 0.0F) {
            const float pack_scale = std::clamp(
                0.97F + 0.10F * (body.shoulder_width_scale - 1.0F), 0.90F, 1.08F) *
                item->variant.size;
            output.dimensions = {
                definition->visual.size.x / body.height * pack_scale,
                definition->visual.size.y / body.height * pack_scale,
                definition->visual.size.z / body.height};
            output.scale = pack_scale;
        } else {
            output.dimensions = baseDimensions(slot, body);
            output.dimensions = {output.dimensions.x * output.scale,
                                 output.dimensions.y * output.scale,
                                 output.dimensions.z * output.scale};
        }
    }
    return result.valid(skeleton)
               ? foundation::Result<EquipmentFit, foundation::Error>::success(std::move(result))
               : foundation::Result<EquipmentFit, foundation::Error>::failure(
                     {foundation::ErrorCode::InvalidState, "equipment fit constraints are infeasible"});
}

bool GearArtifact::valid(const SkeletonData& skeleton) const noexcept {
    if (version == 0U || cache_key == 0 || !equipment.valid() || !fit.valid(skeleton) ||
        detail_level < 1U || detail_level > 3U || !std::isfinite(wear) || wear < 0.0F ||
        wear > 1.0F || !finite(palette)) {
        return false;
    }
    for (const GearPiece& piece : pieces) {
        if (piece.definition_id == 0 || skeleton.find(piece.bone) == nullptr ||
            !finite(piece.center) || !finite(piece.dimensions) || !finite(piece.color) ||
            piece.dimensions.x <= 0.0F || piece.dimensions.y <= 0.0F || piece.dimensions.z <= 0.0F) {
            return false;
        }
    }
    return true;
}

foundation::Result<GearArtifact, foundation::Error> GearGenerator::build(
    const EquipmentState& state,
    const EquipmentFit& fit,
    const SkeletonData& skeleton,
    foundation::Color palette,
    std::uint32_t detail_level,
    float wear) {
    if (!state.valid() || !fit.valid(skeleton) || !finite(palette) ||
        detail_level < 1U || detail_level > 3U || !std::isfinite(wear) ||
        wear < 0.0F || wear > 1.0F) {
        return foundation::Result<GearArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid gear generation input"});
    }
    GearArtifact result{};
    result.equipment = state;
    result.fit = fit;
    result.palette = palette;
    result.detail_level = detail_level;
    result.wear = wear;
    result.cache_key = foundation::stableHashCombine(
        foundation::stableHashCombine(foundation::stable_id("infantry.gear.v1"), state.identity),
        fit.identity);
    result.cache_key = foundation::stableHashCombine(
        foundation::stableHashCombine(result.cache_key, foundation::stableHashFloat(palette.r)),
        foundation::stableHashCombine(foundation::stableHashFloat(palette.g),
                                       foundation::stableHashFloat(palette.b)));
    result.cache_key = foundation::stableHashCombine(result.cache_key, detail_level);
    result.cache_key = foundation::stableHashCombine(
        result.cache_key, foundation::stableHashFloat(wear));
    for (std::size_t index = 0U; index < kEquipmentSlotCount; ++index) {
        const EquipmentSlot slot = static_cast<EquipmentSlot>(index);
        const EquipmentItem* item = state.item(slot);
        const EquipmentFitSlot& fitted = fit.slots[index];
        if (item == nullptr || !fitted.occupied) {
            continue;
        }
        GearPiece piece{};
        piece.slot = slot;
        piece.definition_id = item->definition_id;
        piece.bone = fitted.bone;
        piece.center = fitted.anchor;
        piece.dimensions = fitted.dimensions;
        piece.color = {std::clamp(palette.r * item->variant.shade, 0.0F, 1.0F),
                       std::clamp(palette.g * item->variant.shade, 0.0F, 1.0F),
                       std::clamp(palette.b * item->variant.shade, 0.0F, 1.0F), palette.a};
        const float fade = .018F * wear;
        piece.color.r += (1.0F - piece.color.r) * fade;
        piece.color.g += (1.0F - piece.color.g) * fade;
        piece.color.b += (1.0F - piece.color.b) * fade;
        piece.material_region = materialRegionFor(slot);
        result.pieces.push_back(piece);
    }
    return result.valid(skeleton)
               ? foundation::Result<GearArtifact, foundation::Error>::success(std::move(result))
               : foundation::Result<GearArtifact, foundation::Error>::failure(
                     {foundation::ErrorCode::InvalidState, "gear artifact failed validation"});
}

GearCache::Artifact GearCache::find(foundation::StableId key) const {
    std::scoped_lock lock(mutex_);
    const auto iterator = entries_.find(key);
    return iterator == entries_.end() ? Artifact{} : iterator->second;
}

GearCache::Artifact GearCache::acquire(const EquipmentState& state,
                                       const EquipmentFit& fit,
                                       const SkeletonData& skeleton,
                                       foundation::Color palette,
                                       std::uint32_t detail_level,
                                       float wear) {
    const foundation::StableId key = foundation::stableHashCombine(
        foundation::stableHashCombine(foundation::stable_id("infantry.gear.cache.v1"),
                                      state.identity),
        fit.identity);
    const foundation::StableId palette_key = foundation::stableHashCombine(
        foundation::stableHashCombine(key, foundation::stableHashFloat(palette.r)),
        foundation::stableHashCombine(foundation::stableHashFloat(palette.g),
                                      foundation::stableHashFloat(palette.b)));
    foundation::StableId complete_key = foundation::stableHashCombine(palette_key, detail_level);
    complete_key = foundation::stableHashCombine(complete_key,
                                                  foundation::stableHashFloat(wear));
    if (Artifact cached = find(complete_key)) {
        return cached;
    }
    const auto built = GearGenerator::build(state, fit, skeleton, palette, detail_level, wear);
    if (!built) {
        return {};
    }
    Artifact artifact = std::make_shared<GearArtifact>(built.value());
    std::scoped_lock lock(mutex_);
    const auto [iterator, inserted] = entries_.emplace(complete_key, artifact);
    return inserted ? artifact : iterator->second;
}

std::size_t GearCache::size() const noexcept {
    std::scoped_lock lock(mutex_);
    return entries_.size();
}

void GearCache::clear() {
    std::scoped_lock lock(mutex_);
    entries_.clear();
}

} // namespace genomes::infantry
