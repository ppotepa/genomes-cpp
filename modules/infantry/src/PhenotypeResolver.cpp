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

} // namespace

bool BodyPhenotype::valid() const noexcept {
    return finite(height) && height >= 1.60F && height <= 1.95F && finite(shoulder_width) &&
           shoulder_width > 0.20F && shoulder_width < 0.90F && finite(chest_depth) &&
           chest_depth > 0.10F && chest_depth < 0.60F && finite(hip_width) && hip_width > 0.15F &&
           hip_width < 0.70F && finite(arm_length) && arm_length > 0.30F && arm_length < 1.20F &&
           finite(leg_length) && leg_length > 0.50F && leg_length < 1.40F && finite(pelvis) &&
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
    return {face_section.radius_x * std::cos(theta),
            y,
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
    if (artifact.diagnostics.height_clamped) {
        artifact.diagnostics.adjustments.push_back(
            {AnatomyParameter::Height, requested.height, body.height,
             AnatomyAdjustmentReason::ClampedToDomain});
    }
    const float shoulder_scale = std::clamp(
        (0.82F + requested.body.shoulder_width * 0.40F) *
            (0.96F + requested.body.frame * 0.10F) *
            (0.96F + requested.body.musculature * 0.11F),
        0.78F, 1.28F);
    const float hip_scale = std::clamp(
        (0.86F + requested.body.hip_width * 0.31F) *
            (0.97F + requested.body.frame * 0.08F) *
            (0.97F + requested.body.adiposity * 0.08F),
        0.82F, 1.23F);
    body.shoulder_width = relativeToHeight(0.256F * shoulder_scale, body.height);
    // These are final metre quantities. The reference stores these body
    // proportions in height-relative space; keep the conversion visible at
    // the resolver boundary instead of leaking normalized values into rig or
    // surface generation.
    body.chest_depth = relativeToHeight(
        0.135F * (0.82F + requested.body.chest_depth * 0.40F) *
            (0.96F + requested.body.mass * 0.08F), body.height);
    body.hip_width = relativeToHeight(0.104F * hip_scale, body.height);
    body.arm_length = relativeToHeight(
        0.335F * (0.93F + requested.body.arm_length * 0.16F), body.height);
    body.leg_length = relativeToHeight(
        0.495F * (0.94F + requested.body.leg_length * 0.13F), body.height);
    body.waist_width = relativeToHeight(
        0.20F * (0.78F + requested.body.waist_width * 0.44F) *
            (0.93F + requested.body.adiposity * 0.19F), body.height);
    body.limb_thickness = relativeToHeight(
        0.075F * (0.76F + requested.body.limb_thickness * 0.49F), body.height);
    body.neck_scale = 0.86F + requested.body.neck_thickness * 0.28F;
    body.head_scale = 0.90F + requested.body.head_scale * 0.20F;
    body.hand_scale = 0.88F + requested.body.hand_scale * 0.24F;
    body.foot_scale = 0.90F + requested.body.foot_scale * 0.20F;
    body.skin_color = {0.42F + requested.body.skin_tone * 0.38F,
                       0.24F + requested.body.skin_tone * 0.34F,
                       0.16F + requested.body.skin_tone * 0.28F, 1.0F};
    const float requested_shoulder_width = body.shoulder_width;
    body.shoulder_width = clampMetric(body.shoulder_width, 0.25F, 0.78F,
                                      artifact.diagnostics.shoulder_clamped);
    if (artifact.diagnostics.shoulder_clamped) {
        artifact.diagnostics.adjustments.push_back(
            {AnatomyParameter::ShoulderWidth, requested_shoulder_width, body.shoulder_width,
             AnatomyAdjustmentReason::ClampedToDomain});
    }
    const float hip_y = std::clamp(0.54F + (0.5F - requested.body.torso_leg_ratio) * 0.04F,
                                   0.502F, 0.579F);
    const float neck_y = 0.838F + (body.neck_scale - 1.0F) * 0.010F;
    const float chest_y = hip_y + (neck_y - hip_y) * 0.76F;
    const float shoulder_y = chest_y + (neck_y - chest_y) * 0.34F;
    const float arm_angle = 22.0F * 3.14159265358979323846F / 180.0F;
    const float upper_arm_length = body.arm_length * (0.18F / 0.335F);
    const float forearm_length = body.arm_length * (0.155F / 0.335F);
    const float hand_x = body.shoulder_width * 0.5F +
                         std::sin(arm_angle) * (upper_arm_length + forearm_length);
    const float hand_y = shoulder_y -
                         std::cos(arm_angle) * (upper_arm_length + forearm_length);
    body.pelvis = {0.0F, body.height * hip_y, 0.0F};
    body.chest = {0.0F, body.height * chest_y, 0.0F};
    body.head = {0.0F, body.height * (0.90F + (body.head_scale - 1.0F) * 0.008F), 0.015F};
    body.left_hand = {-hand_x, body.height * hand_y, 0.0F};
    body.right_hand = {-body.left_hand.x, body.left_hand.y, body.left_hand.z};
    const float foot_y = std::max(0.025F, body.pelvis.y - body.leg_length);
    body.left_foot = {-body.hip_width * 0.30F, foot_y, 0.02F};
    body.right_foot = {-body.left_foot.x, body.left_foot.y, body.left_foot.z};

    FacePhenotype face{};
    face.eye_spacing = body.height * (0.0164F + requested.face.eye_spacing * 0.0086F);
    face.eye_width_scale = 0.78F + requested.face.eye_width * 0.46F;
    face.eye_height_scale = 0.73F + requested.face.eye_height * 0.55F;
    face.eye_depth = -0.0025F + requested.face.eye_depth * 0.0055F;
    face.eye_tilt = -0.12F + requested.face.eye_tilt * 0.24F;
    face.eye_radius = relativeToHeight(
        std::max(0.0083F * face.eye_width_scale * 1.12F,
                 0.0034F * face.eye_height_scale * 1.65F) * 0.82F,
        body.height);
    face.eye_y = body.height * (0.935F + requested.face.eye_vertical * 0.010F);
    face.brow_y = face.eye_y + body.height *
                 (0.008F + 0.0034F * (0.73F + requested.face.eye_height * 0.55F)) +
                 (-0.0005F + requested.face.brow_height * 0.002F);
    face.nose_y = body.height *
                  (0.904F - (requested.face.nose_length - 0.5F) * 0.006F);
    face.nose_projection_scale = 0.76F + requested.face.nose_projection * 0.53F;
    face.nose_bridge_scale = 0.70F + requested.face.nose_bridge * 0.60F;
    face.nose_tip_width_scale = 0.72F + requested.face.nose_tip_width * 0.59F;
    face.nostril_width_scale = 0.76F + requested.face.nostril_width * 0.52F;
    face.nose_length = relativeToHeight(
        0.031F * (0.80F + requested.face.nose_length * 0.42F) *
            face.nose_projection_scale, body.height);
    face.nose_width = relativeToHeight(
        0.012F * (0.72F + requested.face.nose_width * 0.58F) *
            face.nose_tip_width_scale, body.height);
    face.mouth_y = body.height * (0.899F + requested.face.mouth_height * 0.005F);
    face.mouth_width = relativeToHeight(
        0.034F + requested.face.mouth_width * 0.026F, body.height);
    face.jaw_width = relativeToHeight(
        0.058F + requested.face.jaw_width * 0.030F, body.height);
    face.head_width = relativeToHeight(
        0.095F * (0.86F + requested.face.head_width * 0.30F), body.height);
    face.head_depth = relativeToHeight(
        0.088F * (0.88F + requested.face.head_depth * 0.26F), body.height);
    face.head_length_scale = 0.92F + requested.face.head_length * 0.18F;
    face.forehead_width_scale = 0.84F + requested.face.forehead_width * 0.32F;
    face.forehead_slope = -0.006F + requested.face.forehead_slope * 0.013F;
    face.temple_width_scale = 0.86F + requested.face.temple_width * 0.29F;
    face.brow_ridge = -0.002F + requested.face.brow_ridge * 0.0065F;
    face.jaw_length_scale = 0.89F + requested.face.jaw_length * 0.23F;
    face.jaw_angle = 0.80F + requested.face.jaw_angle * 0.38F;
    face.chin_width_scale = 0.68F + requested.face.chin_width * 0.66F;
    face.chin_height = -0.005F + requested.face.chin_height * 0.011F;
    face.chin_projection = -0.006F + requested.face.chin_projection * 0.015F;
    face.cheekbone_scale = 0.82F + requested.face.cheekbone_width * 0.38F;
    face.cheekbone_y = -0.006F + requested.face.cheekbone_height * 0.012F;
    face.cheek_fullness = -0.004F + requested.face.cheek_fullness * 0.010F;
    face.midface_projection = -0.004F + requested.face.midface_projection * 0.0105F;
    face.eye_roundness = 0.72F + requested.face.eye_roundness * 0.56F;
    face.hair_density = 0.76F + requested.face.hair_density * 0.42F;
    face.hair_style = static_cast<std::uint8_t>(std::min(6.0F, requested.face.hair_style * 7.0F));
    face.hair_thickness = 0.0025F + requested.face.hair_thickness * 0.0045F;
    face.hair_volume = 0.003F + requested.face.hair_volume * 0.013F;
    face.hair_brightness = 0.82F + requested.face.hair_brightness * 0.36F;
    face.temple_recession = requested.face.temple_recession;
    face.widow_peak = requested.face.widow_peak;
    face.hair_color = {0.07F + requested.face.hair_color * 0.60F,
                       0.05F + requested.face.hair_color * 0.50F,
                       0.03F + requested.face.hair_color * 0.36F, 1.0F};
    face.blink_interval = 2.6F + requested.face.blink_rate * 3.8F;
    face.blink_duration = 0.105F + requested.face.blink_speed * 0.08F;
    face.gaze_restlessness = 0.65F + requested.face.gaze_restlessness * 0.90F;
    face.hairline_y = body.height * (0.943F + requested.face.hairline * 0.018F);
    face.upper_lip = 0.0012F + requested.face.upper_lip * 0.0019F;
    face.lower_lip = 0.0013F + requested.face.lower_lip * 0.0021F;
    face.ear_scale = 0.78F + requested.face.ear_size * 0.46F;
    face.ear_angle = -0.18F + requested.face.ear_angle * 0.42F;
    face.eye_asymmetry = (requested.face.eye_height_asymmetry - 0.5F) * 0.0036F;
    face.brow_asymmetry = (requested.face.brow_height_asymmetry - 0.5F) * 0.0040F;
    face.mouth_asymmetry = (requested.face.mouth_corner_asymmetry - 0.5F) * 0.0032F;
    face.ear_asymmetry = (requested.face.ear_asymmetry - 0.5F) * 0.0038F;
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
                      "infantry phenotype constraints are infeasible"});
}

} // namespace genomes::infantry
