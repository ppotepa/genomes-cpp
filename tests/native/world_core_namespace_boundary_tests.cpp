#include <genomes/world/WorldPosition.hpp>
#include <genomes/world_core/WorldPosition.hpp>

#include <cassert>
#include <type_traits>

int main() {
    static_assert(std::is_same_v<genomes::world::WorldId, genomes::world_core::WorldId>);
    static_assert(std::is_same_v<genomes::world::RegionCoord, genomes::world_core::RegionCoord>);
    static_assert(
        std::is_same_v<genomes::world::WorldCoordinateConfig,
                       genomes::world_core::WorldCoordinateConfig>);

    const genomes::world_core::WorldCoordinateConfig config{128.0};
    const genomes::world_core::WorldPosition position{129.0, 2.0, -1.0};
    const auto coordinate = genomes::world_core::regionCoordFor(position, config);
    const genomes::world::RegionCoord expected{1, -1, 0};
    assert(coordinate == expected);
    assert(genomes::world::regionCoordFor(position, config) == coordinate);
    return 0;
}
