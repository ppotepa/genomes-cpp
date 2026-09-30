#include <genomes/geometry/MeshValidation.hpp>

#include <cmath>
#include <vector>

namespace genomes::geometry {

namespace {
[[nodiscard]] foundation::Vec3 subtract(foundation::Vec3 a, foundation::Vec3 b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
[[nodiscard]] foundation::Vec3 cross(foundation::Vec3 a, foundation::Vec3 b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}
[[nodiscard]] float dot(foundation::Vec3 a, foundation::Vec3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
[[nodiscard]] bool finite(foundation::Vec3 v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
} // namespace

MeshValidationReport validateMesh(std::span<const foundation::Vec3> positions,
                                  std::span<const foundation::Vec3> normals,
                                  std::span<const std::uint32_t> indices,
                                  float area_epsilon,
                                  float winding_epsilon) noexcept {
    MeshValidationReport report{};
    for (const auto position : positions) {
        if (!finite(position)) {
            ++report.nonfinite_vertex_count;
        }
    }
    for (const auto normal : normals) {
        if (!finite(normal)) {
            ++report.nonfinite_vertex_count;
        }
    }
    report.triangle_count = indices.size() / 3U;
    if (indices.size() % 3U != 0U) {
        ++report.invalid_index_count;
    }
    for (std::size_t offset = 0U; offset + 2U < indices.size(); offset += 3U) {
        const auto i0 = indices[offset];
        const auto i1 = indices[offset + 1U];
        const auto i2 = indices[offset + 2U];
        if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size()) {
            ++report.invalid_index_count;
            continue;
        }
        const auto normal = cross(subtract(positions[i1], positions[i0]),
                                  subtract(positions[i2], positions[i0]));
        const float area2 = dot(normal, normal);
        if (!std::isfinite(area2) || area2 <= area_epsilon * area_epsilon) {
            ++report.degenerate_triangle_count;
        }
        if (normals.size() == positions.size() && finite(normals[i0]) &&
            finite(normals[i1]) && finite(normals[i2])) {
            const foundation::Vec3 average{
                (normals[i0].x + normals[i1].x + normals[i2].x) / 3.0F,
                (normals[i0].y + normals[i1].y + normals[i2].y) / 3.0F,
                (normals[i0].z + normals[i1].z + normals[i2].z) / 3.0F};
            if (dot(normal, average) < -winding_epsilon) {
                ++report.winding_mismatch_count;
            }
        }
    }
    return report;
}

MeshValidationReport validateMesh(const MeshData& mesh,float area_epsilon,float winding_epsilon) noexcept {
    std::vector<foundation::Vec3> legacy_positions;
    std::vector<foundation::Vec3> legacy_normals;
    std::span<const foundation::Vec3> positions(mesh.positions);
    std::span<const foundation::Vec3> normals(mesh.normals);
    if(positions.empty()&&!mesh.vertices.empty()){
        legacy_positions.reserve(mesh.vertices.size()); legacy_normals.reserve(mesh.vertices.size());
        for(const auto& vertex:mesh.vertices){legacy_positions.push_back(vertex.position);legacy_normals.push_back(vertex.normal);}
        positions=legacy_positions; normals=legacy_normals;
    }
    auto report=validateMesh(positions,normals,mesh.indices,area_epsilon,winding_epsilon);
    if(!mesh.positions.empty()&&(mesh.normals.size()!=mesh.positions.size()||mesh.uvs.size()!=mesh.positions.size()||(!mesh.tangents.empty()&&mesh.tangents.size()!=mesh.positions.size())||(!mesh.colors.empty()&&mesh.colors.size()!=mesh.positions.size())))++report.stream_mismatch_count;
    for(const auto& range:mesh.submeshes)if(range.first_index%3U!=0U||range.index_count%3U!=0U||static_cast<std::size_t>(range.first_index)+range.index_count>mesh.indices.size())++report.invalid_submesh_count;
    if(!mesh.bounds.empty){math::Aabb recomputed;for(const auto position:positions)recomputed.include(position);const float e=1.0e-4F;if(recomputed.empty||std::fabs(recomputed.min.x-mesh.bounds.min.x)>e||std::fabs(recomputed.min.y-mesh.bounds.min.y)>e||std::fabs(recomputed.min.z-mesh.bounds.min.z)>e||std::fabs(recomputed.max.x-mesh.bounds.max.x)>e||std::fabs(recomputed.max.y-mesh.bounds.max.y)>e||std::fabs(recomputed.max.z-mesh.bounds.max.z)>e)++report.bounds_mismatch_count;}
    return report;
}

} // namespace genomes::geometry
