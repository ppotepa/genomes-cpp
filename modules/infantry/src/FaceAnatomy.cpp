#include <genomes/infantry/FaceAnatomy.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace genomes::infantry {

namespace {
[[nodiscard]] HeadCrossSection interpolate(const HeadCrossSection& lower,
                                           const HeadCrossSection& upper,
                                           float y) noexcept {
    const float t = std::clamp((y - lower.y) / std::max(1.0e-6F, upper.y - lower.y),
                               0.0F, 1.0F);
    const float smooth = t * t * (3.0F - 2.0F * t);
    return {y, lower.half_width + (upper.half_width - lower.half_width) * smooth,
            lower.half_depth + (upper.half_depth - lower.half_depth) * smooth,
            lower.center_z + (upper.center_z - lower.center_z) * smooth};
}
} // namespace

foundation::Result<ResolvedAnatomy, foundation::Error>
FaceAnatomyEvaluator::resolve(const PhenotypeArtifact& phenotype) {
    if (!phenotype.valid()) {
        return foundation::Result<ResolvedAnatomy, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid phenotype for anatomy resolution"});
    }
    const BodyPhenotype& body = phenotype.body;
    const FacePhenotype& face = phenotype.face;
    ResolvedAnatomy result{};
    result.height = body.height;
    const auto add_torso = [&result](float y, float width, float depth, float front) {
        if (result.torso_sections.empty() ||
            y > result.torso_sections.back().y + 1.0e-5F) {
            result.torso_sections.push_back({y, width, depth, front});
        }
    };
    add_torso(0.04F, body.hip_width * 0.30F, body.chest_depth * 0.36F, 0.0F);
    add_torso(body.pelvis.y - body.height * 0.04F, body.hip_width * 0.50F,
              body.chest_depth * 0.46F, 0.0F);
    add_torso(body.pelvis.y, body.hip_width * 0.50F, body.chest_depth * 0.50F, 0.0F);
    add_torso(body.chest.y - body.height * 0.07F, body.waist_width * 0.50F,
              body.chest_depth * 0.48F, 0.0F);
    add_torso(body.chest.y, body.shoulder_width * 0.50F, body.chest_depth * 0.50F, 0.0F);
    add_torso(body.head.y - body.height * 0.06F, body.shoulder_width * 0.38F,
              body.chest_depth * 0.42F, 0.0F);
    add_torso(body.head.y, body.shoulder_width * 0.28F, body.chest_depth * 0.32F, 0.0F);

    const float lower_head_y = face.mouth_y -
                               0.09F * face.jaw_length_scale * face.head_length_scale;
    const auto add_head = [&result](float y, float width, float depth, float center_z) {
        if (result.head_sections.empty() ||
            y > result.head_sections.back().y + 1.0e-5F) {
            result.head_sections.push_back({y, width, depth, center_z});
        }
    };
    const auto at = [&face](float y) {
        const FaceSection section = face.section(y);
        return HeadCrossSection{y, section.radius_x, section.radius_z, section.center_z};
    };
    const std::array<float, 7U> semantic_levels{
        lower_head_y, face.mouth_y, face.nose_y, face.eye_y,
        face.brow_y, face.hairline_y,
        face.hairline_y + std::max(0.012F, face.hair_volume * 4.0F)};
    // Keep the semantic landmarks while sampling each slab at two interior
    // points.  The denser profile mirrors the reference's stable 19-level
    // anatomy description and prevents long, visibly linear forehead/jaw
    // spans from becoming separate primitive-looking volumes.
    for (std::size_t index = 0U; index + 1U < semantic_levels.size(); ++index) {
        const float lower = semantic_levels[index];
        const float upper = semantic_levels[index + 1U];
        for (const float t : {0.0F, 1.0F / 3.0F, 2.0F / 3.0F}) {
            const HeadCrossSection section = at(lower + (upper - lower) * t);
            add_head(section.y, section.half_width, section.half_depth, section.center_z);
        }
    }
    const HeadCrossSection top = at(semantic_levels.back());
    add_head(top.y, top.half_width, top.half_depth, top.center_z);
    if (result.head_sections.size() < 2U) {
        return foundation::Result<ResolvedAnatomy, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "head anatomy has insufficient profile levels"});
    }

    const float eye_front = face.frontZ(face.eye_y);
    const float nose_front = face.frontZ(face.nose_y) + face.nose_length;
    const float mouth_front = face.frontZ(face.mouth_y);
    const float ear_x = face.section(face.eye_y).radius_x * 0.96F * face.ear_scale;
    result.face.left_eye = {-face.eye_spacing,
                            face.eye_y - face.eye_asymmetry * 0.5F,
                            eye_front + face.eye_depth};
    result.face.right_eye = {face.eye_spacing,
                             face.eye_y + face.eye_asymmetry * 0.5F,
                             eye_front + face.eye_depth};
    result.face.nose_bridge = {0.0F,
                               face.eye_y - face.eye_radius * 0.70F,
                               eye_front + face.nose_bridge_scale * 0.001F};
    result.face.nose_tip = {0.0F, face.nose_y, nose_front};
    result.face.left_mouth_corner = {-face.mouth_width * 0.5F,
                                     face.mouth_y - face.mouth_asymmetry * 0.5F,
                                     mouth_front};
    result.face.right_mouth_corner = {face.mouth_width * 0.5F,
                                      face.mouth_y + face.mouth_asymmetry * 0.5F,
                                      mouth_front};
    result.face.chin = {0.0F, lower_head_y, face.section(lower_head_y).center_z +
                                       face.section(lower_head_y).radius_z * 0.82F};
    result.face.left_ear = {-ear_x,
                            face.eye_y - face.eye_radius * 0.25F - face.ear_asymmetry * 0.5F,
                            face.section(face.eye_y).center_z};
    result.face.right_ear = {ear_x,
                             face.eye_y - face.eye_radius * 0.25F + face.ear_asymmetry * 0.5F,
                             face.section(face.eye_y).center_z};
    result.scalp = {lower_head_y, face.hairline_y,
                    face.hairline_y + std::max(0.012F, face.hair_volume * 4.0F),
                    face.temple_recession, face.widow_peak};
    return foundation::Result<ResolvedAnatomy, foundation::Error>::success(std::move(result));
}

HeadCrossSection FaceAnatomyEvaluator::sectionAt(const ResolvedAnatomy& anatomy,
                                                 float y) noexcept {
    if (anatomy.head_sections.empty()) {
        return {};
    }
    if (y <= anatomy.head_sections.front().y) {
        return anatomy.head_sections.front();
    }
    for (std::size_t index = 1U; index < anatomy.head_sections.size(); ++index) {
        if (y <= anatomy.head_sections[index].y) {
            return interpolate(anatomy.head_sections[index - 1U],
                               anatomy.head_sections[index], y);
        }
    }
    return anatomy.head_sections.back();
}

foundation::Vec3 FaceAnatomyEvaluator::scalpPoint(const ResolvedAnatomy& anatomy,
                                                  float normalized_height,
                                                  float azimuth) noexcept {
    const float t = std::clamp(normalized_height, 0.0F, 1.0F);
    const float y = anatomy.scalp.hairline_y +
                    (anatomy.scalp.crown_y - anatomy.scalp.hairline_y) * t;
    const HeadCrossSection section = sectionAt(anatomy, y);
    const float c = std::cos(azimuth);
    const float s = std::sin(azimuth);
    return {section.half_width * c, y,
            section.center_z + section.half_depth * s};
}

} // namespace genomes::infantry
