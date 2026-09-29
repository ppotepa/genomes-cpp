#include <genomes/infantry/HairGenerator.hpp>

#include <genomes/geometry/GeometryConstants.hpp>
#include <genomes/proc/RandomStream.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace genomes::infantry {

namespace {

using foundation::Vec2;
using foundation::Vec3;

[[nodiscard]] Vec3 multiply(Vec3 value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] Vec3 subtract(Vec3 left, Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] float length(Vec3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

[[nodiscard]] Vec3 normalized(Vec3 value) noexcept {
    const float magnitude = length(value);
    return magnitude > geometry::kNormalLengthEpsilon
        ? multiply(value, 1.0F / magnitude) : Vec3{0.0F, 1.0F, 0.0F};
}

[[nodiscard]] Vec3 cross(Vec3 left, Vec3 right) noexcept {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

[[nodiscard]] float dot(Vec3 left, Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] foundation::Color shade(foundation::Color color, float factor) noexcept {
    return {std::clamp(color.r * factor, 0.0F, 1.0F),
            std::clamp(color.g * factor, 0.0F, 1.0F),
            std::clamp(color.b * factor, 0.0F, 1.0F), color.a};
}

[[nodiscard]] std::uint16_t headIndex(const SkeletonData& skeleton) noexcept {
    const BoneRecord* head = skeleton.find(BoneId::Head);
    return head == nullptr ? kInvalidBoneIndex : static_cast<std::uint16_t>(boneIndex(BoneId::Head));
}

struct StyleShape final {
    float side{0.0F};
    float top{0.0F};
    float front{0.0F};
    float part{0.0F};
};

[[nodiscard]] StyleShape styleShape(HairStyle style) noexcept {
    switch (style) {
    case HairStyle::Buzz: return {0.12F, 0.10F, 0.08F, 0.0F};
    case HairStyle::Crew: return {0.36F, 0.72F, 0.40F, 0.0F};
    case HairStyle::Crop: return {0.30F, 0.82F, 0.65F, 0.0F};
    case HairStyle::SidePart: return {0.32F, 0.93F, 0.50F, 0.35F};
    case HairStyle::Fade: return {0.12F, 0.94F, 0.52F, 0.0F};
    case HairStyle::Messy: return {0.42F, 1.05F, 0.67F, 0.0F};
    case HairStyle::Bald: break;
    }
    return {};
}

void appendRing(AppearanceMeshBuilder& builder, const ResolvedAnatomy& anatomy,
                const FacePhenotype& face, float row_t, float hair_coverage,
                std::size_t segments, foundation::Color color,
                std::uint16_t bone, HairStyle style) {
    std::vector<AppearanceVertexSpec> specs;
    specs.reserve(segments);
    constexpr float kTau = 6.2831853071795864769F;
    const SkinInfluence influence{bone, 1.0F};
    const StyleShape shape = styleShape(style);
    for (std::size_t index = 0U; index < segments; ++index) {
        const float angle = kTau * static_cast<float>(index) / static_cast<float>(segments);
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        const float front = std::max(0.0F, s);
        const float side = std::abs(c);
        const float bottom = FaceAnatomyEvaluator::hairlineY(anatomy, angle) +
            (style == HairStyle::Fade ? 0.0025F * side : 0.0F);
        const float scalp_y = bottom + (anatomy.scalp.crown_y - 0.0003F - bottom) * row_t;
        const HeadCrossSection section = FaceAnatomyEvaluator::sectionAt(anatomy, scalp_y);
        const float wave = style == HairStyle::Messy
            ? std::sin(angle * 15.0F + row_t * 13.0F) * 0.08F +
              std::sin(angle * 9.0F - row_t * 7.0F) * 0.04F
            : 0.0F;
        float radial = (0.0008F + face.hair_thickness * 0.22F +
            face.hair_volume * (shape.side * side + shape.front * front) * 0.30F) *
            std::sin((1.0F - row_t) * 1.57079632679F);
        const float covered = std::clamp(hair_coverage * front, 0.0F, 1.0F);
        radial *= 1.0F - 0.82F * covered;
        float top_lift = face.hair_volume * shape.top *
            std::pow(row_t, 1.4F) * 0.58F;
        top_lift *= 1.0F - 0.88F * covered;
        const float part = shape.part * top_lift * std::sin(row_t * 3.14159265359F);
        const Vec3 position{
            section.half_width * c + c * radial * (1.0F + wave) + part,
            scalp_y + top_lift * (1.0F + wave),
            section.center_z + section.half_depth * s + s * radial * (1.0F + wave)};
        specs.push_back({position, normalized({c * (1.0F - row_t * 0.75F),
                                              row_t,
                                              s * (1.0F - row_t * 0.75F)}),
                         {static_cast<float>(index) / static_cast<float>(segments), row_t},
                         color, 4U, std::span<const SkinInfluence>(&influence, 1U)});
    }
    (void)builder.appendRing(specs);
}

void bridge(AppearanceMeshBuilder& builder, const geometry::Ring& first,
            const geometry::Ring& second) {
    const auto append_oriented = [&builder](std::uint32_t a, std::uint32_t b,
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
    };
    const std::size_t count = std::min(first.indices.size(), second.indices.size());
    for (std::size_t index = 0U; index < count; ++index) {
        const std::size_t next = (index + 1U) % count;
        append_oriented(first.indices[index], second.indices[index], first.indices[next]);
        append_oriented(first.indices[next], second.indices[index], second.indices[next]);
    }
}

void cap(AppearanceMeshBuilder& builder, const geometry::Ring& ring, Vec3 center,
         foundation::Color color, std::uint16_t bone) {
    const SkinInfluence influence{bone, 1.0F};
    const AppearanceVertexSpec spec{center, {0.0F, 1.0F, 0.0F}, {0.5F, 0.5F}, color, 4U,
                                    std::span<const SkinInfluence>(&influence, 1U)};
    const auto center_index = builder.appendVertex(spec);
    for (std::size_t index = 0U; index < ring.indices.size(); ++index) {
        const std::size_t next = (index + 1U) % ring.indices.size();
        const auto& vertices = builder.mesh().vertices;
        const Vec3 edge_a = subtract(vertices[ring.indices[index]].position, center);
        const Vec3 edge_b = subtract(vertices[ring.indices[next]].position, center);
        if (dot(cross(edge_a, edge_b), {0.0F, 1.0F, 0.0F}) < 0.0F) {
            builder.appendTriangle(center_index, ring.indices[next], ring.indices[index]);
        } else {
            builder.appendTriangle(center_index, ring.indices[index], ring.indices[next]);
        }
    }
}


} // namespace

foundation::Result<AppearanceMesh, foundation::Error> HairGenerator::build(
    const ResolvedAnatomy& anatomy, const FacePhenotype& face, const SkeletonData& skeleton,
    const AppearanceOptions& options) {
    AppearanceMeshBuilder builder;
    const HairStyle style = canonicalHairStyle(options.hair_style);
    if (style == HairStyle::Bald) {
        return foundation::Result<AppearanceMesh, foundation::Error>::success(
            std::move(builder).finalize());
    }
    const std::uint16_t bone = headIndex(skeleton);
    if (bone == kInvalidBoneIndex) {
        return foundation::Result<AppearanceMesh, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "infantry hair requires a head bone"});
    }
    proc::RandomStream random(options.seed, foundation::stable_id("infantry.appearance.hair"));
    const float density_scale = std::clamp(0.68F + face.hair_density * 0.42F,
                                                   0.60F, 1.20F);
    const std::size_t segments = options.detail_level >= 3U
        ? std::max<std::size_t>(56U, static_cast<std::size_t>(56U * density_scale))
        : options.detail_level == 1U
            ? std::max<std::size_t>(20U, static_cast<std::size_t>(20U * density_scale))
            : std::max<std::size_t>(40U, static_cast<std::size_t>(40U * density_scale));
    foundation::Color hair_color = face.hair_color;
    if (style == HairStyle::Messy) hair_color = shade(hair_color, 0.86F);
    hair_color = shade(hair_color, face.hair_brightness *
                       (0.90F + static_cast<float>(random.uniform01()) * 0.20F));
    const std::size_t rows = options.detail_level >= 3U ? 17U :
        options.detail_level == 1U ? 6U : 12U;
    geometry::Ring previous;
    for (std::size_t row = 0U; row < rows; ++row) {
        const float t = static_cast<float>(row) / static_cast<float>(rows);
        const std::size_t before = builder.mesh().vertices.size();
        appendRing(builder, anatomy, face, t, options.hair_coverage, segments,
                   shade(hair_color, 0.92F), bone, style);
        geometry::Ring current;
        current.indices.reserve(segments);
        for (std::size_t index = 0U; index < segments; ++index)
            current.indices.push_back(static_cast<std::uint32_t>(before + index));
        if (!previous.indices.empty()) bridge(builder, previous, current);
        previous = std::move(current);
    }
    if (!previous.indices.empty()) {
        const StyleShape shape = styleShape(style);
        const HeadCrossSection crown =
            FaceAnatomyEvaluator::sectionAt(anatomy, anatomy.scalp.crown_y);
        cap(builder, previous,
            {shape.part * face.hair_volume * shape.top * 0.02F,
             anatomy.scalp.crown_y + face.hair_volume * shape.top * 0.58F,
             crown.center_z},
            hair_color, bone);
    }
    return foundation::Result<AppearanceMesh, foundation::Error>::success(
        std::move(builder).finalize());
}

} // namespace genomes::infantry
