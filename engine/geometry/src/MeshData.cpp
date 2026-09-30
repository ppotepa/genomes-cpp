#include <genomes/geometry/MeshData.hpp>

#include <cmath>
#include <limits>
#include <utility>

namespace genomes::geometry {

namespace {
[[nodiscard]] bool finite(foundation::Vec3 v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
[[nodiscard]] bool finite(foundation::Vec2 v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y);
}
[[nodiscard]] bool finite(foundation::Color c) noexcept {
    return std::isfinite(c.r) && std::isfinite(c.g) && std::isfinite(c.b) &&
           std::isfinite(c.a);
}
}

bool MeshData::valid() const noexcept {
    const std::size_t vertex_count = positions.empty() ? vertices.size() : positions.size();
    if (vertex_count == 0U || indices.empty() || indices.size() % 3U != 0U) return false;
    if (!positions.empty() && !vertices.empty() && vertices.size() != positions.size()) return false;
    for (const auto& vertex : vertices)
        if (!finite(vertex.position) || !finite(vertex.normal) || !finite(vertex.uv)) return false;
    if (!positions.empty()) {
        for (const auto position : positions) if (!finite(position)) return false;
        for (const auto normal : normals) if (!finite(normal)) return false;
        for (const auto uv : uvs) if (!finite(uv)) return false;
    }
    for (const auto tangent : tangents) if (!math::finite(tangent)) return false;
    for (const auto color : colors) if (!finite(color)) return false;
    for (const auto index : indices)
        if (index >= vertex_count ||
            (index_format == IndexFormat::UInt16 && index > std::numeric_limits<std::uint16_t>::max()))
            return false;
    if (!positions.empty() && (normals.size() != positions.size() || uvs.size() != positions.size())) return false;
    if ((!tangents.empty() && tangents.size() != vertex_count) ||
        (!colors.empty() && colors.size() != vertex_count)) return false;
    for (const auto& range : submeshes)
        if (range.first_index % 3U != 0U || range.index_count % 3U != 0U ||
            static_cast<std::size_t>(range.first_index) + range.index_count > indices.size()) return false;
    if (!bounds.empty && (!finite(bounds.min) || !finite(bounds.max) ||
                          bounds.min.x > bounds.max.x || bounds.min.y > bounds.max.y ||
                          bounds.min.z > bounds.max.z)) return false;
    return true;
}

void MeshData::rebuildStreams() noexcept {
    const bool preserve_tangents = tangents.size() == vertices.size();
    const bool preserve_colors = colors.size() == vertices.size();
    auto old_tangents = std::move(tangents);
    auto old_colors = std::move(colors);
    positions.clear(); normals.clear(); uvs.clear(); bounds = {};
    positions.reserve(vertices.size()); normals.reserve(vertices.size()); uvs.reserve(vertices.size());
    for (const auto& vertex : vertices) {
        positions.push_back(vertex.position); normals.push_back(vertex.normal); uvs.push_back(vertex.uv);
        bounds.include(vertex.position);
    }
    if (preserve_tangents) tangents = std::move(old_tangents);
    if (preserve_colors) colors = std::move(old_colors);
}

void appendTransformed(MeshData& destination, const MeshData& source,
                       const MeshTransform& transform) {
    if (!source.valid() || (!destination.vertices.empty() && !destination.valid())) return;
    const std::size_t source_vertex_count = source.positions.empty() ? source.vertices.size() : source.positions.size();
    if (source_vertex_count == 0U || source_vertex_count > std::numeric_limits<std::uint32_t>::max() ||
        source.indices.size() > std::numeric_limits<std::uint32_t>::max() ||
        destination.vertices.size() > std::numeric_limits<std::uint32_t>::max() - source_vertex_count ||
        destination.indices.size() > std::numeric_limits<std::uint32_t>::max() - source.indices.size()) return;
    const auto base = static_cast<std::uint32_t>(destination.vertices.size());
    const auto index_base = static_cast<std::uint32_t>(destination.indices.size());
    const float c = std::cos(transform.rotation_y);
    const float s = std::sin(transform.rotation_y);
    const auto transform_direction = [&](foundation::Vec3 value) {
        const foundation::Vec3 scaled{
            transform.scale.x != 0.0F ? value.x / transform.scale.x : 0.0F,
            transform.scale.y != 0.0F ? value.y / transform.scale.y : 0.0F,
            transform.scale.z != 0.0F ? value.z / transform.scale.z : 0.0F};
        foundation::Vec3 rotated{scaled.x * c - scaled.z * s, scaled.y,
                                 scaled.x * s + scaled.z * c};
        const float length = std::sqrt(rotated.x * rotated.x + rotated.y * rotated.y +
                                       rotated.z * rotated.z);
        return length > 1.0e-8F ? rotated / length : foundation::Vec3{0.0F, 1.0F, 0.0F};
    };
    const std::size_t destination_vertex_count = destination.vertices.size();
    const bool source_has_tangents = !source.tangents.empty();
    const bool source_has_colors = !source.colors.empty();
    if (!destination.tangents.empty() && destination.tangents.size() != destination_vertex_count)
        destination.tangents.clear();
    if (!destination.colors.empty() && destination.colors.size() != destination_vertex_count)
        destination.colors.clear();
    if (source_has_tangents && destination.tangents.empty())
        destination.tangents.assign(destination_vertex_count, {1.0F, 0.0F, 0.0F, 1.0F});
    if (source_has_colors && destination.colors.empty())
        destination.colors.assign(destination_vertex_count, {1.0F, 1.0F, 1.0F, 1.0F});
    destination.vertices.reserve(destination.vertices.size() + source_vertex_count);
    destination.indices.reserve(destination.indices.size() + source.indices.size());

    for (std::size_t vertex_index = 0; vertex_index < source_vertex_count; ++vertex_index) {
        const MeshVertex vertex = source.vertices.empty()
            ? MeshVertex{source.positions[vertex_index], source.normals[vertex_index], source.uvs[vertex_index]}
            : source.vertices[vertex_index];
        const foundation::Vec3 p{
            vertex.position.x * transform.scale.x,
            vertex.position.y * transform.scale.y,
            vertex.position.z * transform.scale.z};
        destination.vertices.push_back({
            {transform.translation.x + p.x * c - p.z * s,
             transform.translation.y + p.y,
             transform.translation.z + p.x * s + p.z * c},
            transform_direction(vertex.normal), vertex.uv});
        if (source_has_tangents) {
            const auto tangent = source.tangents[vertex_index];
            const auto transformed = transform_direction({tangent.x, tangent.y, tangent.z});
            destination.tangents.push_back({transformed.x, transformed.y, transformed.z, tangent.w});
        } else if (!destination.tangents.empty()) {
            destination.tangents.push_back({1.0F, 0.0F, 0.0F, 1.0F});
        }
        if (source_has_colors)
            destination.colors.push_back(source.colors[vertex_index]);
        else if (!destination.colors.empty())
            destination.colors.push_back({1.0F, 1.0F, 1.0F, 1.0F});
    }
    const bool reflected = transform.scale.x * transform.scale.y * transform.scale.z < 0.0F;
    for (std::size_t index = 0; index < source.indices.size(); index += 3U) {
        destination.indices.push_back(base + source.indices[index]);
        destination.indices.push_back(base + source.indices[index + (reflected ? 2U : 1U)]);
        destination.indices.push_back(base + source.indices[index + (reflected ? 1U : 2U)]);
    }
    destination.rebuildStreams();
    if (source.submeshes.empty()) {
        destination.submeshes.push_back({index_base, static_cast<std::uint32_t>(source.indices.size()), 0U});
    } else {
        for (const auto range : source.submeshes)
            destination.submeshes.push_back({index_base + range.first_index, range.index_count,
                                              range.material_index});
    }
}

} // namespace genomes::geometry
