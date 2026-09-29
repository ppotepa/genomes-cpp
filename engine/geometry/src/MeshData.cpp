#include <genomes/geometry/MeshData.hpp>

#include <cmath>
#include <limits>

namespace genomes::geometry {

namespace {
[[nodiscard]] bool finite(foundation::Vec3 v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
[[nodiscard]] bool finite(foundation::Vec2 v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y);
}
}

bool MeshData::valid() const noexcept {
    if (vertices.empty() || indices.empty() || indices.size() % 3U != 0U) return false;
    for (const auto& vertex : vertices)
        if (!finite(vertex.position) || !finite(vertex.normal) || !finite(vertex.uv)) return false;
    for (const auto index : indices)
        if (index >= vertices.size()) return false;
    return true;
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
}

} // namespace genomes::geometry
