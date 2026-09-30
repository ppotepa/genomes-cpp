#include <genomes/geometry/PrimitiveBuilder.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace genomes::geometry {
namespace {

constexpr std::size_t kMaxPrimitiveVertices = 4'000'000U;
constexpr std::size_t kMaxPrimitiveIndices = 24'000'000U;
constexpr float kPi = 3.14159265358979323846F;

[[nodiscard]] foundation::Error invalid(const char* message) {
    return {foundation::ErrorCode::InvalidArgument, message};
}

[[nodiscard]] foundation::Error too_large(const char* message) {
    return {foundation::ErrorCode::OutOfRange, message};
}

[[nodiscard]] bool finite_positive(float value) noexcept {
    return std::isfinite(value) && value > 0.0F;
}

[[nodiscard]] bool safe_counts(std::size_t vertices, std::size_t indices) noexcept {
    return vertices > 0U && vertices <= kMaxPrimitiveVertices &&
           indices <= kMaxPrimitiveIndices &&
           indices <= std::numeric_limits<std::uint32_t>::max() &&
           vertices <= std::numeric_limits<std::uint32_t>::max();
}

[[nodiscard]] foundation::Result<MeshData, foundation::Error> finish(MeshData mesh) {
    using Result = foundation::Result<MeshData, foundation::Error>;
    if (!safe_counts(mesh.vertices.size(), mesh.indices.size()))
        return Result::failure(too_large("primitive exceeds bounded mesh limits"));
    mesh.rebuildStreams();
    mesh.submeshes.push_back({0U, static_cast<std::uint32_t>(mesh.indices.size()), 0U});
    if (!mesh.valid()) return Result::failure(invalid("primitive generated invalid mesh data"));
    return Result::success(std::move(mesh));
}

[[nodiscard]] std::uint32_t add_vertex(MeshData& mesh, foundation::Vec3 position,
                                       foundation::Vec3 normal, foundation::Vec2 uv) {
    const auto index = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({position, normal, uv});
    return index;
}

[[nodiscard]] foundation::Result<MeshData, foundation::Error>
make_grid_impl(float width, float depth, std::uint32_t segments_x,
               std::uint32_t segments_z) {
    using Result = foundation::Result<MeshData, foundation::Error>;
    if (!finite_positive(width) || !finite_positive(depth) || segments_x == 0U ||
        segments_z == 0U)
        return Result::failure(invalid("grid dimensions must be positive and segmented"));
    const std::size_t row = static_cast<std::size_t>(segments_x) + 1U;
    const std::size_t rows = static_cast<std::size_t>(segments_z) + 1U;
    if (row > kMaxPrimitiveVertices || rows > kMaxPrimitiveVertices / row)
        return Result::failure(too_large("grid segmentation exceeds bounded mesh limits"));
    const std::size_t vertex_count = row * rows;
    if (vertex_count > kMaxPrimitiveVertices)
        return Result::failure(too_large("grid segmentation exceeds bounded mesh limits"));
    const std::size_t cell_count = static_cast<std::size_t>(segments_x) * segments_z;
    if (cell_count > kMaxPrimitiveIndices / 6U)
        return Result::failure(too_large("grid segmentation exceeds bounded mesh limits"));
    const std::size_t index_count = cell_count * 6U;
    MeshData mesh;
    mesh.vertices.reserve(vertex_count);
    mesh.indices.reserve(index_count);
    for (std::uint32_t z = 0U; z <= segments_z; ++z) {
        const float v = static_cast<float>(z) / static_cast<float>(segments_z);
        const float position_z = (v - 0.5F) * depth;
        for (std::uint32_t x = 0U; x <= segments_x; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(segments_x);
            mesh.vertices.push_back({{(u - 0.5F) * width, 0.0F, position_z},
                                     {0.0F, 1.0F, 0.0F}, {u, v}});
        }
    }
    for (std::uint32_t z = 0U; z < segments_z; ++z) {
        for (std::uint32_t x = 0U; x < segments_x; ++x) {
            const auto a = z * static_cast<std::uint32_t>(row) + x;
            const auto b = a + 1U;
            const auto d = a + static_cast<std::uint32_t>(row);
            const auto c = d + 1U;
            mesh.indices.insert(mesh.indices.end(), {a, d, c, a, c, b});
        }
    }
    return finish(std::move(mesh));
}

} // namespace

