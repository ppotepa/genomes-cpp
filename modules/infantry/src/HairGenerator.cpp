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

void appendRing(AppearanceMeshBuilder& builder, const ResolvedAnatomy& anatomy,
                const FacePhenotype& face, float row_t, float hair_height,
                float radius_scale, float hair_coverage, std::size_t segments,
                foundation::Color color, std::uint16_t bone, HairStyle style) {
    std::vector<AppearanceVertexSpec> specs;
    specs.reserve(segments);
    constexpr float kTau = 6.2831853071795864769F;
    const SkinInfluence influence{bone, 1.0F};
    for (std::size_t index = 0U; index < segments; ++index) {
        const float angle = kTau * static_cast<float>(index) / static_cast<float>(segments);
        const float front = std::max(0.0F, std::sin(angle));
        const float temple = front * std::abs(std::cos(angle));
        const float center_front = front * (1.0F - std::abs(std::cos(angle)));
        const float hairline = face.hairline_y -
            face.temple_recession * face.eye_radius * 1.8F * temple +
            face.widow_peak * face.eye_radius * 1.4F * center_front;
        // Equipment coverage is a visibility mask, not a hairstyle mutation:
        // compress covered frontal hair back toward the scalp while retaining
        // the fixed topology needed by immutable morph/skin streams.
        const float visible_scale = 1.0F - 0.82F * hair_coverage * front;
        const float y = hairline + hair_height * row_t * visible_scale;
        const HeadCrossSection section = FaceAnatomyEvaluator::sectionAt(anatomy, y);
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        const float side_part_lift = style == HairStyle::SidePart
            ? 0.012F * std::max(0.0F, c) * (1.0F - row_t) : 0.0F;
        const float crop_forward = style == HairStyle::Crop ? 0.012F * front : 0.0F;
        const float fade_scale = style == HairStyle::Fade
            ? 1.0F - 0.18F * (1.0F - front) * (1.0F - row_t) : 1.0F;
        const float messy_offset = style == HairStyle::Messy
            ? 0.006F * std::sin(angle * 3.0F + row_t * 5.0F) : 0.0F;
        const float offset = 1.0F + face.hair_thickness /
            std::max(0.001F, section.half_depth);
        const float scale = radius_scale * offset * fade_scale *
                            (1.0F - 0.35F * hair_coverage * front);
        const Vec3 position{section.half_width * scale * c, y + side_part_lift + crop_forward,
                            section.center_z + section.half_depth * scale * s + messy_offset};
        specs.push_back({position, normalized({c, 0.12F + 0.88F * row_t, s}),
                         {static_cast<float>(index) / static_cast<float>(segments), row_t},
                         color, 4U, std::span<const SkinInfluence>(&influence, 1U)});
    }
    const auto ring = builder.appendRing(specs);
    (void)ring;
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

void appendEllipsoid(AppearanceMeshBuilder& builder, Vec3 center, Vec3 radii,
                     std::size_t segments, std::size_t rows, foundation::Color color,
                     std::uint16_t region, std::uint16_t bone) {
    const SkinInfluence influence{bone, 1.0F};
    const auto vertex = [&builder, &influence, color](Vec3 position, Vec3 normal,
                                                               Vec2 uv, std::uint16_t material) {
        return builder.appendVertex({position, normal, uv, color, material,
                                     std::span<const SkinInfluence>(&influence, 1U)});
    };
    const std::uint32_t south = vertex({center.x, center.y - radii.y, center.z},
                                       {0.0F, -1.0F, 0.0F}, {0.5F, 0.0F}, region);
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
            current.push_back(vertex({center.x + radii.x * cp * ca,
                                      center.y + radii.y * sp,
                                      center.z + radii.z * cp * sa},
                                     normalized({cp * ca, sp, cp * sa}),
                                     {static_cast<float>(index) / static_cast<float>(segments),
                                      static_cast<float>(row) / static_cast<float>(rows)}, region));
        }
        if (previous.empty()) {
            for (std::size_t index = 0U; index < segments; ++index) {
                const std::size_t next = (index + 1U) % segments;
                const auto& vertices = builder.mesh().vertices;
                const Vec3 edge_a = subtract(vertices[current[next]].position,
                                             vertices[south].position);
                const Vec3 edge_b = subtract(vertices[current[index]].position,
                                             vertices[south].position);
                if (dot(cross(edge_a, edge_b), vertices[current[index]].normal) < 0.0F) {
                    builder.appendTriangle(south, current[index], current[next]);
                } else {
                    builder.appendTriangle(south, current[next], current[index]);
                }
            }
        } else {
            geometry::Ring first{previous};
            geometry::Ring second{current};
            bridge(builder, first, second);
        }
        previous = std::move(current);
    }
    const std::uint32_t north = vertex({center.x, center.y + radii.y, center.z},
                                       {0.0F, 1.0F, 0.0F}, {0.5F, 1.0F}, region);
    for (std::size_t index = 0U; index < previous.size(); ++index) {
        const std::size_t next = (index + 1U) % previous.size();
        const auto& vertices = builder.mesh().vertices;
        const Vec3 edge_a = subtract(vertices[previous[index]].position,
                                     vertices[north].position);
        const Vec3 edge_b = subtract(vertices[previous[next]].position,
                                     vertices[north].position);
        if (dot(cross(edge_a, edge_b), vertices[previous[index]].normal) < 0.0F) {
            builder.appendTriangle(north, previous[next], previous[index]);
        } else {
            builder.appendTriangle(north, previous[index], previous[next]);
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
    const float style_scale = style == HairStyle::Buzz ? 1.0F :
        style == HairStyle::Crew ? 1.08F : style == HairStyle::Crop ? 1.12F :
        style == HairStyle::SidePart ? 1.18F : style == HairStyle::Fade ? 0.94F : 1.08F;
    const float style_width = style == HairStyle::Crop ? 0.72F :
        style == HairStyle::Crew ? 1.06F : 1.0F;
    const float density_scale = std::clamp(0.68F + face.hair_density * 0.42F, 0.60F, 1.20F);
    const std::size_t base_segments = options.detail_level >= 3U ? 24U :
        options.detail_level == 1U ? 10U : 16U;
    const std::size_t segments = options.detail_level >= 3U
        ? std::max<std::size_t>(24U, static_cast<std::size_t>(base_segments * 2U * density_scale))
        : options.detail_level == 1U
            ? std::max<std::size_t>(12U, static_cast<std::size_t>(base_segments * density_scale))
            : std::max<std::size_t>(20U, static_cast<std::size_t>((base_segments + 4U) * density_scale));
    foundation::Color hair_color = face.hair_color;
    if (style == HairStyle::Messy) {
        hair_color = shade(hair_color, 0.86F);
    }
    hair_color = shade(hair_color, face.hair_brightness *
                       (0.90F + static_cast<float>(random.uniform01()) * 0.20F));
    const float hair_height = (0.030F + face.hair_volume * 3.0F) * style_scale;
    const std::size_t rows = options.detail_level >= 3U ? 12U : 8U;
    geometry::Ring previous;
    for (std::size_t row = 0U; row <= rows; ++row) {
        const float t = static_cast<float>(row) / static_cast<float>(rows);
        const float taper = 1.0F - 0.20F * t;
        const std::size_t before = builder.mesh().vertices.size();
        appendRing(builder, anatomy, face, t, hair_height,
                   (0.98F + 0.09F * style_width) * taper,
                   options.hair_coverage, segments,
                   shade(hair_color, 0.90F + 0.08F * t), bone, style);
        geometry::Ring current;
        current.indices.reserve(segments);
        for (std::size_t index = 0U; index < segments; ++index) {
            current.indices.push_back(static_cast<std::uint32_t>(before + index));
        }
        if (!previous.indices.empty()) {
            bridge(builder, previous, current);
        }
        previous = std::move(current);
    }
    if (!previous.indices.empty()) {
        const HeadCrossSection crown = FaceAnatomyEvaluator::sectionAt(
            anatomy, face.hairline_y + hair_height);
        cap(builder, previous, {0.0F, face.hairline_y + hair_height, crown.center_z},
            hair_color, bone);
    }
    if (style == HairStyle::Crew || style == HairStyle::SidePart || style == HairStyle::Messy) {
        const HeadCrossSection scalp = FaceAnatomyEvaluator::sectionAt(anatomy, face.hairline_y);
        const std::size_t style_segments = options.detail_level >= 3U ? 12U : 8U;
        appendEllipsoid(builder, {0.0F, face.hairline_y + hair_height * 1.15F, scalp.center_z},
                        {0.060F * style_scale, 0.057F * style_scale, 0.052F * style_scale},
                        style_segments, 5U, hair_color, 4U, bone);
    }
    return foundation::Result<AppearanceMesh, foundation::Error>::success(
        std::move(builder).finalize());
}

} // namespace genomes::infantry
