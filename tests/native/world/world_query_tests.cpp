#include <genomes/world_core/WorldQuery.hpp>

#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>

namespace {

float terrain(void* context, float x, float z) noexcept {
    const float base = *static_cast<const float*>(context);
    return base + x * 0.1F + z * 0.05F;
}

genomes::world_core::QueryRegion region(genomes::world_core::WorldId world_id,
                                        genomes::world_core::RegionCoord coordinate,
                                   bool resident,
                                   float* terrain_base) {
    const genomes::world_core::RegionId id = genomes::world_core::regionId(world_id, coordinate);
    genomes::world_core::QueryRegion result{};
    result.coordinate = coordinate;
    result.id = id;
    result.revision = 4U;
    result.resident = resident;
    result.terrain_sample = resident ? terrain : nullptr;
    result.terrain_context = resident ? terrain_base : nullptr;
    if (resident) {
        const float origin_x = static_cast<float>(coordinate.x) * 4.0F;
        const float origin_z = static_cast<float>(coordinate.z) * 4.0F;
        result.candidates.push_back({42U,
                                     {{origin_x + 0.25F, 0.0F, origin_z + 0.25F},
                                      {origin_x + 1.5F, 1.0F, origin_z + 1.5F}},
                                     genomes::world_core::QuerySourceKind::Static,
                                     id,
                                     4U});
        result.candidates.push_back({static_cast<genomes::foundation::StableId>(
                                         100U + coordinate.x * 10 + coordinate.z),
                                     {{origin_x + 2.0F, 0.0F, origin_z + 2.0F},
                                      {origin_x + 3.0F, 1.0F, origin_z + 3.0F}},
                                     genomes::world_core::QuerySourceKind::Dynamic,
                                     id,
                                     4U});
    }
    return result;
}

} // namespace

int main() {
    using namespace genomes::world_core;
    const WorldId world_id{19U};
    float terrain_base = 2.0F;
    std::vector<QueryRegion> regions;
    regions.push_back(region(world_id, {-1, -1, 0}, true, &terrain_base));
    regions.push_back(region(world_id, {0, -1, 0}, true, &terrain_base));
    regions.push_back(region(world_id, {-1, 0, 0}, true, &terrain_base));
    regions.push_back(region(world_id, {0, 0, 0}, false, nullptr));

    const auto created = WorldQuerySnapshot::create(world_id, {4.0}, std::move(regions));
    assert(created);
    WorldQuerySnapshot snapshot = std::move(created.value());
    const QueryAabbResult broadphase =
        snapshot.queryAabb({{-4.0F, -1.0F, -4.0F}, {4.0F, 2.0F, 4.0F}});
    assert(broadphase.completeness == QueryCompleteness::PartialUnloaded);
    assert(broadphase.candidates.size() == 4U);
    assert(broadphase.candidates.front().id == 42U);

    const QuerySegmentResult segment = snapshot.querySegment({-3.5F, 0.5F, -3.5F},
                                                              {3.5F, 0.5F, 3.5F});
    assert(segment.completeness == QueryCompleteness::PartialUnloaded);
    assert(!segment.hits.empty());
    assert(segment.hits.front().t <= segment.hits.back().t);

    const TerrainSampleResult loaded = snapshot.terrainHeight(-3.0F, -3.0F);
    assert(loaded.completeness == QueryCompleteness::Complete);
    assert(loaded.height > 1.0F);
    const TerrainSampleResult unloaded = snapshot.terrainHeight(1.0F, 1.0F);
    assert(unloaded.completeness == QueryCompleteness::PartialUnloaded);
    assert(snapshot.regionAt({-1.0F, 0.0F, -1.0F}).has_value());

    WorldQueryService service;
    assert(service.publish(std::move(snapshot)));
    assert(service.snapshot() != nullptr);
    return 0;
}