foundation::Result<MeshData, foundation::Error> makeBoxResult(const BoxSpec& spec) {
    MeshData mesh;
    if (!std::isfinite(spec.size.x) || !std::isfinite(spec.size.y) ||
        !std::isfinite(spec.size.z) || spec.size.x <= 0.0F ||
        spec.size.y <= 0.0F || spec.size.z <= 0.0F) {
        return foundation::Result<MeshData, foundation::Error>::failure(
            invalid("box dimensions must be finite and positive"));
    }
    const foundation::Vec3 h{spec.size.x * 0.5F, spec.size.y * 0.5F, spec.size.z * 0.5F};
    const std::array<foundation::Vec3, 8> corners{{
        {-h.x, -h.y, -h.z}, {h.x, -h.y, -h.z}, {h.x, h.y, -h.z}, {-h.x, h.y, -h.z},
        {-h.x, -h.y, h.z},  {h.x, -h.y, h.z},  {h.x, h.y, h.z},  {-h.x, h.y, h.z}}};
    constexpr std::array<std::array<std::uint32_t, 4>, 6> faces{{
        {{0, 3, 2, 1}}, {{4, 5, 6, 7}}, {{0, 4, 7, 3}},
        {{1, 2, 6, 5}}, {{3, 7, 6, 2}}, {{0, 1, 5, 4}}}};
    constexpr std::array<foundation::Vec3, 6> normals{{
        {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}}};
    constexpr std::array<foundation::Vec2, 4> uvs{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
    mesh.vertices.reserve(24U);
    mesh.indices.reserve(36U);
    for (std::size_t face = 0; face < faces.size(); ++face) {
        const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
        for (std::size_t corner = 0; corner < 4U; ++corner)
            mesh.vertices.push_back({corners[faces[face][corner]], normals[face], uvs[corner]});
        mesh.indices.insert(mesh.indices.end(),
                            {base, base + 1U, base + 2U, base, base + 2U, base + 3U});
    }
    return finish(std::move(mesh));
}

MeshData makeBox(const BoxSpec& spec) {
    auto result = makeBoxResult(spec);
    return result ? std::move(result).value() : MeshData{};
}

foundation::Result<MeshData, foundation::Error> makePlaneResult(const PlaneSpec& spec) {
    return make_grid_impl(spec.width, spec.depth, spec.segments_x, spec.segments_z);
}

foundation::Result<MeshData, foundation::Error> makeGridResult(const GridSpec& spec) {
    return make_grid_impl(spec.width, spec.depth, spec.segments_x, spec.segments_z);
}

foundation::Result<MeshData, foundation::Error> makeUvSphereResult(const UvSphereSpec& spec) {
    using Result = foundation::Result<MeshData, foundation::Error>;
    if (!finite_positive(spec.radius) || spec.segments < 3U || spec.rings < 2U)
        return Result::failure(invalid("sphere radius or segmentation is invalid"));
    const std::size_t stride = static_cast<std::size_t>(spec.segments) + 1U;
    const std::size_t intermediate = static_cast<std::size_t>(spec.rings) - 1U;
    if (stride > kMaxPrimitiveVertices || intermediate > kMaxPrimitiveVertices / stride ||
        static_cast<std::size_t>(spec.segments) >
            kMaxPrimitiveIndices / (static_cast<std::size_t>(spec.rings) * 6U))
        return Result::failure(too_large("sphere segmentation exceeds bounded mesh limits"));
    MeshData mesh;
    mesh.vertices.reserve(2U + intermediate * stride);
    mesh.indices.reserve(static_cast<std::size_t>(spec.segments) * spec.rings * 6U);
    const auto top = add_vertex(mesh, {0.0F, spec.radius, 0.0F}, {0.0F, 1.0F, 0.0F}, {0, 0});
    std::vector<std::vector<std::uint32_t>> rings;
    rings.reserve(intermediate);
    for (std::uint32_t ring = 1U; ring < spec.rings; ++ring) {
        const float v = static_cast<float>(ring) / static_cast<float>(spec.rings);
        const float phi = v * kPi;
        const float y = std::cos(phi);
        const float radial = std::sin(phi);
        std::vector<std::uint32_t> row_ids;
        row_ids.reserve(stride);
        for (std::uint32_t segment = 0U; segment <= spec.segments; ++segment) {
            const float u = static_cast<float>(segment) / static_cast<float>(spec.segments);
            const float theta = u * 2.0F * kPi;
            const foundation::Vec3 normal{radial * std::cos(theta), y, radial * std::sin(theta)};
            row_ids.push_back(add_vertex(mesh, normal * spec.radius, normal, {u, v}));
        }
        rings.push_back(std::move(row_ids));
    }
    const auto bottom = add_vertex(mesh, {0.0F, -spec.radius, 0.0F}, {0.0F, -1.0F, 0.0F}, {0, 1});
    for (std::uint32_t segment = 0U; segment < spec.segments; ++segment)
        mesh.indices.insert(mesh.indices.end(),
                            {top, rings.front()[segment + 1U], rings.front()[segment]});
    for (std::size_t ring = 0U; ring + 1U < rings.size(); ++ring) {
        for (std::uint32_t segment = 0U; segment < spec.segments; ++segment) {
            const auto a = rings[ring][segment];
            const auto b = rings[ring][segment + 1U];
            const auto d = rings[ring + 1U][segment];
            const auto c = rings[ring + 1U][segment + 1U];
            mesh.indices.insert(mesh.indices.end(), {a, b, c, a, c, d});
        }
    }
    const auto& last = rings.back();
    for (std::uint32_t segment = 0U; segment < spec.segments; ++segment)
        mesh.indices.insert(mesh.indices.end(), {last[segment], last[segment + 1U], bottom});
    return finish(std::move(mesh));
}

foundation::Result<MeshData, foundation::Error> makeCylinderResult(const CylinderSpec& spec) {
    using Result = foundation::Result<MeshData, foundation::Error>;
    if (!finite_positive(spec.radius) || !finite_positive(spec.height) || spec.segments < 3U)
        return Result::failure(invalid("cylinder dimensions or segmentation is invalid"));
    const std::size_t stride = static_cast<std::size_t>(spec.segments) + 1U;
    const std::size_t cap_vertices = spec.caps ? 2U * (stride + 1U) : 0U;
    const std::size_t indices_per_segment = spec.caps ? 12U : 6U;
    if (2U * stride + cap_vertices > kMaxPrimitiveVertices ||
        static_cast<std::size_t>(spec.segments) > kMaxPrimitiveIndices / indices_per_segment)
        return Result::failure(too_large("cylinder segmentation exceeds bounded mesh limits"));
    MeshData mesh;
    mesh.vertices.reserve(2U * stride + cap_vertices);
    mesh.indices.reserve(static_cast<std::size_t>(spec.segments) * (6U + (spec.caps ? 6U : 0U)));
    const float half_height = spec.height * 0.5F;
    for (std::uint32_t ring = 0U; ring < 2U; ++ring) {
        const float v = static_cast<float>(ring);
        const float y = ring == 0U ? -half_height : half_height;
        for (std::uint32_t segment = 0U; segment <= spec.segments; ++segment) {
            const float u = static_cast<float>(segment) / static_cast<float>(spec.segments);
            const float theta = u * 2.0F * kPi;
            const foundation::Vec3 normal{std::cos(theta), 0.0F, std::sin(theta)};
            add_vertex(mesh, {spec.radius * normal.x, y, spec.radius * normal.z}, normal, {u, v});
        }
    }
    for (std::uint32_t segment = 0U; segment < spec.segments; ++segment) {
        const auto a = segment;
        const auto b = a + 1U;
        const auto d = static_cast<std::uint32_t>(stride) + segment;
        const auto c = d + 1U;
        mesh.indices.insert(mesh.indices.end(), {a, d, b, b, d, c});
    }
    if (spec.caps) {
        const auto bottom_center = add_vertex(mesh, {0, -half_height, 0}, {0, -1, 0}, {0.5F, 0.5F});
        const auto bottom_ring = static_cast<std::uint32_t>(mesh.vertices.size());
        for (std::uint32_t segment = 0U; segment <= spec.segments; ++segment) {
            const float u = static_cast<float>(segment) / static_cast<float>(spec.segments);
            const float theta = u * 2.0F * kPi;
            add_vertex(mesh, {spec.radius * std::cos(theta), -half_height,
                              spec.radius * std::sin(theta)},
                       {0, -1, 0}, {0.5F + 0.5F * std::cos(theta),
                                    0.5F + 0.5F * std::sin(theta)});
        }
        const auto top_center = add_vertex(mesh, {0, half_height, 0}, {0, 1, 0}, {0.5F, 0.5F});
        const auto top_ring = static_cast<std::uint32_t>(mesh.vertices.size());
        for (std::uint32_t segment = 0U; segment <= spec.segments; ++segment) {
            const float u = static_cast<float>(segment) / static_cast<float>(spec.segments);
            const float theta = u * 2.0F * kPi;
            add_vertex(mesh, {spec.radius * std::cos(theta), half_height,
                              spec.radius * std::sin(theta)},
                       {0, 1, 0}, {0.5F + 0.5F * std::cos(theta),
                                   0.5F + 0.5F * std::sin(theta)});
        }
        for (std::uint32_t segment = 0U; segment < spec.segments; ++segment) {
            mesh.indices.insert(mesh.indices.end(),
                                {bottom_center, bottom_ring + segment,
                                 bottom_ring + segment + 1U});
            mesh.indices.insert(mesh.indices.end(),
                                {top_center, top_ring + segment + 1U, top_ring + segment});
        }
    }
    return finish(std::move(mesh));
}

foundation::Result<MeshData, foundation::Error> makeConeResult(const ConeSpec& spec) {
    using Result = foundation::Result<MeshData, foundation::Error>;
    if (!finite_positive(spec.radius) || !finite_positive(spec.height) || spec.segments < 3U)
        return Result::failure(invalid("cone dimensions or segmentation is invalid"));
    const std::size_t stride = static_cast<std::size_t>(spec.segments) + 1U;
    const std::size_t cap_vertices = spec.cap ? stride + 1U : 0U;
    const std::size_t indices_per_segment = spec.cap ? 6U : 3U;
    if (stride + spec.segments + cap_vertices > kMaxPrimitiveVertices ||
        static_cast<std::size_t>(spec.segments) > kMaxPrimitiveIndices / indices_per_segment)
        return Result::failure(too_large("cone segmentation exceeds bounded mesh limits"));
    MeshData mesh;
    mesh.vertices.reserve(stride + spec.segments + cap_vertices);
    mesh.indices.reserve(static_cast<std::size_t>(spec.segments) * (3U + (spec.cap ? 3U : 0U)));
    const float half_height = spec.height * 0.5F;
    const float slope = spec.radius / spec.height;
    for (std::uint32_t segment = 0U; segment <= spec.segments; ++segment) {
        const float u = static_cast<float>(segment) / static_cast<float>(spec.segments);
        const float theta = u * 2.0F * kPi;
        const foundation::Vec3 normal = math::normalized({std::cos(theta), slope, std::sin(theta)});
        add_vertex(mesh, {spec.radius * std::cos(theta), -half_height,
                          spec.radius * std::sin(theta)}, normal, {u, 1.0F});
    }
    const auto apex_start = static_cast<std::uint32_t>(mesh.vertices.size());
    for (std::uint32_t segment = 0U; segment < spec.segments; ++segment) {
        const float u = (static_cast<float>(segment) + 0.5F) /
                        static_cast<float>(spec.segments);
        const float theta = u * 2.0F * kPi;
        const foundation::Vec3 normal = math::normalized({std::cos(theta), slope, std::sin(theta)});
        add_vertex(mesh, {0.0F, half_height, 0.0F}, normal, {u, 0.0F});
    }
    for (std::uint32_t segment = 0U; segment < spec.segments; ++segment)
        mesh.indices.insert(mesh.indices.end(),
                            {segment, apex_start + segment, segment + 1U});
    if (spec.cap) {
        const auto center = add_vertex(mesh, {0, -half_height, 0}, {0, -1, 0}, {0.5F, 0.5F});
        const auto ring = static_cast<std::uint32_t>(mesh.vertices.size());
        for (std::uint32_t segment = 0U; segment <= spec.segments; ++segment) {
            const float u = static_cast<float>(segment) / static_cast<float>(spec.segments);
            const float theta = u * 2.0F * kPi;
            add_vertex(mesh, {spec.radius * std::cos(theta), -half_height,
                              spec.radius * std::sin(theta)},
                       {0, -1, 0}, {0.5F + 0.5F * std::cos(theta),
                                    0.5F + 0.5F * std::sin(theta)});
        }
        for (std::uint32_t segment = 0U; segment < spec.segments; ++segment)
            mesh.indices.insert(mesh.indices.end(), {center, ring + segment, ring + segment + 1U});
    }
    return finish(std::move(mesh));
}

foundation::Result<MeshData, foundation::Error> makeCapsuleResult(const CapsuleSpec& spec) {
    using Result = foundation::Result<MeshData, foundation::Error>;
    if (!finite_positive(spec.radius) || !finite_positive(spec.height) || spec.segments < 3U ||
        spec.hemisphere_rings < 2U || spec.body_rings == 0U)
        return Result::failure(invalid("capsule dimensions or segmentation is invalid"));
    const std::size_t profile_rings = static_cast<std::size_t>(spec.hemisphere_rings) * 2U +
                                      spec.body_rings - 1U;
    const std::size_t stride = static_cast<std::size_t>(spec.segments) + 1U;
    if (profile_rings > kMaxPrimitiveVertices / stride ||
        profile_rings > kMaxPrimitiveIndices /
                             (static_cast<std::size_t>(spec.segments) * 6U))
        return Result::failure(too_large("capsule segmentation exceeds bounded mesh limits"));
    MeshData mesh;
    mesh.vertices.reserve(2U + profile_rings * stride);
    mesh.indices.reserve(profile_rings * static_cast<std::size_t>(spec.segments) * 6U);
    const float top_center = spec.height * 0.5F;
    const float bottom_center = -top_center;
    const std::size_t total_steps = static_cast<std::size_t>(spec.hemisphere_rings) * 2U +
                                    spec.body_rings;
    const auto top = add_vertex(mesh, {0, top_center + spec.radius, 0}, {0, 1, 0}, {0, 0});
    std::vector<std::vector<std::uint32_t>> rings;
    rings.reserve(profile_rings);
    auto add_ring = [&](float y, float radial, float normal_y, std::size_t step) {
        std::vector<std::uint32_t> row;
        row.reserve(stride);
        for (std::uint32_t segment = 0U; segment <= spec.segments; ++segment) {
            const float u = static_cast<float>(segment) / static_cast<float>(spec.segments);
            const float theta = u * 2.0F * kPi;
            const float radial_factor = radial / spec.radius;
            const foundation::Vec3 normal{radial_factor * std::cos(theta), normal_y,
                                          radial_factor * std::sin(theta)};
            row.push_back(add_vertex(mesh, {radial * std::cos(theta), y,
                                             radial * std::sin(theta)},
                                     normal, {u, static_cast<float>(step) /
                                                   static_cast<float>(total_steps)}));
        }
        rings.push_back(std::move(row));
    };
    for (std::uint32_t ring = 1U; ring <= spec.hemisphere_rings; ++ring) {
        const float angle = static_cast<float>(ring) * (0.5F * kPi) /
                            static_cast<float>(spec.hemisphere_rings);
        add_ring(top_center + spec.radius * std::cos(angle),
                 spec.radius * std::sin(angle), std::cos(angle), ring);
    }
    for (std::uint32_t ring = 1U; ring <= spec.body_rings; ++ring) {
        const auto step = static_cast<std::size_t>(spec.hemisphere_rings) + ring;
        add_ring(top_center - spec.height * static_cast<float>(ring) /
                                static_cast<float>(spec.body_rings),
                 spec.radius, 0.0F, step);
    }
    for (std::uint32_t ring = 1U; ring < spec.hemisphere_rings; ++ring) {
        const float angle = static_cast<float>(ring) * (0.5F * kPi) /
                            static_cast<float>(spec.hemisphere_rings);
        const auto step = static_cast<std::size_t>(spec.hemisphere_rings) +
                          spec.body_rings + ring;
        add_ring(bottom_center - spec.radius * std::sin(angle),
                 spec.radius * std::cos(angle), -std::sin(angle), step);
    }
    const auto bottom = add_vertex(mesh, {0, bottom_center - spec.radius, 0}, {0, -1, 0}, {0, 1});
    for (std::uint32_t segment = 0U; segment < spec.segments; ++segment)
        mesh.indices.insert(mesh.indices.end(),
                            {top, rings.front()[segment + 1U], rings.front()[segment]});
    for (std::size_t ring = 0U; ring + 1U < rings.size(); ++ring) {
        for (std::uint32_t segment = 0U; segment < spec.segments; ++segment) {
            const auto a = rings[ring][segment];
            const auto b = rings[ring][segment + 1U];
            const auto d = rings[ring + 1U][segment];
            const auto c = rings[ring + 1U][segment + 1U];
            mesh.indices.insert(mesh.indices.end(), {a, b, c, a, c, d});
        }
    }
    const auto& last = rings.back();
    for (std::uint32_t segment = 0U; segment < spec.segments; ++segment)
        mesh.indices.insert(mesh.indices.end(), {last[segment], last[segment + 1U], bottom});
    return finish(std::move(mesh));
}

} // namespace genomes::geometry
