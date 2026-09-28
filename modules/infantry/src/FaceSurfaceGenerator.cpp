#include <genomes/infantry/FaceSurfaceGenerator.hpp>

#include <genomes/infantry/BodySurfaceGenerator.hpp>
#include <genomes/infantry/InfantryMaterials.hpp>

#include <genomes/geometry/GeometryConstants.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace genomes::infantry {

namespace {

using foundation::Vec2;
using foundation::Vec3;

[[nodiscard]] Vec3 subtract(Vec3 left, Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] float dot(Vec3 left, Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] Vec3 cross(Vec3 left, Vec3 right) noexcept {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

[[nodiscard]] foundation::Color shade(foundation::Color color, float factor) noexcept {
    return {std::clamp(color.r * factor, 0.0F, 1.0F),
            std::clamp(color.g * factor, 0.0F, 1.0F),
            std::clamp(color.b * factor, 0.0F, 1.0F), color.a};
}

[[nodiscard]] std::array<SkinInfluence, 4U> oneBone(const SkeletonData& skeleton, BoneId id,
                                                     std::uint8_t& count) {
    const auto* bone = skeleton.find(id);
    if (bone == nullptr) {
        count = 0U;
        return {};
    }
    const SkinInfluence influence{static_cast<std::uint16_t>(boneIndex(id)), 1.0F};
    count = 1U;
    return {influence, {}, {}, {}};
}

void appendOriented(AppearanceMeshBuilder& builder, std::uint32_t a, std::uint32_t b,
                    std::uint32_t c) {
    const auto& vertices = builder.mesh().vertices;
    const Vec3 ab = subtract(vertices[b].position, vertices[a].position);
    const Vec3 ac = subtract(vertices[c].position, vertices[a].position);
    const Vec3 average{
        (vertices[a].normal.x + vertices[b].normal.x + vertices[c].normal.x) / 3.0F,
        (vertices[a].normal.y + vertices[b].normal.y + vertices[c].normal.y) / 3.0F,
        (vertices[a].normal.z + vertices[b].normal.z + vertices[c].normal.z) / 3.0F};
    if (dot(cross(ab, ac), average) < 0.0F) {
        std::swap(b, c);
    }
    builder.appendTriangle(a, b, c);
}

void appendOpening(AppearanceMeshBuilder& builder, float center_x, float center_y,
                   float radius_x, float radius_y, float depth, std::size_t segments,
                   foundation::Color color, std::uint16_t region,
                   const std::array<SkinInfluence, 4U>& influences, std::uint8_t count) {
    std::vector<std::uint32_t> outer;
    std::vector<std::uint32_t> inner;
    outer.reserve(segments);
    inner.reserve(segments);
    const std::span<const SkinInfluence> weights(influences.data(), count);
    for (std::size_t index = 0U; index < segments; ++index) {
        const float angle = 6.28318530718F * static_cast<float>(index) /
                            static_cast<float>(segments);
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        const Vec2 uv{0.5F + c * 0.5F, 0.5F + s * 0.5F};
        outer.push_back(builder.appendVertex({{center_x + radius_x * c, center_y + radius_y * s,
                                                depth}, {0.0F, 0.0F, 1.0F}, uv, color,
                                               region, weights}));
        inner.push_back(builder.appendVertex({{center_x + radius_x * 0.68F * c,
                                                center_y + radius_y * 0.68F * s,
                                                depth + 0.0005F}, {0.0F, 0.0F, 1.0F}, uv,
                                               shade(color, 0.70F), region, weights}));
    }
    for (std::size_t index = 0U; index < segments; ++index) {
        const std::size_t next = (index + 1U) % segments;
        appendOriented(builder, outer[index], outer[next], inner[index]);
        appendOriented(builder, outer[next], inner[next], inner[index]);
    }
}

void appendEllipsoid(AppearanceMeshBuilder& builder, Vec3 center, Vec3 radii,
                     std::size_t segments, std::size_t rows, foundation::Color color,
                     std::uint16_t region, const std::array<SkinInfluence, 4U>& influences,
                     std::uint8_t count, float rotation_z = 0.0F) {
    const std::span<const SkinInfluence> weights(influences.data(), count);
    const float rotation_cos = std::cos(rotation_z);
    const float rotation_sin = std::sin(rotation_z);
    const auto orient = [center, rotation_cos, rotation_sin](Vec3 local) {
        return Vec3{center.x + rotation_cos * local.x - rotation_sin * local.y,
                    center.y + rotation_sin * local.x + rotation_cos * local.y,
                    center.z + local.z};
    };
    const auto orientNormal = [rotation_cos, rotation_sin](Vec3 local) {
        return Vec3{rotation_cos * local.x - rotation_sin * local.y,
                    rotation_sin * local.x + rotation_cos * local.y, local.z};
    };
    const auto vertex = [&builder, color, region, weights, &orient, &orientNormal](
                            Vec3 position, Vec3 normal, Vec2 uv) {
        return builder.appendVertex({orient(position), orientNormal(normal), uv, color, region,
                                     weights});
    };
    const std::uint32_t south = vertex({0.0F, -radii.y, 0.0F},
                                       {0.0F, -1.0F, 0.0F}, {0.5F, 0.0F});
    std::vector<std::uint32_t> previous;
    for (std::size_t row = 1U; row < rows; ++row) {
        const float phi = -1.57079632679F + 3.14159265359F *
            static_cast<float>(row) / static_cast<float>(rows);
        const float cp = std::cos(phi);
        const float sp = std::sin(phi);
        std::vector<std::uint32_t> current;
        current.reserve(segments);
        for (std::size_t index = 0U; index < segments; ++index) {
            const float angle = 6.28318530718F * static_cast<float>(index) /
                                static_cast<float>(segments);
            const float ca = std::cos(angle);
            const float sa = std::sin(angle);
            current.push_back(vertex({radii.x * cp * ca, radii.y * sp, radii.z * cp * sa},
                                     {cp * ca, sp, cp * sa},
                                     {static_cast<float>(index) / static_cast<float>(segments),
                                      static_cast<float>(row) / static_cast<float>(rows)}));
        }
        if (previous.empty()) {
            for (std::size_t index = 0U; index < segments; ++index) {
                const std::size_t next = (index + 1U) % segments;
                appendOriented(builder, south, current[next], current[index]);
            }
        } else {
            for (std::size_t index = 0U; index < segments; ++index) {
                const std::size_t next = (index + 1U) % segments;
                appendOriented(builder, previous[index], current[index], previous[next]);
                appendOriented(builder, previous[next], current[index], current[next]);
            }
        }
        previous = std::move(current);
    }
    const std::uint32_t north = vertex({0.0F, radii.y, 0.0F},
                                       {0.0F, 1.0F, 0.0F}, {0.5F, 1.0F});
    for (std::size_t index = 0U; index < previous.size(); ++index) {
        const std::size_t next = (index + 1U) % previous.size();
        appendOriented(builder, north, previous[index], previous[next]);
    }
}

void appendProfileRing(AppearanceMeshBuilder& builder, const HeadCrossSection& section,
                       std::size_t segments, foundation::Color color,
                       std::uint16_t material_region,
                       std::span<const SkinInfluence> influences, float uv_v,
                       geometry::Ring& ring) {
    std::vector<AppearanceVertexSpec> specs;
    specs.reserve(segments);
    for (std::size_t index = 0U; index < segments; ++index) {
        const float angle = 6.28318530718F * static_cast<float>(index) /
                            static_cast<float>(segments);
        const Vec3 normal{std::cos(angle), 0.0F, std::sin(angle)};
        specs.push_back({{section.half_width * normal.x, section.y,
                          section.center_z + section.half_depth * normal.z},
                         normal,
                         {static_cast<float>(index) / static_cast<float>(segments), uv_v},
                         color, material_region, influences});
    }
    ring = builder.appendRing(specs);
}

void cap(AppearanceMeshBuilder& builder, const geometry::Ring& ring, Vec3 center,
         foundation::Color color, std::span<const SkinInfluence> influences) {
    const auto center_index = builder.appendVertex(
        {center, {0.0F, 1.0F, 0.0F}, {0.5F, 0.5F}, color,
         static_cast<std::uint16_t>(AppearanceMaterialRegion::Skin), influences});
    for (std::size_t index = 0U; index < ring.indices.size(); ++index) {
        const std::size_t next = (index + 1U) % ring.indices.size();
        appendOriented(builder, center_index, ring.indices[index], ring.indices[next]);
    }
}

} // namespace

foundation::Result<void, foundation::Error> FaceSurfaceGenerator::build(
    AppearanceMeshBuilder& builder, const ResolvedAnatomy& anatomy, const FacePhenotype& face,
    const SkeletonData& skeleton, const AppearanceOptions& options,
    const geometry::Ring& neck_ring) {
    std::uint8_t head_count = 0U;
    const auto head_weights = oneBone(skeleton, BoneId::Head, head_count);
    if (head_count == 0U) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "infantry face requires a head bone"});
    }
    const std::size_t profile_segments = options.detail_level >= 3U ? 24U :
        options.detail_level == 1U ? 10U : 16U;
    const std::span<const SkinInfluence> head_span(head_weights.data(), head_count);
    geometry::Ring previous = neck_ring;
    const float neck_y = neck_ring.indices.empty()
        ? face.mouth_y - 0.04F
        : builder.mesh().vertices[neck_ring.indices.front()].position.y;
    const std::array<float, 17U> requested_levels{
        neck_y + 0.001F,
        face.mouth_y - 0.035F, face.mouth_y - 0.012F, face.mouth_y,
        face.mouth_y + 0.012F, face.nose_y - 0.018F, face.nose_y,
        face.nose_y + 0.018F, face.eye_y - face.eye_radius * 1.35F,
        face.eye_y, face.eye_y + face.eye_radius * 1.35F, face.brow_y,
        face.brow_y + (face.hairline_y - face.brow_y) * 0.30F,
        face.brow_y + (face.hairline_y - face.brow_y) * 0.60F,
        face.brow_y + (face.hairline_y - face.brow_y) * 0.84F,
        face.hairline_y, face.hairline_y + 0.018F};
    std::vector<float> levels;
    levels.reserve(requested_levels.size());
    for (const float level : requested_levels) {
        if (std::isfinite(level) &&
            (levels.empty() || level > levels.back() + geometry::kSectionOrderingEpsilon)) {
            levels.push_back(level);
        }
    }
    bool has_left_eye_opening = false;
    bool has_right_eye_opening = false;
    bool has_mouth_opening = false;
    for (const float y : levels) {
        const HeadCrossSection section = FaceAnatomyEvaluator::sectionAt(anatomy, y);
        const auto neck_head = normalizeTopFour(std::array<SkinInfluence, 2U>{
            SkinInfluence{static_cast<std::uint16_t>(boneIndex(BoneId::Neck)), 0.65F},
            SkinInfluence{static_cast<std::uint16_t>(boneIndex(BoneId::Head)), 0.35F}});
        const auto& normalized = y <= neck_y + 0.010F ? neck_head :
            normalizeTopFour(std::array<SkinInfluence, 1U>{
                SkinInfluence{static_cast<std::uint16_t>(boneIndex(BoneId::Head)), 1.0F}});
        const std::span<const SkinInfluence> weights(normalized.values.data(), normalized.count);
        geometry::Ring current;
        appendProfileRing(builder, section, profile_segments, options.skin_color,
                          static_cast<std::uint16_t>(AppearanceMaterialRegion::Skin),
                          weights, y, current);
        if (!previous.indices.empty()) {
            for (std::size_t index = 0U; index < previous.indices.size(); ++index) {
                const std::size_t next = (index + 1U) % previous.indices.size();
                const Vec3 a = builder.mesh().vertices[previous.indices[index]].position;
                const Vec3 b = builder.mesh().vertices[current.indices[index]].position;
                const float center_x = (a.x + b.x) * 0.5F;
                const float center_y = (a.y + b.y) * 0.5F;
                const float center_z = (a.z + b.z) * 0.5F;
                const HeadCrossSection center_section =
                    FaceAnatomyEvaluator::sectionAt(anatomy, center_y);
                const bool front_side = center_z > center_section.center_z +
                                        center_section.half_depth * 0.08F;
                const bool eye_hole = front_side &&
                                      std::abs(center_y - face.eye_y) < face.eye_radius * 1.35F &&
                                      std::abs(std::abs(center_x) - face.eye_spacing) <
                                          face.eye_radius * 1.8F;
                const bool mouth_hole = front_side &&
                                        std::abs(center_y - face.mouth_y) < 0.018F &&
                                        std::abs(center_x) < face.mouth_width * 0.62F;
                if (eye_hole) {
                    if (center_x < 0.0F) {
                        has_left_eye_opening = true;
                    } else {
                        has_right_eye_opening = true;
                    }
                }
                if (mouth_hole) {
                    has_mouth_opening = true;
                }
                if (!eye_hole && !mouth_hole) {
                    appendOriented(builder, previous.indices[index], current.indices[index],
                                   previous.indices[next]);
                    appendOriented(builder, previous.indices[next], current.indices[index],
                                   current.indices[next]);
                }
            }
        }
        previous = std::move(current);
    }
    if (!has_left_eye_opening || !has_right_eye_opening || !has_mouth_opening) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState,
             "face profile did not emit both eye and mouth openings"});
    }
    if (!previous.indices.empty()) {
        const HeadCrossSection crown = FaceAnatomyEvaluator::sectionAt(
            anatomy, face.hairline_y + 0.018F);
        cap(builder, previous, {0.0F, face.hairline_y + 0.018F, crown.center_z},
            options.skin_color, head_span);
    }
    const std::size_t segments = options.detail_level >= 3U ? 16U :
        options.detail_level == 1U ? 8U : 12U;
    appendOpening(builder, -face.eye_spacing, face.eye_y - face.eye_asymmetry * 0.5F,
                  face.eye_radius * 1.45F,
                  face.eye_radius * 0.90F, face.frontZ(face.eye_y) + 0.001F, segments,
                  shade(options.skin_color, 0.96F), 2U, head_weights, head_count);
    appendOpening(builder, face.eye_spacing, face.eye_y + face.eye_asymmetry * 0.5F,
                  face.eye_radius * 1.45F,
                  face.eye_radius * 0.90F, face.frontZ(face.eye_y) + 0.001F, segments,
                  shade(options.skin_color, 0.96F), 2U, head_weights, head_count);
    appendOpening(builder, 0.0F, face.mouth_y, face.mouth_width * 0.50F, 0.010F,
                  face.frontZ(face.mouth_y) + 0.001F, segments, {0.12F, 0.035F, 0.025F, 1.0F},
                  3U, head_weights, head_count);

    const std::size_t feature_segments = std::max<std::size_t>(8U, segments);
    const std::size_t feature_rows = options.detail_level >= 3U ? 8U : 5U;
    const foundation::Color eye_white{0.86F, 0.85F, 0.78F, 1.0F};
    const foundation::Color iris_color{0.18F, 0.25F, 0.20F, 1.0F};
    const foundation::Color pupil_color{0.015F, 0.012F, 0.010F, 1.0F};
    for (const float side : {-1.0F, 1.0F}) {
        std::uint8_t eye_count = 0U;
        const auto eye_weights = oneBone(skeleton, side < 0.0F ? BoneId::EyeL : BoneId::EyeR,
                                         eye_count);
        const float eye_y = face.eye_y + side * face.eye_asymmetry * 0.5F;
        const float front = face.frontZ(eye_y) + face.eye_depth;
        appendEllipsoid(builder, {side * face.eye_spacing, eye_y,
                                  front - face.eye_radius * 0.42F},
                        {face.eye_radius * 1.18F * face.eye_width_scale,
                         face.eye_radius * 0.84F * face.eye_height_scale,
                         face.eye_radius * 0.66F}, feature_segments, feature_rows,
                        eye_white, 5U, eye_weights, eye_count);
        appendEllipsoid(builder, {side * face.eye_spacing, eye_y,
                                  front + face.eye_radius * 0.28F},
                        {face.eye_radius * 0.46F, face.eye_radius * 0.46F,
                         face.eye_radius * 0.12F}, feature_segments, 3U,
                        iris_color, 6U, eye_weights, eye_count);
        appendEllipsoid(builder, {side * face.eye_spacing, eye_y,
                                  front + face.eye_radius * 0.39F},
                        {face.eye_radius * 0.17F, face.eye_radius * 0.17F,
                         face.eye_radius * 0.06F}, feature_segments, 2U,
                        pupil_color, 7U, eye_weights, eye_count);
        const HeadCrossSection eye_section = FaceAnatomyEvaluator::sectionAt(anatomy, face.eye_y);
        appendEllipsoid(builder, {side * eye_section.half_width * 0.96F,
                                  eye_y - face.eye_radius * 0.25F, eye_section.center_z},
                        {face.eye_radius * 0.46F * face.eye_width_scale,
                         face.eye_radius * 1.25F * face.eye_height_scale,
                         face.eye_radius * 0.30F}, feature_segments, 4U,
                        shade(options.skin_color, 0.86F), 8U, head_weights, head_count);
        appendEllipsoid(builder, side < 0.0F ? anatomy.face.left_ear : anatomy.face.right_ear,
                        {face.eye_radius * 0.58F * face.ear_scale,
                         face.eye_radius * 1.15F * face.ear_scale,
                         face.eye_radius * 0.20F * face.ear_scale},
                        std::max<std::size_t>(8U, feature_segments / 2U), 4U,
                        shade(options.skin_color, 0.90F),
                        static_cast<std::uint16_t>(AppearanceMaterialRegion::Ear), head_weights,
                        head_count, side * face.ear_angle);

        const float brow_y = face.brow_y + side * face.brow_asymmetry * 0.5F;
        appendEllipsoid(builder, {side * face.eye_spacing, brow_y,
                                  face.frontZ(brow_y) + face.brow_ridge + 0.0015F},
                        {face.eye_radius * 0.92F, face.eye_radius * 0.16F,
                         face.eye_radius * 0.12F},
                        feature_segments, 2U, shade(face.hair_color, 0.82F),
                        static_cast<std::uint16_t>(AppearanceMaterialRegion::Hair), head_weights,
                        head_count, side * face.eye_tilt * 0.35F);
    }
    const float nose_front = face.frontZ(face.nose_y);
    BodySurfaceGenerator::appendProfiledLimb(
        builder, {0.0F, face.eye_y - face.eye_radius, nose_front},
        {0.0F, face.nose_y, nose_front + face.nose_length * 0.34F},
        face.nose_width * 0.28F * face.nose_bridge_scale,
        face.nose_width * 0.52F * face.nose_tip_width_scale, feature_segments,
        shade(options.skin_color, 1.04F), 9U, head_weights, head_count,
        head_weights, head_count);
    std::uint8_t jaw_count = 0U;
    const auto jaw_weights = normalizeTopFour(std::array<SkinInfluence, 2U>{
        SkinInfluence{static_cast<std::uint16_t>(boneIndex(BoneId::Head)), 0.55F},
        SkinInfluence{static_cast<std::uint16_t>(boneIndex(BoneId::Jaw)), 0.45F}});
    jaw_count = jaw_weights.count;
    appendEllipsoid(builder, {0.0F, face.mouth_y + face.mouth_width * 0.02F,
                              face.frontZ(face.mouth_y) + 0.0018F},
                    {face.mouth_width * 0.48F, face.upper_lip, 0.0014F}, feature_segments, 2U,
                    shade(options.skin_color, 1.08F), 10U, jaw_weights.values, jaw_count);
    appendEllipsoid(builder, {0.0F, face.mouth_y - 0.0028F,
                              face.frontZ(face.mouth_y) + 0.0019F},
                    {face.mouth_width * 0.43F, face.lower_lip, 0.0013F}, feature_segments, 2U,
                    shade(options.skin_color, 0.92F), 10U, jaw_weights.values, jaw_count);
    return foundation::Result<void, foundation::Error>::success();
}

} // namespace genomes::infantry
