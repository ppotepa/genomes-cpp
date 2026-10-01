#include <genomes/buildings/BuildingModel.hpp>

#include <cassert>
#include <cmath>

namespace {

genomes::buildings::BuildingSpec validSpec() {
    genomes::buildings::BuildingSpec spec{};
    spec.building_id = 42;
    spec.seed = 7;
    spec.footprint = {16.0F, 1.0F, 10.0F};
    spec.floors = 2;
    spec.floor_height = 3.0F;
    spec.wall_thickness = 0.2F;
    spec.rooms_per_floor = 2;
    return spec;
}

void roomsUsePositiveFullXyzExtents() {
    const auto generated = genomes::buildings::BuildingGenerator::generate(validSpec());
    assert(generated);
    assert(generated.value().rooms.size() == 4);
    const auto& room = generated.value().rooms.front();
    assert(std::abs(room.extent.x - 7.8F) < 1.0e-6F);
    assert(std::abs(room.extent.y - 2.8F) < 1.0e-6F);
    assert(std::abs(room.extent.z - 9.6F) < 1.0e-6F);
    assert(room.extent.x > 0.0F && room.extent.y > 0.0F && room.extent.z > 0.0F);
}

void narrowManyRoomPlanIsRejectedBeforeReservation() {
    auto invalid = validSpec();
    invalid.footprint = {2.0F, 1.0F, 10.0F};
    invalid.wall_thickness = 0.2F;
    invalid.rooms_per_floor = 16;
    assert(!invalid.valid());
    assert(!genomes::buildings::BuildingGenerator::generate(invalid));
}

} // namespace

int main() {
    roomsUsePositiveFullXyzExtents();
    narrowManyRoomPlanIsRejectedBeforeReservation();
    return 0;
}
