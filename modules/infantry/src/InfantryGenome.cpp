#include <genomes/infantry/InfantryGenome.hpp>

#include <algorithm>
#include <cmath>

namespace genomes::infantry {

namespace {

constexpr std::array<std::string_view, GenomeGeneCount> kGenomeGeneNames{
    "heightGene", "speedGene",
    "body.frameGene", "body.massGene", "body.musculatureGene", "body.adiposityGene",
    "body.shoulderBreadthGene", "body.hipBreadthGene", "body.torsoLegRatioGene",
    "body.armLengthGene", "body.legLengthGene", "body.chestDepthGene",
    "body.waistWidthGene", "body.limbThicknessGene", "body.neckThicknessGene",
    "body.headScaleGene", "body.handScaleGene", "body.footScaleGene", "body.skinToneGene",
    "face.headWidthGene", "face.headDepthGene", "face.headLengthGene",
    "face.foreheadWidthGene", "face.foreheadSlopeGene", "face.templeWidthGene",
    "face.browRidgeGene", "face.jawWidthGene", "face.jawLengthGene", "face.jawAngleGene",
    "face.chinWidthGene", "face.chinHeightGene", "face.chinProjectionGene",
    "face.cheekboneWidthGene", "face.cheekboneHeightGene", "face.cheekFullnessGene",
    "face.midfaceProjectionGene", "face.eyeSpacingGene", "face.eyeWidthGene",
    "face.eyeHeightGene", "face.eyeRoundnessGene", "face.eyeDepthGene", "face.eyeTiltGene",
    "face.eyeVerticalGene", "face.eyeColorGene", "face.browHeightGene",
    "face.browThicknessGene", "face.browTiltGene", "face.browSpacingGene",
    "face.noseWidthGene", "face.noseLengthGene", "face.noseProjectionGene",
    "face.noseBridgeGene", "face.noseTipWidthGene", "face.noseTipRotationGene",
    "face.nostrilWidthGene", "face.mouthWidthGene", "face.upperLipGene",
    "face.lowerLipGene", "face.mouthHeightGene", "face.earSizeGene", "face.earAngleGene",
    "face.hairColorGene", "face.hairBrightnessGene", "face.hairStyleGene",
    "face.hairDensityGene", "face.hairThicknessGene", "face.hairVolumeGene",
    "face.hairlineGene", "face.templeRecessionGene", "face.widowPeakGene",
    "face.restingEyeOpennessGene", "face.restingBrowGene", "face.restingMouthGene",
    "face.eyeHeightAsymmetryGene", "face.browHeightAsymmetryGene",
    "face.mouthCornerAsymmetryGene", "face.earAsymmetryGene", "face.blinkRateGene",
    "face.blinkSpeedGene", "face.gazeRestlessnessGene", "face.expressionScaleGene",
    "face.eyeExpressionGene", "face.mouthExpressionGene", "face.browExpressionGene",
};

[[nodiscard]] bool finite(float value) noexcept { return std::isfinite(value); }

[[nodiscard]] float normalizedGene(float value, float minimum, float maximum) noexcept {
    return std::clamp((value - minimum) / (maximum - minimum), 0.0F, 1.0F);
}

[[nodiscard]] double vary(double value, double scale) noexcept {
    return std::clamp(0.5 + (value - 0.5) * scale, 0.0, 1.0);
}

class ReferenceRandom final {
public:
    explicit ReferenceRandom(std::uint32_t seed) noexcept : state_(seed) {}

