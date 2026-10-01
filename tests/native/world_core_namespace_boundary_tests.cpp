#include <genomes/world/BuildingSite.hpp>
#include <genomes/world/WorldQuery.hpp>
#include <genomes/world/WorldQuerySnapshot.hpp>
#include <genomes/world/WorldSave.hpp>
#include <genomes/world/WorldPosition.hpp>
#include <genomes/world_core/BuildingSite.hpp>
#include <genomes/world_core/WorldQuery.hpp>
#include <genomes/world_core/WorldQuerySnapshot.hpp>
#include <genomes/world_core/WorldSave.hpp>
#include <genomes/world_core/WorldPosition.hpp>

#include <cassert>
#include <type_traits>

int main() {
    static_assert(std::is_same_v<genomes::world::WorldId, genomes::world_core::WorldId>);
    static_assert(std::is_same_v<genomes::world::RegionCoord, genomes::world_core::RegionCoord>);
    static_assert(
        std::is_same_v<genomes::world::WorldCoordinateConfig,
                       genomes::world_core::WorldCoordinateConfig>);
    static_assert(std::is_same_v<genomes::world::BuildingSiteRequest,
                                 genomes::world_core::BuildingSiteRequest>);
    static_assert(std::is_same_v<genomes::world::BuildingSiteResolution,
                                 genomes::world_core::BuildingSiteResolution>);
    static_assert(std::is_same_v<genomes::world::WorldQuerySnapshot,
                                 genomes::world_core::WorldQuerySnapshot>);
    static_assert(std::is_same_v<genomes::world::WorldQueryService,
                                 genomes::world_core::WorldQueryService>);
    static_assert(std::is_same_v<genomes::world::WorldSaveModel,
                                 genomes::world_core::WorldSaveModel>);
    static_assert(std::is_same_v<genomes::world::WorldSaveCodec,
                                 genomes::world_core::WorldSaveCodec>);

    const genomes::world_core::WorldCoordinateConfig config{128.0};
    const genomes::world_core::WorldPosition position{129.0, 2.0, -1.0};
    const auto coordinate = genomes::world_core::regionCoordFor(position, config);
    const genomes::world::RegionCoord expected{1, -1, 0};
    assert(coordinate == expected);
    assert(genomes::world::regionCoordFor(position, config) == coordinate);

    genomes::world_core::BuildingSiteRequest request{};
    request.request_id = 1;
    request.parcel_id = 2;
    request.seed = 3;
    request.buildable_polygon = {{-6.0F, -5.0F}, {6.0F, -5.0F},
                                 {6.0F, 5.0F}, {-6.0F, 5.0F}};
    request.preferred_footprint = {12.0F, 1.0F, 10.0F};
    request.floors_min = 1U;
    request.floors_max = 3U;
    request.access_width = 1.2F;
    assert(request.valid());
    const auto translated = request.translated({10.0F, 2.0F, -4.0F});
    assert(translated.buildable_polygon.front().x == 4.0F);
    assert(translated.buildable_polygon.front().y == -9.0F);
    return 0;
}
