#include "TestSupport.hpp"

#include <genomes/geometry/IndexOrderOptimizer.hpp>
#include <genomes/render/SkinnedDeformer.hpp>
#include <genomes/render/SkinnedMeshOptimizer.hpp>

#include <iostream>
#include <limits>

using namespace genomes;
using upgrade_test::check;

namespace {

render::SkinnedMeshPrototype fixture() {
    render::SkinnedMeshPrototype mesh;
    mesh.mesh_id = 11U;
    mesh.revision = 17U;
    for (std::uint32_t i = 0; i < 7U; ++i) {
        render::SkinnedMeshVertex vertex;
        const float f = static_cast<float>(i);
        vertex.position = {static_cast<float>(i % 3U), static_cast<float>(i / 3U), 0};
        vertex.normal = {0, 0, 1};
        vertex.uv = {f * 0.1F, f * 0.2F};
        vertex.color = {0.1F * f, 0.5F, 0.25F, 1};
        vertex.bone_indices = {0U, 1U, 0U, 0U};
        vertex.bone_weights = {0.25F, 0.75F, 0, 0};
        mesh.vertices.push_back(vertex);
    }
    // Vertex 6 is intentionally unreferenced; tags may still address it.
    mesh.indices = {4, 1, 3, 1, 2, 3, 4, 5, 1, 5, 0, 1, 1, 5, 3, 5, 2, 3};
    render::MaterialDescriptor opaque;
    opaque.material_id = 1U;
    render::MaterialDescriptor blend = opaque;
    blend.material_id = 2U;
    blend.alpha_mode = render::MaterialAlphaMode::Blend;
    mesh.materials = {opaque, blend};
    mesh.material_groups = {{0U, 12U, 0U}, {12U, 6U, 1U}};
    mesh.morph_target_count = 4U;
    mesh.morph_weights = {0.1F, 0.2F, 0.3F, 0.4F};
    for (std::size_t m = 0; m < mesh.morphs.size(); ++m) {
        for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
            const float d = static_cast<float>(m * 7U + i + 1U) * 0.001F;
            mesh.morphs[m].position_deltas.push_back({d, -d, d});
            mesh.morphs[m].normal_deltas.push_back({0, d, 0});
        }
    }
    mesh.conservative_bounds_radius = 10.0F;
    return mesh;
}

void checks() {
    check(geometry::indexOptimizerAvailable() == (GENOMES_EXPECT_MESHOPTIMIZER != 0),
          "CMake option and optimizer implementation disagree");
    check((geometry::indexOptimizerFingerprint() != 0U) == geometry::indexOptimizerAvailable(),
          "optimizer fingerprint does not distinguish the disabled path");
    const auto source = fixture();
    auto prepared = source;
    const auto report = render::optimizeSkinnedDrawOrder(prepared);
    check(report, "valid fixture optimization failed");
    check(report.value().triangle_count == 6U, "incorrect triangle metric");
    check(report.value().optimized_ranges == (geometry::indexOptimizerAvailable() ? 1U : 0U),
          "opaque range did not follow the selected optimization mode");
    upgrade_test::checkVertexStreams(source, prepared);
    upgrade_test::checkGroups(source, prepared);
    check(prepared.mesh_id == source.mesh_id && prepared.revision == source.revision,
          "low-level optimizer must not assign resource identities");
    check(std::equal(source.indices.begin() + 12, source.indices.end(),
                     prepared.indices.begin() + 12), "blend order changed");
    auto repeated = source;
    check(render::optimizeSkinnedDrawOrder(repeated), "repeat failed");
    check(repeated.indices == prepared.indices, "optimizer is not deterministic");

    const std::array<float, 16> identity{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    const std::array<float, 16> rotated{0,1,0,0, -1,0,0,0, 0,0,1,0, 2,3,1,1};
    const std::array palette{identity, rotated};
    const auto before = render::deformSkinnedCPU(source, palette, source.morph_weights);
    const auto after = render::deformSkinnedCPU(prepared, palette, prepared.morph_weights);
    for (std::size_t i = 0; i < before.vertices.size(); ++i) {
        check(upgrade_test::equal(before.vertices[i].position, after.vertices[i].position),
              "skinning plus morph positions changed");
        check(upgrade_test::equal(before.vertices[i].normal, after.vertices[i].normal),
              "skinning plus morph normals changed");
    }
    auto alpha = source;
    alpha.vertices[1].color.a = 0.5F;
    check(render::optimizeSkinnedDrawOrder(alpha), "alpha mesh rejected");
    check(alpha.indices == source.indices, "vertex alpha order changed");
    auto tinted = source;
    tinted.materials[0].instance_tint = true;
    check(render::optimizeSkinnedDrawOrder(tinted), "tint-enabled mesh rejected");
    check(tinted.indices == source.indices, "unknown per-instance alpha was reordered");
    auto unknown = source;
    unknown.materials.clear();
    unknown.material_groups.clear();
    check(render::optimizeSkinnedDrawOrder(unknown), "unknown material mesh rejected");
    check(unknown.indices == source.indices, "unspecified material treated as opaque");

    auto invalid = source;
    invalid.material_groups[1].first_index = 3U;
    check(!render::optimizeSkinnedDrawOrder(invalid), "overlapping groups accepted");
    check(invalid.indices == source.indices, "failed preparation wrote partial indices");
    upgrade_test::checkVertexStreams(source, invalid);
    invalid = source;
    invalid.indices[0] = 99U;
    const auto bad_indices = invalid.indices;
    check(!render::optimizeSkinnedDrawOrder(invalid), "invalid vertex index accepted");
    check(invalid.indices == bad_indices, "failed bounds check changed source");
    invalid = source;
    invalid.materials[0].roughness = std::numeric_limits<float>::quiet_NaN();
    check(!render::optimizeSkinnedDrawOrder(invalid), "invalid material accepted");
    check(invalid.indices == source.indices, "invalid material mutated indices");
    check(geometry::optimizeIndexOrder({}, 0U, {}), "empty input should be an identity");
    check(!geometry::optimizeIndexOrder(source.indices, source.vertices.size(), {}),
          "non-empty stream accepted without explicit order policy");
}

} // namespace

int main() {
    try {
        checks();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
