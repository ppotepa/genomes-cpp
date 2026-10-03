#include <genomes/infantry/PhenotypeResolver.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace genomes::infantry {

namespace {

[[nodiscard]] bool finite(float value) noexcept { return std::isfinite(value); }

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

[[nodiscard]] float clampMetric(float value, float minimum, float maximum, bool& changed) noexcept {
    const float resolved = std::clamp(value, minimum, maximum);
    changed = changed || resolved != value;
    return resolved;
}

[[nodiscard]] float clampResolved(double value, double minimum, double maximum) noexcept {
    return static_cast<float>(std::clamp(value, minimum, maximum));
}

[[nodiscard]] std::uint32_t mixHex(std::uint32_t left, std::uint32_t right,
                                   float amount) noexcept {
    const auto channel = [amount](std::uint32_t a, std::uint32_t b) {
        return static_cast<std::uint32_t>(std::floor(
            static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * amount +
            0.5F)) & 255U;
    };
    return (channel((left >> 16U) & 255U, (right >> 16U) & 255U) << 16U) |
           (channel((left >> 8U) & 255U, (right >> 8U) & 255U) << 8U) |
           channel(left & 255U, right & 255U);
}

template <std::size_t Size>
[[nodiscard]] std::uint32_t paletteHex(float gene,
                                       const std::array<std::uint32_t, Size>& colors) noexcept {
    const float position = std::clamp(gene, 0.0F, 0.999999F) * static_cast<float>(Size - 1U);
    const auto index = static_cast<std::size_t>(position);
    return mixHex(colors[index], colors[std::min(Size - 1U, index + 1U)],
                  position - static_cast<float>(index));
}

[[nodiscard]] std::uint32_t shadeHex(std::uint32_t color, float factor) noexcept {
    const auto channel = [factor](std::uint32_t value) {
        return static_cast<std::uint32_t>(std::floor(
            std::clamp(static_cast<float>(value) * factor, 0.0F, 255.0F) + 0.5F)) & 255U;
    };
    return (channel((color >> 16U) & 255U) << 16U) |
           (channel((color >> 8U) & 255U) << 8U) | channel(color & 255U);
}

} // namespace

bool BodyPhenotype::valid() const noexcept {
    return finite(height) && height >= 1.60F && height <= 1.95F && finite(speed_multiplier) &&
           speed_multiplier >= 0.88F && speed_multiplier <= 1.12F && finite(walk_speed) &&
           walk_speed > 0.0F && finite(run_speed) && run_speed > walk_speed &&
           finite(prone_speed) && prone_speed > 0.0F && finite(crouch_speed) &&
           crouch_speed > 0.0F && finite(shoulder_width) &&
           shoulder_width > 0.20F && shoulder_width < 0.90F && finite(chest_depth) &&
           chest_depth > 0.10F && chest_depth < 0.60F && finite(hip_width) && hip_width > 0.12F &&
           hip_width < 0.70F && finite(arm_length) && arm_length > 0.30F && arm_length < 1.20F &&
           finite(leg_length) && leg_length > 0.50F && leg_length < 1.40F &&
           finite(anatomy_leg_length) && anatomy_leg_length > 0.50F &&
           anatomy_leg_length < 1.40F && finite(pelvis) &&
           finite(limb_thickness) && limb_thickness > 0.04F && limb_thickness < 0.30F &&
           finite(chest) && finite(head) && finite(left_hand) && finite(right_hand) &&
           finite(left_foot) && finite(right_foot);
}

bool FacePhenotype::valid() const noexcept {
    return finite(eye_spacing) && eye_spacing > 0.02F && eye_spacing < 0.20F &&
           finite(eye_radius) && eye_radius > 0.003F && eye_radius < 0.05F && finite(brow_y) &&
           finite(eye_y) && finite(nose_y) && finite(nose_length) && nose_length > 0.005F &&
           finite(nose_width) && nose_width > 0.005F && finite(mouth_y) && finite(mouth_width) &&
           mouth_width > 0.01F && finite(upper_lip) && finite(lower_lip) &&
           finite(jaw_width) && jaw_width > 0.05F && finite(head_width) && head_width > 0.08F &&
           finite(head_depth) && head_depth > 0.06F && finite(head_length_scale) &&
           head_length_scale > 0.5F && finite(forehead_width_scale) &&
           forehead_width_scale > 0.5F && finite(forehead_slope) && finite(temple_width_scale) &&
           temple_width_scale > 0.5F && finite(brow_ridge) && finite(jaw_length_scale) &&
           jaw_length_scale > 0.5F && finite(jaw_angle) && jaw_angle > 0.5F &&
           finite(chin_width_scale) && chin_width_scale > 0.4F && finite(chin_height) &&
           finite(chin_projection) && finite(cheekbone_scale) && cheekbone_scale > 0.5F &&
           finite(cheekbone_y) && finite(cheek_fullness) && finite(midface_projection) &&
           finite(eye_width_scale) && eye_width_scale > 0.5F && finite(eye_height_scale) &&
           eye_height_scale > 0.5F && finite(eye_depth) && finite(eye_tilt) &&
           finite(nose_projection_scale) && nose_projection_scale > 0.5F &&
           finite(nose_bridge_scale) && nose_bridge_scale > 0.5F &&
           finite(nose_tip_width_scale) && nose_tip_width_scale > 0.5F &&
           finite(nostril_width_scale) && nostril_width_scale > 0.5F && finite(ear_scale) &&
           ear_scale > 0.5F && finite(ear_angle) && finite(eye_asymmetry) &&
           finite(brow_asymmetry) && finite(mouth_asymmetry) && finite(ear_asymmetry) &&
           finite(temple_recession) && temple_recession >= 0.0F && temple_recession <= 1.0F &&
           finite(widow_peak) && widow_peak >= 0.0F && widow_peak <= 1.0F &&
           finite(hair_brightness) && hair_brightness > 0.0F && hair_brightness < 2.0F &&
           finite(hairline_y) &&
           brow_y > eye_y && eye_y > nose_y && nose_y > mouth_y && hairline_y > brow_y;
}

