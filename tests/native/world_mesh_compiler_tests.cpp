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
    // A presentation compiler receives resolved plans; it must not silently
    // regenerate this site or accept a mixed-resolution artifact.
    assert(!world_render::WorldMeshCompiler::compile(plan, terrain.value(), {}));

    buildings::BuildingGenerationResult resolved{};
    constexpr foundation::StableId part_id = 0xB17U;
    resolved.plan.parts.push_back({.id = part_id,
                                   .kind = buildings::BuildingPartKind::Wall,
                                   .extent = {2.0F, 3.0F, 0.2F}});
    const std::array<buildings::BuildingGenerationResult, 1U> resolved_buildings{
        std::move(resolved)};
    const auto compiled = world_render::WorldMeshCompiler::compile(
        plan, terrain.value(), resolved_buildings);
    assert(compiled && compiled.value().mesh);
    const auto range = compiled.value().part_draw_ranges.find(part_id);
    assert(range != compiled.value().part_draw_ranges.end());
    assert(range->second.index_count == 36U);
    return 0;
}
