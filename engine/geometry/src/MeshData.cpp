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
    if (source.vertices.empty()) return;
    const auto base = static_cast<std::uint32_t>(destination.vertices.size());
    const float c = std::cos(transform.rotation_y);
    const float s = std::sin(transform.rotation_y);
    destination.vertices.reserve(destination.vertices.size() + source.vertices.size());
    destination.indices.reserve(destination.indices.size() + source.indices.size());

    for (const auto& vertex : source.vertices) {
        const foundation::Vec3 p{
            vertex.position.x * transform.scale.x,
            vertex.position.y * transform.scale.y,
            vertex.position.z * transform.scale.z};
        const foundation::Vec3 n0{
            transform.scale.x != 0.0F ? vertex.normal.x / transform.scale.x : 0.0F,
            transform.scale.y != 0.0F ? vertex.normal.y / transform.scale.y : 0.0F,
            transform.scale.z != 0.0F ? vertex.normal.z / transform.scale.z : 0.0F};
        foundation::Vec3 n{n0.x * c - n0.z * s, n0.y, n0.x * s + n0.z * c};
        const float length = std::sqrt(n.x*n.x + n.y*n.y + n.z*n.z);
        if (length > 1.0e-8F) {
            n.x /= length; n.y /= length; n.z /= length;
        } else {
            n = {0.0F, 1.0F, 0.0F};
        }
        destination.vertices.push_back({
            {transform.translation.x + p.x * c - p.z * s,
             transform.translation.y + p.y,
             transform.translation.z + p.x * s + p.z * c},
            n, vertex.uv});
    }
    for (const auto index : source.indices) destination.indices.push_back(base + index);
    destination.rebuildStreams();
    if (destination.submeshes.empty()) destination.submeshes.push_back({0U, static_cast<std::uint32_t>(destination.indices.size()), 0U});
}

} // namespace genomes::geometry