FaceSection FacePhenotype::section(float y) const noexcept {
    // The reference does not describe a face as one linear ellipse. Keep the
    // same landmark-driven boundary in native space: jaw, mouth, nose, eye,
    // brow and forehead each have a controlled profile and are interpolated
    // between their neighboring anatomical levels.
    struct Profile final {
        float y;
        float radius_x;
        float radius_z;
        float center_z;
    };
    const float jaw_radius = jaw_width * 0.5F;
    const float head_radius = std::max(jaw_radius * 0.78F, head_width * 0.5F);
    const float bottom = mouth_y - 0.09F * jaw_length_scale * head_length_scale;
    const float cheek = std::exp(-((y - (eye_y + cheekbone_y)) / 0.020F) *
                                 ((y - (eye_y + cheekbone_y)) / 0.020F));
    const float temple = std::exp(-((y - 0.951F) / 0.020F) *
                                  ((y - 0.951F) / 0.020F));
    const std::array<Profile, 6U> profile{
        Profile{bottom, jaw_radius * 0.46F * chin_width_scale,
                head_depth * 0.31F, chin_projection},
        Profile{mouth_y + chin_height * 0.30F,
                jaw_radius * (0.70F + 0.10F * (chin_width_scale - 1.0F)),
                head_depth * 0.39F, chin_projection * 0.60F},
        Profile{nose_y, head_radius * (0.78F + 0.10F * (jaw_angle - 1.0F)),
                head_depth * 0.47F + midface_projection, 0.016F + midface_projection * 0.25F},
        Profile{eye_y, head_radius * (1.0F +
                                      (0.10F * (cheekbone_scale - 1.0F) +
                                       0.12F * cheek_fullness) * cheek),
                head_depth * 0.54F + midface_projection,
                0.022F + midface_projection * 0.50F},
        Profile{brow_y, head_radius * (1.02F + 0.10F * (forehead_width_scale - 1.0F)),
                head_depth * 0.53F, 0.022F + brow_ridge},
        Profile{hairline_y, head_radius * (0.88F + 0.15F * (forehead_width_scale - 1.0F)) *
                    (1.0F + 0.20F * (temple_width_scale - 1.0F) * temple),
                head_depth * 0.45F, 0.012F + forehead_slope},
    };
    if (y <= profile.front().y) {
        return {profile.front().radius_x, profile.front().radius_z, profile.front().center_z};
    }
    for (std::size_t index = 0U; index + 1U < profile.size(); ++index) {
        const Profile& lower = profile[index];
        const Profile& upper = profile[index + 1U];
        if (y > upper.y) {
            continue;
        }
        const float t = std::clamp((y - lower.y) / std::max(1.0e-6F, upper.y - lower.y),
                                   0.0F, 1.0F);
        const float smooth = t * t * (3.0F - 2.0F * t);
        return {lower.radius_x + (upper.radius_x - lower.radius_x) * smooth,
                lower.radius_z + (upper.radius_z - lower.radius_z) * smooth,
                lower.center_z + (upper.center_z - lower.center_z) * smooth};
    }
    return {profile.back().radius_x, profile.back().radius_z, profile.back().center_z};
}

foundation::Vec3 FacePhenotype::point(float y, float theta) const noexcept {
    const FaceSection face_section = section(y);
    return {face_section.radius_x * std::cos(theta), y,
            face_section.center_z + face_section.radius_z * std::sin(theta)};
}

float FacePhenotype::frontZ(float y) const noexcept {
    const FaceSection face_section = section(y);
    return face_section.center_z + face_section.radius_z;
}

