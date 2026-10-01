#include <genomes/terrain/HeightField.hpp>
#include <genomes/world_render/WorldMeshCompiler.hpp>

#include <cassert>

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
    return 0;
}
