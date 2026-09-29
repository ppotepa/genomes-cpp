#pragma once

#include <genomes/render/RenderTypes.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace upgrade_test {

template<class Condition> void check(const Condition& condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

inline bool equal(genomes::foundation::Vec3 a, genomes::foundation::Vec3 b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline bool equal(const genomes::render::SkinnedMeshVertex& a,
                  const genomes::render::SkinnedMeshVertex& b) {
    return equal(a.position, b.position) && equal(a.normal, b.normal) &&
        a.uv.x == b.uv.x && a.uv.y == b.uv.y &&
        a.color.r == b.color.r && a.color.g == b.color.g &&
        a.color.b == b.color.b && a.color.a == b.color.a &&
        a.bone_indices == b.bone_indices && a.bone_weights == b.bone_weights &&
        a.material_region == b.material_region;
}

inline void checkVertexStreams(const genomes::render::SkinnedMeshPrototype& a,
                               const genomes::render::SkinnedMeshPrototype& b) {
    check(a.vertices.size() == b.vertices.size(), "vertex count changed");
    for (std::size_t i = 0; i < a.vertices.size(); ++i)
        check(equal(a.vertices[i], b.vertices[i]), "vertex ID or attribute changed");
    check(a.morph_target_count == b.morph_target_count, "morph count changed");
    check(a.morph_weights == b.morph_weights, "default morph weights changed");
    for (std::size_t m = 0; m < a.morphs.size(); ++m) {
        const auto& x = a.morphs[m];
        const auto& y = b.morphs[m];
        check(x.position_deltas.size() == y.position_deltas.size(), "morph positions resized");
        check(x.normal_deltas.size() == y.normal_deltas.size(), "morph normals resized");
        for (std::size_t i = 0; i < x.position_deltas.size(); ++i)
            check(equal(x.position_deltas[i], y.position_deltas[i]), "morph position changed");
        for (std::size_t i = 0; i < x.normal_deltas.size(); ++i)
            check(equal(x.normal_deltas[i], y.normal_deltas[i]), "morph normal changed");
    }
    check(equal(a.conservative_bounds_center, b.conservative_bounds_center) &&
          a.conservative_bounds_radius == b.conservative_bounds_radius, "bounds changed");
}

inline auto triangles(const std::vector<std::uint32_t>& indices,
                       std::size_t start, std::size_t count) {
    std::vector<std::array<std::uint32_t, 3>> result;
    check(count % 3U == 0U && start <= indices.size() &&
          count <= indices.size() - start, "bad test triangle range");
    for (std::size_t i = start; i < start + count; i += 3U)
        result.push_back({indices[i], indices[i + 1U], indices[i + 2U]});
    std::sort(result.begin(), result.end());
    return result;
}

inline void checkGroups(const genomes::render::SkinnedMeshPrototype& a,
                        const genomes::render::SkinnedMeshPrototype& b) {
    check(a.indices.size() == b.indices.size(), "triangle count changed");
    check(a.material_groups.size() == b.material_groups.size(), "material groups changed");
    for (std::size_t i = 0; i < a.material_groups.size(); ++i) {
        const auto& x = a.material_groups[i];
        const auto& y = b.material_groups[i];
        check(x.first_index == y.first_index && x.index_count == y.index_count &&
              x.material_index == y.material_index, "material boundaries changed");
        check(triangles(a.indices, x.first_index, x.index_count) ==
              triangles(b.indices, y.first_index, y.index_count),
              "triangle membership or winding changed");
    }
}

} // namespace upgrade_test