bool PhenotypeArtifact::valid() const noexcept {
    return version == 1U && cache_key != 0 && requested.valid() && body.valid() && face.valid();
}

foundation::Result<PhenotypeArtifact, foundation::Error> PhenotypeResolver::resolve(
    const InfantryGenome& genome, const GenomeOverrides& overrides) {
    if (!genome.valid() || !overrides.valid()) {
        return foundation::Result<PhenotypeArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid infantry phenotype input"});
    }
    const auto requested_result = genome.withOverrides(overrides);
    if (!requested_result) {
        return foundation::Result<PhenotypeArtifact, foundation::Error>::failure(
            requested_result.error());
    }
    const InfantryGenome& requested = requested_result.value();
    PhenotypeArtifact artifact{};
    artifact.requested = requested;
    artifact.cache_key = foundation::stableHashCombine(
        foundation::stableHashCombine(foundation::stable_id("infantry.phenotype.v1"),
                                      requested.identityHash()),
        overrides.hash());

    BodyPhenotype body{};
    body.height = clampMetric(requested.height, 1.60F, 1.95F, artifact.diagnostics.height_clamped);
    body.reference_height = std::clamp(requested.reference_height, 1.60, 1.95);
    const float speed_gene=std::clamp(requested.move_speed-2.6F,0.0F,1.0F);
    body.speed_multiplier=0.88F+0.24F*speed_gene;
    body.walk_speed=1.4F*body.speed_multiplier;
    body.run_speed=4.2F*body.speed_multiplier;
    body.prone_speed=0.55F*body.speed_multiplier;
    body.crouch_speed=0.75F*body.speed_multiplier;
    if (artifact.diagnostics.height_clamped) {
        artifact.diagnostics.adjustments.push_back(
            {AnatomyParameter::Height, requested.height, body.height,
             AnatomyAdjustmentReason::ClampedToDomain});
    }
    body.frame = 0.90F + requested.body.frame * 0.21F;
    body.mass = 0.84F + requested.body.mass * 0.34F;
    body.muscle = 0.84F + requested.body.musculature * 0.36F;
    body.fat = 0.82F + requested.body.adiposity * 0.38F;
    body.torso_leg_bias = requested.body.torso_leg_ratio * 2.0F - 1.0F;
    const float shoulder_scale = clampResolved(
        (0.82F + requested.body.shoulder_width * 0.40F) *
            (0.96F + requested.body.frame * 0.10F) *
            (0.96F + requested.body.musculature * 0.11F),
        0.78F, 1.28F);
    body.shoulder_width_scale = shoulder_scale;
    const float hip_scale = clampResolved(
        (0.86F + requested.body.hip_width * 0.31F) *
            (0.97F + requested.body.frame * 0.08F) *
            (0.97F + requested.body.adiposity * 0.08F),
        0.82F, 1.23F);
    body.hip_width_scale = hip_scale;
    body.chest_width_scale = clampResolved(
        0.86F + ((requested.body.mass + requested.body.musculature +
                  requested.body.shoulder_width) / 3.0F) * 0.32F,
        0.82F, 1.24F);
    body.chest_depth_scale = clampResolved(
        (0.82F + requested.body.chest_depth * 0.40F) *
            (0.96F + requested.body.mass * 0.12F), 0.80F, 1.28F);
    body.waist_width_scale = clampResolved(
        (0.78F + requested.body.waist_width * 0.44F) *
            (0.93F + requested.body.adiposity * 0.19F), 0.74F, 1.30F);
    body.waist_depth_scale = clampResolved(
        0.82F + ((requested.body.adiposity + requested.body.mass) * 0.5F) * 0.36F,
        0.78F, 1.24F);
    const auto mix_reference=[](double a,double b,double t){return a+(b-a)*t;};
    body.reference_shoulder_width_scale=std::clamp(
        mix_reference(.82,1.22,requested.body.shoulder_width)*
        mix_reference(.96,1.06,requested.body.frame)*
        mix_reference(.96,1.07,requested.body.musculature),.78,1.28);
    body.reference_hip_width_scale=std::clamp(
        mix_reference(.86,1.17,requested.body.hip_width)*
        mix_reference(.97,1.05,requested.body.frame)*
        mix_reference(.97,1.05,requested.body.adiposity),.82,1.23);
    body.reference_chest_width_scale=std::clamp(
        mix_reference(.86,1.18,(requested.body.mass+requested.body.musculature+
              requested.body.shoulder_width)/3.0),.82,1.24);
    body.reference_chest_depth_scale=std::clamp(
        mix_reference(.82,1.22,requested.body.chest_depth)*
        mix_reference(.96,1.08,requested.body.mass),.80,1.28);
    body.reference_waist_width_scale=std::clamp(
        mix_reference(.78,1.22,requested.body.waist_width)*
        mix_reference(.93,1.12,requested.body.adiposity),.74,1.30);
    body.reference_waist_depth_scale=std::clamp(
        mix_reference(.82,1.18,(requested.body.adiposity+requested.body.mass)*.5),.78,1.24);
    body.reference_head_scale=mix_reference(.93,1.07,requested.body.head_scale);
    body.reference_arm_length_scale=mix_reference(.93,1.09,requested.body.arm_length);
    body.reference_hand_scale=mix_reference(.88,1.13,requested.body.hand_scale);
    const double reference_limb_base=mix_reference(.76,1.25,requested.body.limb_thickness);
    body.reference_leg_thickness_scale=std::clamp(
        reference_limb_base*(.94+(1.12-.94)*requested.body.musculature)*
        (.97+(1.06-.97)*requested.body.mass),.72,1.36);
    const float limb_base = 0.76F + requested.body.limb_thickness * 0.49F;
    body.arm_thickness_scale = clampResolved(
        limb_base * (0.92F + requested.body.musculature * 0.20F) *
            (0.97F + requested.body.mass * 0.08F), 0.72F, 1.36F);
    body.leg_thickness_scale = clampResolved(
        limb_base * (0.94F + requested.body.musculature * 0.18F) *
            (0.97F + requested.body.mass * 0.09F), 0.72F, 1.36F);
    body.neck_scale = clampResolved(
        (0.79F + requested.body.neck_thickness * 0.46F) *
            (0.96F + requested.body.frame * 0.12F), 0.76F, 1.32F);
    body.reference_neck_scale = std::clamp(
        (.79 + (1.25 - .79) * requested.body.neck_thickness) *
            (.96 + (1.08 - .96) * requested.body.frame),
        .76, 1.32);
    body.leg_length_scale = 0.94F + requested.body.leg_length * 0.13F;
    body.arm_length_scale = 0.93F + requested.body.arm_length * 0.16F;
    const double reference_leg_length_scale=mix_reference(.94,1.07,
        static_cast<double>(requested.body.leg_length));
    const double reference_torso_leg_bias=mix_reference(-1.0,1.0,
        static_cast<double>(requested.body.torso_leg_ratio));
    body.reference_hip_y = std::clamp(0.54+
        (reference_leg_length_scale-1.0)*.42-
        reference_torso_leg_bias*.020,0.502,0.579);
    body.hip_y = static_cast<float>(body.reference_hip_y);
    body.head_scale = 0.93F + requested.body.head_scale * 0.14F;
    body.hand_scale = 0.88F + requested.body.hand_scale * 0.25F;
    body.foot_scale = 0.89F + requested.body.foot_scale * 0.25F;
    body.shoulder_width = relativeToHeight(0.256F * shoulder_scale, body.height);
    // These are final metre quantities. The reference stores these body
    // proportions in height-relative space; keep the conversion visible at
    // the resolver boundary instead of leaking normalized values into rig or
    // surface generation.
    body.chest_depth = relativeToHeight(0.135F * body.chest_depth_scale, body.height);
    body.hip_width = relativeToHeight(0.104F * hip_scale, body.height);
    body.arm_length = relativeToHeight(
        0.335F * body.arm_length_scale, body.height);
    body.leg_length = relativeToHeight(
        0.495F * body.leg_length_scale, body.height);
    body.anatomy_leg_length = (body.hip_y - 0.060F) * body.height;
    body.waist_width = relativeToHeight(
        0.20F * body.waist_width_scale, body.height);
    body.limb_thickness = relativeToHeight(
        0.075F * (0.76F + requested.body.limb_thickness * 0.49F), body.height);
    body.skin_color = {static_cast<float>(0.42 + requested.body.skin_tone * 0.38),
                       static_cast<float>(0.24 + requested.body.skin_tone * 0.34),
                       static_cast<float>(0.16 + requested.body.skin_tone * 0.28), 1.0F};
    body.skin_color_hex = paletteHex(requested.body.skin_tone,
        std::array<std::uint32_t, 6U>{0x6F4A37U, 0x8F6048U, 0xAE795EU,
                                      0xC79576U, 0xD6AD8EU, 0xE0BD9FU});
    const float requested_shoulder_width = body.shoulder_width;
    body.shoulder_width = clampMetric(body.shoulder_width, 0.25F, 0.78F,
                                      artifact.diagnostics.shoulder_clamped);
    if (artifact.diagnostics.shoulder_clamped) {
        artifact.diagnostics.adjustments.push_back(
            {AnatomyParameter::ShoulderWidth, requested_shoulder_width, body.shoulder_width,
             AnatomyAdjustmentReason::ClampedToDomain});
    }
    // Keep semantic phenotype landmarks on the same normalized-space equations
    // as reference/js/rendering/infantryRig.js, then convert to metres once.
    // Mixing shoulder_y (ratio) with metre arm lengths used to scale hand Y by
    // height twice and made standalone surfaces disagree with the canonical rig.
    const float hip_y = body.hip_y;
    const float neck_y = 0.837F + (0.843F - 0.837F) *
        std::clamp((body.neck_scale - 0.76F) / 0.56F, 0.0F, 1.0F);
    const float chest_y = hip_y + (neck_y - hip_y) * 0.76F;
    const float shoulder_y = chest_y + (neck_y - chest_y) * 0.34F;
    const float arm_angle = 22.0F * 3.14159265358979323846F / 180.0F;
    const float upper_arm_length = 0.18F * body.arm_length_scale * body.height;
    const float forearm_length = 0.155F * body.arm_length_scale * body.height;
    const float shoulder_half = 0.128F * body.shoulder_width_scale * body.height;
    const float hand_x = shoulder_half +
                         std::sin(arm_angle) * (upper_arm_length + forearm_length);
    const float hand_y = body.height * shoulder_y -
                         std::cos(arm_angle) * (upper_arm_length + forearm_length);
    body.pelvis = {0.0F, body.height * hip_y, 0.0F};
    body.chest = {0.0F, body.height * chest_y, 0.0F};
    body.head = {0.0F, body.height * (0.90F + (body.head_scale - 1.0F) * 0.008F), 0.015F};
    body.left_hand = {hand_x, hand_y, 0.0F};
    body.right_hand = {-hand_x, hand_y, 0.0F};
    const float foot_x = 0.052F * body.hip_width_scale * body.height;
    const float foot_y = 0.045F * body.height;
    body.left_foot = {foot_x, foot_y, 0.0F};
    body.right_foot = {-foot_x, foot_y, 0.0F};

    FacePhenotype face{};
    face.reference_head_width_scale=(.86+.30*static_cast<double>(requested.face.head_width)) *
        (.97+.06*static_cast<double>(requested.body.head_scale));
    face.reference_head_depth_scale=.88+.26*static_cast<double>(requested.face.head_depth);
    face.head_width_scale = (0.86F + requested.face.head_width * 0.30F) *
                            (0.97F + requested.body.head_scale * 0.06F);
    face.head_depth_scale = 0.88F + requested.face.head_depth * 0.26F;
    const float head_width_fit = std::clamp((face.head_width_scale - 0.86F) / 0.30F,
                                            0.0F, 1.0F);
    face.jaw_width_scale = std::clamp(
        (0.74 + requested.face.jaw_width * 0.55) *
            (0.92 + static_cast<double>(head_width_fit) * 0.16), 0.70, 1.34);
    const float jaw_width_fit = std::clamp((face.jaw_width_scale - 0.70F) / 0.64F,
                                           0.0F, 1.0F);
    face.eye_spacing_ratio = (0.0164F + requested.face.eye_spacing * 0.0086F) *
                             (0.91F + head_width_fit * 0.18F);
    face.eye_spacing = body.height * face.eye_spacing_ratio;
    face.eye_width_scale = 0.78F + requested.face.eye_width * 0.46F;
    face.eye_height_scale = 0.73F + requested.face.eye_height * 0.55F;
    face.eye_size_scale = 0.76F +
        ((requested.face.eye_width + requested.face.eye_height) * 0.5F) * 0.50F;
    face.eye_depth = -0.0025F + requested.face.eye_depth * 0.0055F;
    face.eye_tilt = -0.12F + requested.face.eye_tilt * 0.24F;
    face.eye_radius = relativeToHeight(
        std::max(0.0083F * face.eye_width_scale * 1.12F,
                 0.0034F * face.eye_height_scale * 1.65F) * 0.82F,
        body.height);
    face.eye_y_ratio = 0.935F + requested.face.eye_vertical * 0.010F;
    face.eye_y = body.height * face.eye_y_ratio;
    face.brow_y_ratio = face.eye_y_ratio + 0.0034F * face.eye_height_scale + 0.008F +
                        (-0.0005F + requested.face.brow_height * 0.002F);
    face.brow_y = body.height * face.brow_y_ratio;
    face.brow_thickness = 0.00055F + requested.face.brow_thickness * 0.00073F;
    face.brow_tilt = -0.15F + requested.face.brow_tilt * 0.31F;
    face.brow_spacing = -0.003F + requested.face.brow_spacing * 0.0065F;
    face.nose_y = body.height *
                  (0.904F - (requested.face.nose_length - 0.5F) * 0.006F);
    face.nose_projection_scale = 0.76F + requested.face.nose_projection * 0.53F;
    face.nose_bridge_scale = 0.70F + requested.face.nose_bridge * 0.60F;
    face.nose_tip_width_scale = 0.72F + requested.face.nose_tip_width * 0.59F;
    face.nostril_width_scale = 0.76F + requested.face.nostril_width * 0.52F;
    face.nose_width_scale = (0.72F + requested.face.nose_width * 0.58F) *
                            (0.91F + head_width_fit * 0.18F);
    face.nose_length_scale = 0.80F + requested.face.nose_length * 0.42F;
    face.nose_tip_rotation = -0.14F + requested.face.nose_tip_rotation * 0.30F;
    face.nose_length = relativeToHeight(
        0.031F * (0.80F + requested.face.nose_length * 0.42F) *
            face.nose_projection_scale, body.height);
    face.nose_width = relativeToHeight(
        0.012F * (0.72F + requested.face.nose_width * 0.58F) *
            face.nose_tip_width_scale, body.height);
    face.mouth_y_ratio = 0.899F + requested.face.mouth_height * 0.005F +
                         (-0.005F + requested.face.chin_height * 0.011F) * 0.30F;
    face.mouth_y = body.height * face.mouth_y_ratio;
    face.mouth_width_ratio = (0.017F + requested.face.mouth_width * 0.013F) *
                             (0.92F + jaw_width_fit * 0.16F);
    face.mouth_width = relativeToHeight(face.mouth_width_ratio, body.height);
    face.jaw_width = relativeToHeight(
        0.058F + requested.face.jaw_width * 0.030F, body.height);
    face.head_width = relativeToHeight(0.095F * face.head_width_scale, body.height);
    face.head_depth = relativeToHeight(0.088F * face.head_depth_scale, body.height);
    face.head_length_scale = 0.92F + requested.face.head_length * 0.18F;
    face.forehead_width_scale = 0.84F + requested.face.forehead_width * 0.32F;
    face.forehead_slope = -0.006F + requested.face.forehead_slope * 0.013F;
    face.temple_width_scale = 0.86F + requested.face.temple_width * 0.29F;
    face.brow_ridge = -0.002F + requested.face.brow_ridge * 0.0065F;
    face.jaw_length_scale = 0.89F + requested.face.jaw_length * 0.23F;
    face.jaw_angle = 0.80F + requested.face.jaw_angle * 0.38F;
    face.chin_width_scale = std::clamp(
        (0.68 + requested.face.chin_width * 0.66) *
            (0.91 + static_cast<double>(jaw_width_fit) * 0.18), 0.64, 1.39);
    face.chin_height = -0.005F + requested.face.chin_height * 0.011F;
    face.chin_projection = -0.006F + requested.face.chin_projection * 0.015F;
    face.cheekbone_scale = 0.82F + requested.face.cheekbone_width * 0.38F;
    face.cheekbone_y = -0.006F + requested.face.cheekbone_height * 0.012F;
    face.cheek_fullness = -0.004F + requested.face.cheek_fullness * 0.010F;
    face.midface_projection = -0.004F + requested.face.midface_projection * 0.0105F;
    face.eye_roundness = 0.72F + requested.face.eye_roundness * 0.56F;
    face.hair_density = 0.76F + requested.face.hair_density * 0.42F;
    face.hair_style = static_cast<std::uint8_t>(std::min(6.0, requested.face.hair_style * 7.0));
    face.hair_thickness = 0.0025F + requested.face.hair_thickness * 0.0045F;
    face.hair_volume = 0.003F + requested.face.hair_volume * 0.013F;
    face.hair_brightness = 0.82F + requested.face.hair_brightness * 0.36F;
    face.temple_recession = requested.face.temple_recession * 0.012F;
    face.widow_peak = requested.face.widow_peak * 0.009F;
    face.hair_color = {static_cast<float>(0.07 + requested.face.hair_color * 0.60),
                       static_cast<float>(0.05 + requested.face.hair_color * 0.50),
                       static_cast<float>(0.03 + requested.face.hair_color * 0.36), 1.0F};
    const auto hair_base = paletteHex(requested.face.hair_color,
        std::array<std::uint32_t, 9U>{0x11100FU, 0x1A1512U, 0x2C1C14U, 0x452A1AU,
                                      0x684125U, 0x8E663BU, 0xB58F58U, 0xD1B578U,
                                      0x8B4829U});
    face.hair_color_hex = shadeHex(hair_base,
                                   0.76F + requested.face.hair_brightness * 0.40F);
    face.eye_color_hex = paletteHex(requested.face.eye_color,
        std::array<std::uint32_t, 8U>{0x2D2018U, 0x4B3522U, 0x6A4C2FU, 0x6C6341U,
                                      0x596755U, 0x687477U, 0x71879BU, 0x526A82U});
    face.blink_interval = 2.6F + requested.face.blink_rate * 3.8F;
    face.blink_duration = 0.105F + requested.face.blink_speed * 0.08F;
    face.gaze_restlessness = 0.65F + requested.face.gaze_restlessness * 0.90F;
    face.hairline_ratio = 0.943F + requested.face.hairline * 0.018F;
    face.hairline_y = body.height * face.hairline_ratio;
    face.upper_lip = 0.0012F + requested.face.upper_lip * 0.0019F;
    face.lower_lip = 0.0013F + requested.face.lower_lip * 0.0021F;
    face.ear_scale = 0.78F + requested.face.ear_size * 0.46F;
    face.ear_angle = -0.18F + requested.face.ear_angle * 0.42F;
    face.eye_asymmetry = (requested.face.eye_height_asymmetry - 0.5F) * 0.0036F;
    face.brow_asymmetry = (requested.face.brow_height_asymmetry - 0.5F) * 0.0040F;
    face.mouth_asymmetry = (requested.face.mouth_corner_asymmetry - 0.5F) * 0.0032F;
    face.ear_asymmetry = (requested.face.ear_asymmetry - 0.5F) * 0.0038F;
    face.neutral_eye_open = 0.67F + requested.face.resting_eye_openness * 0.26F;
    face.neutral_brow = -0.09F + requested.face.resting_brow * 0.18F;
    face.neutral_mouth = -0.12F + requested.face.resting_mouth * 0.22F;
    face.expression_scale = 0.84F + requested.face.expression_scale * 0.32F;
    face.eye_expression_scale = 0.84F + requested.face.eye_expression * 0.34F;
    face.mouth_expression_scale = 0.82F + requested.face.mouth_expression * 0.38F;
    face.brow_expression_scale = 0.84F + requested.face.brow_expression * 0.34F;

    auto& reference_face = face.reference;
    reference_face.head_width_scale =
        mix_reference(.86, 1.16, requested.face.head_width) *
        mix_reference(.97, 1.03, requested.body.head_scale);
    const double reference_head_width_fit = std::clamp(
        (reference_face.head_width_scale - .86) / .30, 0.0, 1.0);
    reference_face.head_depth_scale = mix_reference(.88, 1.14, requested.face.head_depth);
    reference_face.head_length_scale = mix_reference(.92, 1.10, requested.face.head_length);
    reference_face.forehead_width_scale =
        mix_reference(.84, 1.16, requested.face.forehead_width);
    reference_face.forehead_slope = mix_reference(-.006, .007, requested.face.forehead_slope);
    reference_face.temple_width_scale = mix_reference(.86, 1.15, requested.face.temple_width);
    reference_face.brow_ridge = mix_reference(-.002, .0045, requested.face.brow_ridge);
    reference_face.jaw_width_scale = std::clamp(
        mix_reference(.74, 1.29, requested.face.jaw_width) *
            mix_reference(.92, 1.08, reference_head_width_fit),
        .70, 1.34);
    const double reference_jaw_width_fit = std::clamp(
        (reference_face.jaw_width_scale - .70) / .64, 0.0, 1.0);
    reference_face.jaw_length_scale = mix_reference(.89, 1.12, requested.face.jaw_length);
    reference_face.jaw_angle = mix_reference(.80, 1.18, requested.face.jaw_angle);
    reference_face.chin_width_scale = std::clamp(
        mix_reference(.68, 1.34, requested.face.chin_width) *
            mix_reference(.91, 1.09, reference_jaw_width_fit),
        .64, 1.39);
    reference_face.chin_height = mix_reference(-.005, .006, requested.face.chin_height);
    reference_face.chin_projection =
        mix_reference(-.006, .009, requested.face.chin_projection);
    reference_face.cheekbone_scale =
        mix_reference(.82, 1.20, requested.face.cheekbone_width);
    reference_face.cheekbone_y =
        mix_reference(-.006, .006, requested.face.cheekbone_height);
    reference_face.cheek_fullness =
        mix_reference(-.004, .006, requested.face.cheek_fullness);
    reference_face.midface_projection =
        mix_reference(-.004, .0065, requested.face.midface_projection);
    reference_face.eye_spacing =
        mix_reference(.0164, .0250, requested.face.eye_spacing) *
        mix_reference(.91, 1.09, reference_head_width_fit);
    reference_face.eye_width_scale = mix_reference(.78, 1.24, requested.face.eye_width);
    reference_face.eye_height_scale = mix_reference(.73, 1.28, requested.face.eye_height);
    reference_face.eye_roundness = mix_reference(.72, 1.28, requested.face.eye_roundness);
    reference_face.eye_depth = mix_reference(-.0025, .0030, requested.face.eye_depth);
    reference_face.eye_tilt = mix_reference(-.12, .12, requested.face.eye_tilt);
    reference_face.eye_y = mix_reference(.935, .945, requested.face.eye_vertical);
    reference_face.brow_y = reference_face.eye_y + .0034 * reference_face.eye_height_scale +
                            .008 + mix_reference(-.0005, .0015, requested.face.brow_height);
    reference_face.brow_thickness =
        mix_reference(.00055, .00128, requested.face.brow_thickness);
    reference_face.brow_tilt = mix_reference(-.15, .16, requested.face.brow_tilt);
    reference_face.brow_spacing = mix_reference(-.003, .0035, requested.face.brow_spacing);
    reference_face.nose_width_scale =
        mix_reference(.72, 1.30, requested.face.nose_width) *
        mix_reference(.91, 1.09, reference_head_width_fit);
    reference_face.nose_length_scale = mix_reference(.80, 1.22, requested.face.nose_length);
    reference_face.nose_projection_scale =
        mix_reference(.76, 1.29, requested.face.nose_projection);
    reference_face.nose_bridge_scale = mix_reference(.70, 1.30, requested.face.nose_bridge);
    reference_face.nose_tip_width_scale =
        mix_reference(.72, 1.31, requested.face.nose_tip_width);
    reference_face.nose_tip_rotation =
        mix_reference(-.14, .16, requested.face.nose_tip_rotation);
    reference_face.nostril_width_scale =
        mix_reference(.76, 1.28, requested.face.nostril_width);
    reference_face.mouth_width =
        mix_reference(.017, .030, requested.face.mouth_width) *
        mix_reference(.92, 1.08, reference_jaw_width_fit);
    reference_face.upper_lip = mix_reference(.0012, .0031, requested.face.upper_lip);
    reference_face.lower_lip = mix_reference(.0013, .0034, requested.face.lower_lip);
    reference_face.mouth_y =
        mix_reference(.899, .904, requested.face.mouth_height) +
        reference_face.chin_height * .30;
    reference_face.ear_scale = mix_reference(.78, 1.24, requested.face.ear_size);
    reference_face.ear_angle = mix_reference(-.18, .24, requested.face.ear_angle);
    reference_face.hair_density = mix_reference(.76, 1.18, requested.face.hair_density);
    reference_face.hair_thickness =
        mix_reference(.0025, .0070, requested.face.hair_thickness);
    reference_face.hair_volume = mix_reference(.003, .016, requested.face.hair_volume);
    reference_face.hairline = mix_reference(.943, .961, requested.face.hairline);
    reference_face.temple_recession = mix_reference(0.0, .012, requested.face.temple_recession);
    reference_face.widow_peak = mix_reference(0.0, .009, requested.face.widow_peak);
    reference_face.neutral_mouth = mix_reference(-.12, .10, requested.face.resting_mouth);
    reference_face.eye_asymmetry = (requested.face.eye_height_asymmetry - .5) * .0036;
    reference_face.brow_asymmetry = (requested.face.brow_height_asymmetry - .5) * .0040;
    reference_face.mouth_asymmetry = (requested.face.mouth_corner_asymmetry - .5) * .0032;
    reference_face.ear_asymmetry = (requested.face.ear_asymmetry - .5) * .0038;
    reference_face.eye_color_hex = face.eye_color_hex;
    reference_face.hair_color_hex = face.hair_color_hex;
    reference_face.hair_style = face.hair_style;
    face.hairline_y = std::max(
        face.hairline_y, face.brow_y + body.height * 0.006F);
    const float maximum_eye_spacing = face.jaw_width * 0.42F;
    if (face.eye_spacing > maximum_eye_spacing) {
        const float requested_eye_spacing = face.eye_spacing;
        face.eye_spacing = maximum_eye_spacing;
        artifact.diagnostics.face_spacing_adjusted = true;
        artifact.diagnostics.adjustments.push_back(
            {AnatomyParameter::EyeSpacing, requested_eye_spacing, face.eye_spacing,
             AnatomyAdjustmentReason::ConstrainedToFaceWidth});
    }
    if (!(face.brow_y > face.eye_y && face.eye_y > face.nose_y && face.nose_y > face.mouth_y)) {
        const float requested_mouth_y = face.mouth_y;
        face.eye_y = face.brow_y - 0.04F;
        face.nose_y = face.eye_y - 0.07F;
        face.mouth_y = face.nose_y - 0.075F;
        artifact.diagnostics.landmark_order_adjusted = true;
        artifact.diagnostics.adjustments.push_back(
            {AnatomyParameter::LandmarkOrder, requested_mouth_y, face.mouth_y,
             AnatomyAdjustmentReason::RestoredLandmarkOrdering});
    }
    artifact.body = body;
    artifact.face = face;
    return artifact.valid()
               ? foundation::Result<PhenotypeArtifact, foundation::Error>::success(artifact)
               : foundation::Result<PhenotypeArtifact, foundation::Error>::failure(
                     {foundation::ErrorCode::InvalidState,
                      !body.valid() ? "infantry body phenotype constraints are infeasible" :
                      !face.valid() ? "infantry face phenotype constraints are infeasible" :
                                      "infantry phenotype constraints are infeasible"});
}

} // namespace genomes::infantry