    [[nodiscard]] double uniform01() noexcept {
        state_ += 0x6D2B79F5U;
        std::uint32_t t = state_;
        t = (t ^ (t >> 15U)) * (t | 1U);
        t ^= t + ((t ^ (t >> 7U)) * (t | 61U));
        const std::uint32_t result = t ^ (t >> 14U);
        return static_cast<double>(result) / 4294967296.0;
    }

private:
    std::uint32_t state_{0};
};

[[nodiscard]] double varied(ReferenceRandom& random) noexcept {
    const double signed_value = static_cast<double>(random.uniform01()) * 2.0 - 1.0;
    const double magnitude = std::pow(std::abs(signed_value), 1.30);
    return std::clamp(0.5 + 0.5 * (signed_value < 0.0 ? -magnitude : magnitude), 0.0, 1.0);
}

[[nodiscard]] float centred(ReferenceRandom& random) noexcept {
    return static_cast<float>((random.uniform01() + random.uniform01()) * 0.5);
}

void hashOptional(std::uint64_t& hash, const std::optional<double>& value) noexcept {
    hash = foundation::stableHashCombine(hash, value.has_value() ? 1U : 0U);
    if (value.has_value()) {
        hash = foundation::stableHashCombine(hash, foundation::stableHashDouble(*value));
    }
}

constexpr std::array<double BodyGenes::*, 17> kBodyGeneMembers{
    &BodyGenes::frame, &BodyGenes::mass, &BodyGenes::musculature, &BodyGenes::adiposity,
    &BodyGenes::shoulder_width, &BodyGenes::hip_width, &BodyGenes::torso_leg_ratio,
    &BodyGenes::arm_length, &BodyGenes::leg_length, &BodyGenes::chest_depth,
    &BodyGenes::waist_width, &BodyGenes::limb_thickness, &BodyGenes::neck_thickness,
    &BodyGenes::head_scale, &BodyGenes::hand_scale, &BodyGenes::foot_scale, &BodyGenes::skin_tone,
};

constexpr std::array<float FaceGenes::*, 65> kFaceGeneMembers{
    &FaceGenes::head_width, &FaceGenes::head_depth, &FaceGenes::head_length,
    &FaceGenes::forehead_width, &FaceGenes::forehead_slope, &FaceGenes::temple_width,
    &FaceGenes::brow_ridge, &FaceGenes::jaw_width, &FaceGenes::jaw_length,
    &FaceGenes::jaw_angle, &FaceGenes::chin_width, &FaceGenes::chin_height,
    &FaceGenes::chin_projection, &FaceGenes::cheekbone_width, &FaceGenes::cheekbone_height,
    &FaceGenes::cheek_fullness, &FaceGenes::midface_projection, &FaceGenes::eye_spacing,
    &FaceGenes::eye_width, &FaceGenes::eye_height, &FaceGenes::eye_roundness,
    &FaceGenes::eye_depth, &FaceGenes::eye_tilt, &FaceGenes::eye_vertical,
    &FaceGenes::eye_color, &FaceGenes::brow_height, &FaceGenes::brow_thickness,
    &FaceGenes::brow_tilt, &FaceGenes::brow_spacing, &FaceGenes::nose_width,
    &FaceGenes::nose_length, &FaceGenes::nose_projection, &FaceGenes::nose_bridge,
    &FaceGenes::nose_tip_width, &FaceGenes::nose_tip_rotation, &FaceGenes::nostril_width,
    &FaceGenes::mouth_width, &FaceGenes::upper_lip, &FaceGenes::lower_lip,
    &FaceGenes::mouth_height, &FaceGenes::ear_size, &FaceGenes::ear_angle,
    &FaceGenes::hair_color, &FaceGenes::hair_brightness, &FaceGenes::hair_style,
    &FaceGenes::hair_density, &FaceGenes::hair_thickness, &FaceGenes::hair_volume,
    &FaceGenes::hairline, &FaceGenes::temple_recession, &FaceGenes::widow_peak,
    &FaceGenes::resting_eye_openness, &FaceGenes::resting_brow, &FaceGenes::resting_mouth,
    &FaceGenes::eye_height_asymmetry, &FaceGenes::brow_height_asymmetry,
    &FaceGenes::mouth_corner_asymmetry, &FaceGenes::ear_asymmetry, &FaceGenes::blink_rate,
    &FaceGenes::blink_speed, &FaceGenes::gaze_restlessness, &FaceGenes::expression_scale,
    &FaceGenes::eye_expression, &FaceGenes::mouth_expression, &FaceGenes::brow_expression,
};

void applyGeneOverride(InfantryGenome& genome, GenomeGene gene, double value) noexcept {
    const auto index = static_cast<std::size_t>(gene);
    if (gene == GenomeGene::Height) {
        genome.height = 1.60F + value * 0.35F;
        genome.reference_height = 1.60 + value * 0.35;
    } else if (gene == GenomeGene::Speed) {
        genome.move_speed = 2.6F + value;
    } else if (index >= static_cast<std::size_t>(GenomeGene::BodyFrame) &&
               index <= static_cast<std::size_t>(GenomeGene::BodySkinTone)) {
        genome.body.*kBodyGeneMembers[index - static_cast<std::size_t>(GenomeGene::BodyFrame)] =
            value;
    } else if (index >= static_cast<std::size_t>(GenomeGene::FaceHeadWidth) &&
               index < static_cast<std::size_t>(GenomeGene::Count)) {
        genome.face.*kFaceGeneMembers[index - static_cast<std::size_t>(GenomeGene::FaceHeadWidth)] =
            value;
    }
}

} // namespace

std::string_view genomeGeneName(GenomeGene gene) noexcept {
    const auto index = static_cast<std::size_t>(gene);
    return index < kGenomeGeneNames.size() ? kGenomeGeneNames[index] : std::string_view{};
}

std::optional<GenomeGene> genomeGeneFromName(std::string_view name) noexcept {
    const auto found = std::find(kGenomeGeneNames.begin(), kGenomeGeneNames.end(), name);
    if (found == kGenomeGeneNames.end()) {
        return std::nullopt;
    }
    return static_cast<GenomeGene>(std::distance(kGenomeGeneNames.begin(), found));
}

bool BodyGenes::valid() const noexcept {
    const double values[] = {frame, mass, musculature, adiposity, shoulder_width, chest_depth,
                            hip_width, torso_leg_ratio, arm_length, leg_length, waist_width,
                            limb_thickness, neck_thickness, head_scale, hand_scale, foot_scale,
                            skin_tone};
    for (const double value : values) {
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
    for (const auto& gene : genes) {
        if (gene && (!std::isfinite(*gene) || *gene < 0.0 || *gene > 1.0)) {
            return false;
        }
    }
    return valid_optional(height) && valid_optional(shoulder_width) &&
           valid_optional(chest_depth) && valid_optional(hip_width) && valid_optional(arm_length) &&
           valid_optional(leg_length) && valid_optional(eye_spacing) && valid_optional(eye_size) &&
           valid_optional(nose_length) && valid_optional(nose_width) &&
           valid_optional(mouth_width) && valid_optional(jaw_width) && valid_optional(hairline);
}

bool GenomeOverrides::set(GenomeGene gene, double value) noexcept {
    const auto index = static_cast<std::size_t>(gene);
    if (index >= genes.size() || !std::isfinite(value) || value < 0.0 || value > 1.0) {
        return false;
    }
    genes[index] = value;
    return true;
}

bool GenomeOverrides::set(std::string_view name, double value) noexcept {
    const auto gene = genomeGeneFromName(name);
    return gene && set(*gene, value);
}

std::optional<double> GenomeOverrides::get(GenomeGene gene) const noexcept {
    const auto index = static_cast<std::size_t>(gene);
    return index < genes.size() ? genes[index] : std::nullopt;
}

std::optional<double> GenomeOverrides::get(std::string_view name) const noexcept {
    const auto gene = genomeGeneFromName(name);
    return gene ? get(*gene) : std::nullopt;
}

foundation::StableId GenomeOverrides::hash() const noexcept {
    std::uint64_t result = foundation::stable_id("genomes.infantry.overrides.v2");
    for (const auto& gene : genes) {
        hashOptional(result, gene);
    }
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
           finite(diversity_scale) && diversity_scale >= 0.0F && diversity_scale <= 1.75F &&
           body.valid() && face.valid();
}

bool isValidVariation(double value) noexcept {
    return std::isfinite(value) && value >= 0.0 && value <= 1.75;
}

foundation::Result<InfantryGenome, foundation::Error> InfantryGenome::generate(
    proc::Seed seed, float diversity_scale) {
    if (!isValidVariation(diversity_scale)) {
        return foundation::Result<InfantryGenome, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid infantry diversity scale"});
    }
    InfantryGenome result{};
    result.seed = seed;
    result.diversity_scale = diversity_scale;
    ReferenceRandom base(static_cast<std::uint32_t>(seed));
    result.reference_height = 1.60 + 0.35 * base.uniform01();
    result.height = static_cast<float>(result.reference_height);
    result.move_speed = 2.6F + 1.0F * base.uniform01();
    result.perception_radius = 50.0F + 20.0F * base.uniform01();
    result.attack_range = 30.0F + 10.0F * base.uniform01();
    result.max_health = 90.0F + 20.0F * base.uniform01();
    result.appearance_variant = static_cast<std::uint32_t>(base.uniform01() * 16.0F);

    ReferenceRandom body_stream(static_cast<std::uint32_t>(seed) ^ 0x8B7A3D11U);
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

    ReferenceRandom face_stream(static_cast<std::uint32_t>(seed) ^ 0x51F15E37U);
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
    result.face.eye_color = face_stream.uniform01();
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
    result.face.hair_color = face_stream.uniform01();
    result.face.hair_brightness = centred(face_stream);
    result.face.hair_style = face_stream.uniform01();
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
    if (!valid() || !isValidVariation(scale)) {
        return foundation::Result<InfantryGenome, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid infantry genome variation"});
    }
    InfantryGenome result = *this;
    result.diversity_scale = scale;
    const float height_gene = vary(normalizedGene(height, 1.60F, 1.95F), scale);
    result.reference_height = 1.60 + height_gene * 0.35;
    result.height = static_cast<float>(result.reference_height);
    const float speed_gene = vary(normalizedGene(move_speed, 2.60F, 3.60F), scale);
    result.move_speed = 2.60F + speed_gene;
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
    for (std::size_t index = 0; index < overrides.genes.size(); ++index) {
        if (overrides.genes[index]) {
            applyGeneOverride(result, static_cast<GenomeGene>(index),
                              *overrides.genes[index]);
        }
    }
    if (overrides.height) {
        result.height = *overrides.height;
        result.reference_height = *overrides.height;
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

double InfantryGenome::geneValue(GenomeGene gene) const noexcept {
    const auto index = static_cast<std::size_t>(gene);
    if (gene == GenomeGene::Height) return normalizedGene(height, 1.60F, 1.95F);
    if (gene == GenomeGene::Speed) return normalizedGene(move_speed, 2.6F, 3.6F);
    if (index >= static_cast<std::size_t>(GenomeGene::BodyFrame) &&
        index <= static_cast<std::size_t>(GenomeGene::BodySkinTone)) {
        return body.*kBodyGeneMembers[index - static_cast<std::size_t>(GenomeGene::BodyFrame)];
    }
    if (index >= static_cast<std::size_t>(GenomeGene::FaceHeadWidth) &&
        index < static_cast<std::size_t>(GenomeGene::Count)) {
        return face.*kFaceGeneMembers[index - static_cast<std::size_t>(GenomeGene::FaceHeadWidth)];
    }
    return 0.0;
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
    const double body_values[] = {body.frame, body.mass, body.musculature, body.adiposity,
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
    for (const double value : body_values) {
        result = foundation::stableHashCombine(result, foundation::stableHashDouble(value));
    }
    for (const float value : face_values) {
        result = foundation::stableHashCombine(result, foundation::stableHashFloat(value));
    }
    return result == 0 ? 1 : result;
}

} // namespace genomes::infantry
