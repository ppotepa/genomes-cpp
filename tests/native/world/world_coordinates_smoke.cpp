#include <genomes/world/WorldPosition.hpp>

#include <cassert>
#include <cmath>

int main() {
    const genomes::world::WorldCoordinateConfig config{100.0};
    const auto negative = genomes::world::regionCoordFor({-0.001, 0.0, -100.0}, config);
    assert(negative.x == -1);
    assert(negative.z == -1);
    assert((genomes::world::regionCoordFor({100.0, 0.0, 100.0}, config) ==
            genomes::world::RegionCoord{1, 1, 0}));

    const genomes::world::RegionCoord coord{-4, 7, 0};
    const genomes::world::WorldPosition global{-399.25, 12.5, 712.75};
    const auto local = genomes::world::toLocal(global, coord, config);
    assert(std::abs(local.x - 0.75F) < 1e-5F);
    assert(std::abs(local.y - 12.5F) < 1e-5F);
    assert(std::abs(local.z - 12.75F) < 1e-5F);
    const auto round_trip = genomes::world::toGlobal(local, coord, config);
    assert(std::abs(round_trip.x - global.x) < 1e-4);
    assert(std::abs(round_trip.y - global.y) < 1e-4);
    assert(std::abs(round_trip.z - global.z) < 1e-4);

    const genomes::world::WorldId world_id(42);
    const auto id_a = genomes::world::regionId(world_id, coord);
    const auto id_b = genomes::world::regionId(world_id, coord);
    const auto id_c = genomes::world::regionId(world_id, {coord.x + 1, coord.z, coord.layer});
    assert(id_a.isValid());
    assert(id_a == id_b);
    assert(id_a != id_c);
    return 0;
}
