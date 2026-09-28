#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/proc/RandomStream.hpp>

#include <algorithm>
#include <cmath>

namespace genomes::infantry {

namespace {

[[nodiscard]] bool finite(float value) noexcept { return std::isfinite(value); }

[[nodiscard]] float normalizedGene(float value, float minimum, float maximum) noexcept {
    return std::clamp((value - minimum) / (maximum - minimum), 0.0F, 1.0F);
}

[[nodiscard]] float vary(float value, float scale) noexcept {
    return std::clamp(0.5F + (value - 0.5F) * scale, 0.0F, 1.0F);
}

[[nodiscard]] float varied(proc::RandomStream& random) noexcept {
    const float signed_value = static_cast<float>(random.uniform01() * 2.0 - 1.0);
    const float magnitude = std::pow(std::abs(signed_value), 1.30F);
    return std::clamp(0.5F + 0.5F * (signed_value < 0.0F ? -magnitude : magnitude), 0.0F,
                      1.0F);
}

[[nodiscard]] float centred(proc::RandomStream& random) noexcept {
    return static_cast<float>((random.uniform01() + random.uniform01()) * 0.5);
}

void hashOptional(std::uint64_t& hash, const std::optional<float>& value) noexcept {
    hash = foundation::stableHashCombine(hash, value.has_value() ? 1U : 0U);
    if (value.has_value()) {
        hash = foundation::stableHashCombine(hash, foundation::stableHashFloat(*value));
    }
}

} // namespace

bool BodyGenes::valid() const noexcept {
    const float values[] = {frame, mass, musculature, adiposity, shoulder_width, chest_depth,
                            hip_width, torso_leg_ratio, arm_length, leg_length, waist_width,
                            limb_thickness, neck_thickness, head_scale, hand_scale, foot_scale,
                            skin_tone};
    for (const float value : values) {
        if (!finite(value) || value < 0.0F || value > 1.0F) {
            return false;
        }
    }
    return true;
}

bool FaceGenes::valid() const noexcept {
    const float values[] = {head_width, head_depth, head_length, forehead_width, forehead_slope,
                            temple_width, brow_ridge, jaw_width, jaw_length, jaw_angle,
                            chin_width, chin_height, chin_projection, cheekbone_width,
                            cheekbone_height, cheek_fullness, midface_projection, eye_spacing,
                            eye_size, eye_width, eye_height, eye_roundness, eye_depth, eye_tilt,
                            eye_vertical, eye_color, brow_height, brow_thickness, brow_tilt,
                            brow_spacing, nose_length, nose_width, nose_projection, nose_bridge,
                            nose_tip_width, nose_tip_rotation, nostril_width, mouth_width,
                            upper_lip, lower_lip, mouth_height, ear_size, ear_angle, hair_color,
                            hair_brightness, hair_style, hair_density, hair_thickness, hair_volume,
                            hairline, temple_recession, widow_peak, resting_eye_openness,
                            resting_brow, resting_mouth, eye_height_asymmetry,
                            brow_height_asymmetry, mouth_corner_asymmetry, ear_asymmetry,
                            blink_rate, blink_speed, gaze_restlessness, expression_scale,
                            eye_expression, mouth_expression, brow_expression};
    for (const float value : values) {
        if (!finite(value) || value < 0.0F || value > 1.0F) {
            return false;
        }
    }
    return true;
}

bool GenomeOverrides::valid() const noexcept {
    const auto valid_optional = [](const std::optional<float>& value) {
        return !value.has_value() || std::isfinite(*value);
    };
    return valid_optional(height) && valid_optional(shoulder_width) &&
           valid_optional(chest_depth) && valid_optional(hip_width) && valid_optional(arm_length) &&
           valid_optional(leg_length) && valid_optional(eye_spacing) && valid_optional(eye_size) &&
           valid_optional(nose_length) && valid_optional(nose_width) &&
           valid_optional(mouth_width) && valid_optional(jaw_width) && valid_optional(hairline);
}

foundation::StableId GenomeOverrides::hash() const noexcept {
    std::uint64_t result = foundation::stable_id("genomes.infantry.overrides.v1");
    hashOptional(result, height);
    hashOptional(result, shoulder_width);
    hashOptional(result, chest_depth);
    hashOptional(result, hip_width);
    hashOptional(result, arm_length);
    hashOptional(result, leg_length);
    hashOptional(result, eye_spacing);
    hashOptional(result, eye_size);
    hashOptional(result, nose_length);
    hashOptional(result, nose_width);
    hashOptional(result, mouth_width);
    hashOptional(result, jaw_width);
    hashOptional(result, hairline);
    return result == 0 ? 1 : result;
}

bool InfantryGenome::valid() const noexcept {
    return version == InfantryGenomeVersion && finite(height) && height > 0.5F && height < 3.5F &&
           finite(move_speed) && move_speed >= 0.0F && move_speed <= 20.0F &&
           finite(perception_radius) && perception_radius > 0.0F && finite(attack_range) &&
           attack_range > 0.0F && finite(max_health) && max_health > 0.0F &&
           finite(diversity_scale) && diversity_scale >= 0.0F && diversity_scale <= 2.0F &&
           body.valid() && face.valid();
}

foundation::Result<InfantryGenome, foundation::Error> InfantryGenome::generate(
    proc::Seed seed, float diversity_scale) {
    if (!std::isfinite(diversity_scale) || diversity_scale < 0.0F || diversity_scale > 2.0F) {
        return foundation::Result<InfantryGenome, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid infantry diversity scale"});
    }
    InfantryGenome result{};
    result.seed = seed;
    result.diversity_scale = diversity_scale;
    const proc::SeedPath root(seed);
    proc::RandomStream identity(root.child("identity", 0));
    proc::RandomStream body_stream(root.child("body", 0));
    proc::RandomStream face_stream(root.child("face", 0));
    result.height = static_cast<float>(identity.uniformRange(1.60, 1.95));
    result.move_speed = static_cast<float>(identity.uniformRange(2.6, 3.6));
    result.perception_radius = static_cast<float>(identity.uniformRange(50.0, 70.0));
    result.attack_range = static_cast<float>(identity.uniformRange(30.0, 40.0));
    result.max_health = static_cast<float>(identity.uniformRange(90.0, 110.0));
    result.appearance_variant = identity.bounded(16U);
    result.body.frame = varied(body_stream);
    result.body.mass = varied(body_stream);
    result.body.musculature = varied(body_stream);
    result.body.adiposity = varied(body_stream);
    result.body.shoulder_width = varied(body_stream);
    result.body.hip_width = varied(body_stream);
    result.body.torso_leg_ratio = varied(body_stream);
    result.body.arm_length = varied(body_stream);
    result.body.leg_length = varied(body_stream);
    result.body.chest_depth = varied(body_stream);
    result.body.waist_width = varied(body_stream);
    result.body.limb_thickness = varied(body_stream);
    result.body.neck_thickness = varied(body_stream);
    result.body.head_scale = varied(body_stream);
    result.body.hand_scale = varied(body_stream);
    result.body.foot_scale = varied(body_stream);
    result.body.skin_tone = varied(body_stream);

    result.face.head_width = varied(face_stream);
    result.face.head_depth = varied(face_stream);
    result.face.head_length = varied(face_stream);
    result.face.forehead_width = varied(face_stream);
    result.face.forehead_slope = varied(face_stream);
    result.face.temple_width = varied(face_stream);
    result.face.brow_ridge = varied(face_stream);
    result.face.jaw_width = varied(face_stream);
    result.face.jaw_length = varied(face_stream);
    result.face.jaw_angle = varied(face_stream);
    result.face.chin_width = varied(face_stream);
    result.face.chin_height = varied(face_stream);
    result.face.chin_projection = varied(face_stream);
    result.face.cheekbone_width = varied(face_stream);
    result.face.cheekbone_height = varied(face_stream);
    result.face.cheek_fullness = varied(face_stream);
    result.face.midface_projection = varied(face_stream);
    result.face.eye_spacing = varied(face_stream);
    result.face.eye_width = varied(face_stream);
    result.face.eye_height = varied(face_stream);
    result.face.eye_roundness = varied(face_stream);
    result.face.eye_depth = varied(face_stream);
    result.face.eye_tilt = varied(face_stream);
    result.face.eye_vertical = varied(face_stream);
    result.face.eye_color = static_cast<float>(face_stream.uniform01());
    result.face.brow_height = varied(face_stream);
    result.face.brow_thickness = varied(face_stream);
    result.face.brow_tilt = varied(face_stream);
    result.face.brow_spacing = varied(face_stream);
    result.face.nose_width = varied(face_stream);
    result.face.nose_length = varied(face_stream);
    result.face.nose_projection = varied(face_stream);
    result.face.nose_bridge = varied(face_stream);
    result.face.nose_tip_width = varied(face_stream);
    result.face.nose_tip_rotation = varied(face_stream);
    result.face.nostril_width = varied(face_stream);
    result.face.mouth_width = varied(face_stream);
    result.face.upper_lip = varied(face_stream);
    result.face.lower_lip = varied(face_stream);
    result.face.mouth_height = varied(face_stream);
    result.face.ear_size = varied(face_stream);
    result.face.ear_angle = varied(face_stream);
    result.face.hair_color = static_cast<float>(face_stream.uniform01());
    result.face.hair_brightness = centred(face_stream);
    result.face.hair_style = static_cast<float>(face_stream.uniform01());
    result.face.hair_density = varied(face_stream);
    result.face.hair_thickness = varied(face_stream);
    result.face.hair_volume = varied(face_stream);
    result.face.hairline = varied(face_stream);
    result.face.temple_recession = varied(face_stream);
    result.face.widow_peak = varied(face_stream);
    result.face.resting_eye_openness = centred(face_stream);
    result.face.resting_brow = centred(face_stream);
    result.face.resting_mouth = centred(face_stream);
    result.face.eye_height_asymmetry = centred(face_stream);
    result.face.brow_height_asymmetry = centred(face_stream);
    result.face.mouth_corner_asymmetry = centred(face_stream);
    result.face.ear_asymmetry = centred(face_stream);
    result.face.blink_rate = centred(face_stream);
    result.face.blink_speed = centred(face_stream);
    result.face.gaze_restlessness = centred(face_stream);
    result.face.expression_scale = centred(face_stream);
    result.face.eye_expression = centred(face_stream);
    result.face.mouth_expression = centred(face_stream);
    result.face.brow_expression = centred(face_stream);
    return result.applyVariation(diversity_scale);
}

foundation::Result<InfantryGenome, foundation::Error> InfantryGenome::applyVariation(
    float scale) const {
    if (!valid() || !std::isfinite(scale) || scale < 0.0F || scale > 2.0F) {
        return foundation::Result<InfantryGenome, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid infantry genome variation"});
    }
    InfantryGenome result = *this;
    result.diversity_scale = scale;
    const float height_gene = vary(normalizedGene(height, 1.60F, 1.95F), scale);
    result.height = 1.60F + height_gene * 0.35F;
    result.body.shoulder_width = vary(body.shoulder_width, scale);
    result.body.chest_depth = vary(body.chest_depth, scale);
    result.body.hip_width = vary(body.hip_width, scale);
    result.body.arm_length = vary(body.arm_length, scale);
    result.body.leg_length = vary(body.leg_length, scale);
    result.body.frame = vary(body.frame, scale);
    result.body.mass = vary(body.mass, scale);
    result.body.musculature = vary(body.musculature, scale);
    result.body.adiposity = vary(body.adiposity, scale);
    result.body.torso_leg_ratio = vary(body.torso_leg_ratio, scale);
    result.body.waist_width = vary(body.waist_width, scale);
    result.body.limb_thickness = vary(body.limb_thickness, scale);
    result.body.neck_thickness = vary(body.neck_thickness, scale);
    result.body.head_scale = vary(body.head_scale, scale);
    result.body.hand_scale = vary(body.hand_scale, scale);
    result.body.foot_scale = vary(body.foot_scale, scale);
    result.body.skin_tone = vary(body.skin_tone, scale);
    result.face.eye_spacing = vary(face.eye_spacing, scale);
    result.face.eye_size = vary(face.eye_size, scale);
    result.face.nose_length = vary(face.nose_length, scale);
    result.face.nose_width = vary(face.nose_width, scale);
    result.face.mouth_width = vary(face.mouth_width, scale);
    result.face.jaw_width = vary(face.jaw_width, scale);
    result.face.hairline = vary(face.hairline, scale);
    result.face.head_width = vary(face.head_width, scale);
    result.face.head_depth = vary(face.head_depth, scale);
    result.face.head_length = vary(face.head_length, scale);
    result.face.forehead_width = vary(face.forehead_width, scale);
    result.face.forehead_slope = vary(face.forehead_slope, scale);
    result.face.temple_width = vary(face.temple_width, scale);
    result.face.brow_ridge = vary(face.brow_ridge, scale);
    result.face.jaw_length = vary(face.jaw_length, scale);
    result.face.jaw_angle = vary(face.jaw_angle, scale);
    result.face.chin_width = vary(face.chin_width, scale);
    result.face.chin_height = vary(face.chin_height, scale);
    result.face.chin_projection = vary(face.chin_projection, scale);
    result.face.cheekbone_width = vary(face.cheekbone_width, scale);
    result.face.cheekbone_height = vary(face.cheekbone_height, scale);
    result.face.cheek_fullness = vary(face.cheek_fullness, scale);
    result.face.midface_projection = vary(face.midface_projection, scale);
    result.face.eye_width = vary(face.eye_width, scale);
    result.face.eye_height = vary(face.eye_height, scale);
    result.face.eye_roundness = vary(face.eye_roundness, scale);
    result.face.eye_depth = vary(face.eye_depth, scale);
    result.face.eye_tilt = vary(face.eye_tilt, scale);
    result.face.eye_vertical = vary(face.eye_vertical, scale);
    result.face.eye_color = vary(face.eye_color, scale);
    result.face.brow_height = vary(face.brow_height, scale);
    result.face.brow_thickness = vary(face.brow_thickness, scale);
    result.face.brow_tilt = vary(face.brow_tilt, scale);
    result.face.brow_spacing = vary(face.brow_spacing, scale);
    result.face.nose_projection = vary(face.nose_projection, scale);
    result.face.nose_bridge = vary(face.nose_bridge, scale);
    result.face.nose_tip_width = vary(face.nose_tip_width, scale);
    result.face.nose_tip_rotation = vary(face.nose_tip_rotation, scale);
    result.face.nostril_width = vary(face.nostril_width, scale);
    result.face.upper_lip = vary(face.upper_lip, scale);
    result.face.lower_lip = vary(face.lower_lip, scale);
    result.face.mouth_height = vary(face.mouth_height, scale);
    result.face.ear_size = vary(face.ear_size, scale);
    result.face.ear_angle = vary(face.ear_angle, scale);
    result.face.hair_color = vary(face.hair_color, scale);
    result.face.hair_brightness = vary(face.hair_brightness, scale);
    result.face.hair_style = vary(face.hair_style, scale);
    result.face.hair_density = vary(face.hair_density, scale);
    result.face.hair_thickness = vary(face.hair_thickness, scale);
    result.face.hair_volume = vary(face.hair_volume, scale);
    result.face.temple_recession = vary(face.temple_recession, scale);
    result.face.widow_peak = vary(face.widow_peak, scale);
    result.face.resting_eye_openness = vary(face.resting_eye_openness, scale);
    result.face.resting_brow = vary(face.resting_brow, scale);
    result.face.resting_mouth = vary(face.resting_mouth, scale);
    result.face.eye_height_asymmetry = vary(face.eye_height_asymmetry, scale);
    result.face.brow_height_asymmetry = vary(face.brow_height_asymmetry, scale);
    result.face.mouth_corner_asymmetry = vary(face.mouth_corner_asymmetry, scale);
    result.face.ear_asymmetry = vary(face.ear_asymmetry, scale);
    result.face.blink_rate = vary(face.blink_rate, scale);
    result.face.blink_speed = vary(face.blink_speed, scale);
    result.face.gaze_restlessness = vary(face.gaze_restlessness, scale);
    result.face.expression_scale = vary(face.expression_scale, scale);
    result.face.eye_expression = vary(face.eye_expression, scale);
    result.face.mouth_expression = vary(face.mouth_expression, scale);
    result.face.brow_expression = vary(face.brow_expression, scale);
    return foundation::Result<InfantryGenome, foundation::Error>::success(result);
}

foundation::Result<InfantryGenome, foundation::Error> InfantryGenome::withOverrides(
    const GenomeOverrides& overrides) const {
    if (!valid() || !overrides.valid()) {
        return foundation::Result<InfantryGenome, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid infantry genome override"});
    }
    InfantryGenome result = *this;
    if (overrides.height) {
        result.height = *overrides.height;
    }
    if (overrides.shoulder_width) {
        result.body.shoulder_width = *overrides.shoulder_width;
    }
    if (overrides.chest_depth) {
        result.body.chest_depth = *overrides.chest_depth;
    }
    if (overrides.hip_width) {
        result.body.hip_width = *overrides.hip_width;
    }
    if (overrides.arm_length) {
        result.body.arm_length = *overrides.arm_length;
    }
    if (overrides.leg_length) {
        result.body.leg_length = *overrides.leg_length;
    }
    if (overrides.eye_spacing) {
        result.face.eye_spacing = *overrides.eye_spacing;
    }
    if (overrides.eye_size) {
        result.face.eye_size = *overrides.eye_size;
    }
    if (overrides.nose_length) {
        result.face.nose_length = *overrides.nose_length;
    }
    if (overrides.nose_width) {
        result.face.nose_width = *overrides.nose_width;
    }
    if (overrides.mouth_width) {
        result.face.mouth_width = *overrides.mouth_width;
    }
    if (overrides.jaw_width) {
        result.face.jaw_width = *overrides.jaw_width;
    }
    if (overrides.hairline) {
        result.face.hairline = *overrides.hairline;
    }
    return result.valid()
               ? foundation::Result<InfantryGenome, foundation::Error>::success(result)
               : foundation::Result<InfantryGenome, foundation::Error>::failure(
                     {foundation::ErrorCode::InvalidArgument,
                      "infantry override is outside the genome domain"});
}

foundation::StableId InfantryGenome::identityHash() const noexcept {
    std::uint64_t result = foundation::stableHashCombine(
        foundation::stable_id("genomes.infantry.genome.v1"), seed);
    result = foundation::stableHashCombine(result, version);
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(height));
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(move_speed));
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(perception_radius));
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(attack_range));
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(max_health));
    result = foundation::stableHashCombine(result, appearance_variant);
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(diversity_scale));
    const float body_values[] = {body.frame, body.mass, body.musculature, body.adiposity,
                                 body.shoulder_width, body.chest_depth, body.hip_width,
                                 body.torso_leg_ratio, body.arm_length, body.leg_length,
                                 body.waist_width, body.limb_thickness, body.neck_thickness,
                                 body.head_scale, body.hand_scale, body.foot_scale, body.skin_tone};
    const float face_values[] = {face.head_width, face.head_depth, face.head_length,
                                 face.forehead_width, face.forehead_slope, face.temple_width,
                                 face.brow_ridge, face.jaw_width, face.jaw_length, face.jaw_angle,
                                 face.chin_width, face.chin_height, face.chin_projection,
                                 face.cheekbone_width, face.cheekbone_height, face.cheek_fullness,
                                 face.midface_projection, face.eye_spacing, face.eye_size,
                                 face.eye_width, face.eye_height, face.eye_roundness, face.eye_depth,
                                 face.eye_tilt, face.eye_vertical, face.eye_color, face.brow_height,
                                 face.brow_thickness, face.brow_tilt, face.brow_spacing,
                                 face.nose_length, face.nose_width, face.nose_projection,
                                 face.nose_bridge, face.nose_tip_width, face.nose_tip_rotation,
                                 face.nostril_width, face.mouth_width, face.upper_lip, face.lower_lip,
                                 face.mouth_height, face.ear_size, face.ear_angle, face.hair_color,
                                 face.hair_brightness, face.hair_style, face.hair_density,
                                 face.hair_thickness, face.hair_volume, face.hairline,
                                 face.temple_recession, face.widow_peak, face.resting_eye_openness,
                                 face.resting_brow, face.resting_mouth, face.eye_height_asymmetry,
                                 face.brow_height_asymmetry, face.mouth_corner_asymmetry,
                                 face.ear_asymmetry, face.blink_rate, face.blink_speed,
                                 face.gaze_restlessness, face.expression_scale, face.eye_expression,
                                 face.mouth_expression, face.brow_expression};
    for (const float value : body_values) {
        result = foundation::stableHashCombine(result, foundation::stableHashFloat(value));
    }
    for (const float value : face_values) {
        result = foundation::stableHashCombine(result, foundation::stableHashFloat(value));
    }
    return result == 0 ? 1 : result;
}

} // namespace genomes::infantry
