#include <genomes/terrain/HeightField.hpp>
#include <genomes/world_render/WorldMeshCompiler.hpp>

#include <array>
#include <cassert>
#include <utility>

int main() {
    using namespace genomes;
    terrain::TerrainSpec terrain_spec{};
    terrain_spec.world_id = world::WorldId{1U};
    terrain_spec.coordinates.region_size_m = 128.0;
    terrain_spec.samples_x = 2U;
    terrain_spec.samples_z = 2U;
    terrain_spec.cell_size_m = 8.0F;
    const auto terrain = terrain::HeightField::create(terrain_spec);
    assert(terrain);

    world::WorldPlan plan{};
    plan.map_size_m = 128U;
    plan.features.push_back({foundation::stable_id("world.mesh.test"),
                             world::WorldFeatureKind::TerrainPatch});
    plan.building_sites.emplace_back();
    plan.hydrology.river_points = {{-48.0F, 0.0F, 0.0F}, {48.0F, -1.0F, 0.0F}};
    plan.hydrology.rivers.push_back({foundation::stable_id("world.mesh.river"),
                                     0U, 2U, 20.0F, 1.0F, 48.0F});
    // A presentation compiler receives resolved plans; it must not silently
    // regenerate this site or accept a mixed-resolution artifact.
    const world::WorldArtifactRevision revision = world::artifactRevision(plan);
    assert(!world_render::WorldMeshCompiler::compile(plan, terrain.value(), {}, revision));
    assert(!world_render::WorldMeshCompiler::compile(
        plan, terrain.value(), {}, revision + 1U));

    buildings::BuildingGenerationResult resolved{};
    constexpr foundation::StableId part_id = 0xB17U;
    resolved.plan.parts.push_back({.id = part_id,
                                   .kind = buildings::BuildingPartKind::Wall,
                                   .extent = {2.0F, 3.0F, 0.2F}});
    const std::array<buildings::BuildingGenerationResult, 1U> resolved_buildings{
        std::move(resolved)};
    terrain::TerrainMesh resolved_water{};
    resolved_water.vertices = {
        {{-10.0F, 0.25F, -1.0F}, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F}},
        {{10.0F, 0.25F, -1.0F}, {0.0F, 1.0F, 0.0F}, {1.0F, 0.0F}},
        {{10.0F, 0.25F, 1.0F}, {0.0F, 1.0F, 0.0F}, {1.0F, 1.0F}},
        {{-10.0F, 0.25F, 1.0F}, {0.0F, 1.0F, 0.0F}, {0.0F, 1.0F}},
    };
    resolved_water.indices = {0U, 1U, 2U, 0U, 2U, 3U};
    const auto compiled = world_render::WorldMeshCompiler::compile(
        plan, terrain.value(), resolved_buildings, revision, &resolved_water);
    assert(compiled && compiled.value().mesh);
    assert(compiled.value().source_revision == revision);
    assert(compiled.value().mesh->revision == revision);
    assert(compiled.value().mesh->materials.size() == 2U);
    assert(compiled.value().mesh->material_groups.size() == 2U);
    const auto& opaque = compiled.value().mesh->material_groups[0U];
    const auto& water = compiled.value().mesh->material_groups[1U];
    assert(opaque.material_index == 0U);
    assert(water.material_index == 1U);
    assert(opaque.first_index == 0U);
    assert(water.first_index == opaque.index_count);
    assert(water.index_count >= 6U);
    assert(water.index_count % 6U == 0U);
    assert(compiled.value().mesh->materials[1U].alpha_mode ==
           render::MaterialAlphaMode::Opaque);
    assert(compiled.value().mesh->materials[1U].double_sided);
    assert(compiled.value().water_mesh != nullptr);
    assert(compiled.value().water_mesh->vertices.size() == resolved_water.vertices.size());
    assert(compiled.value().water_mesh->indices == resolved_water.indices);
    const auto range = compiled.value().part_draw_ranges.find(part_id);
    assert(range != compiled.value().part_draw_ranges.end());
    assert(range->second.index_count == 36U);
    return 0;
}
