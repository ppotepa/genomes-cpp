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

genomes::world::BuildingSiteRequest validSite() {
    genomes::world::BuildingSiteRequest site{};
    site.request_id = 100;
    site.parcel_id = 200;
    site.seed = 300;
    site.preferred_position = {10.0F, 0.0F, -5.0F};
    site.preferred_footprint = {6.0F, 1.0F, 4.0F};
    site.clearance_m = 0.0F;
    site.buildable_polygon = {{6.0F, -8.0F}, {14.0F, -8.0F},
                              {14.0F, -2.0F}, {6.0F, -2.0F}};
    return site;
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

void siteRequiresCenteredRotationAlignedRectangle() {
    const auto valid = genomes::buildings::BuildingGenerator::generateSite(validSite());
    assert(valid);

    auto self_intersecting = validSite();
    self_intersecting.buildable_polygon = {{6.0F, -8.0F}, {14.0F, -2.0F},
                                            {14.0F, -8.0F}, {6.0F, -2.0F}};
    assert(!genomes::buildings::BuildingGenerator::generateSite(self_intersecting));

    auto asymmetric = validSite();
    asymmetric.buildable_polygon[0].x = 7.0F;
    assert(!genomes::buildings::BuildingGenerator::generateSite(asymmetric));

    auto five_points = validSite();
    five_points.buildable_polygon.push_back({10.0F, -5.0F});
    assert(!genomes::buildings::BuildingGenerator::generateSite(five_points));
}

} // namespace

int main() {
    roomsUsePositiveFullXyzExtents();
    narrowManyRoomPlanIsRejectedBeforeReservation();
    siteRequiresCenteredRotationAlignedRectangle();
    return 0;
}
