#include <genomes/buildings/BuildingModel.hpp>

#include <cassert>
#include <cstddef>
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

void partIdentityIsSemanticAndGeneratorVersioned() {
    const auto first = genomes::buildings::BuildingGenerator::generate(validSpec());
    assert(first);
    auto changed_seed = validSpec();
    changed_seed.seed = 8;
    const auto second = genomes::buildings::BuildingGenerator::generate(changed_seed);
    assert(second);
    assert(first.value().generator_version == genomes::buildings::BuildingGeneratorVersion);
    assert(first.value().compatible());
    assert(first.value().content_hash != second.value().content_hash);
    assert(first.value().parts.size() == second.value().parts.size());
    for (std::size_t index = 0; index < first.value().parts.size(); ++index) {
        const auto& left = first.value().parts[index];
        const auto& right = second.value().parts[index];
        assert(left.id == right.id);
        assert(left.key.building_id == 42);
        assert(left.key.floor == right.key.floor);
        assert(left.key.kind == right.key.kind);
        assert(left.key.role == right.key.role);
        assert(left.key.ordinal == right.key.ordinal);
    }
    genomes::buildings::BuildingPlan legacy{};
    legacy.generator_version = 1;
    assert(!legacy.compatible());
}

} // namespace

int main() {
    roomsUsePositiveFullXyzExtents();
    narrowManyRoomPlanIsRejectedBeforeReservation();
    siteRequiresCenteredRotationAlignedRectangle();
    partIdentityIsSemanticAndGeneratorVersioned();
    return 0;
}
