#include <genomes/hydrology/HydrologyArtifact.hpp>
#include <genomes/proc/SeedPath.hpp>
#include <genomes/terrain/TerrainGenerator.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>

int main() {
    using namespace genomes;

    terrain::TerrainSpec terrain_spec{};
    terrain_spec.world_id = world::WorldId(7U);
    terrain_spec.seed_path = proc::SeedPath(0xC0FFEEU).child("terrain", 0U);
    terrain_spec.samples_x = 65U;
    terrain_spec.samples_z = 65U;
    terrain_spec.cell_size_m = 8.0F;
    terrain_spec.origin_offset_x = -256.0;
    terrain_spec.origin_offset_z = -256.0;
    terrain_spec.coordinates.region_size_m = 512.0;
    terrain_spec.generation.preset = world::TerrainPreset::RollingHills;

    auto terrain_result = terrain::TerrainGenerator::generate(terrain_spec);
    assert(terrain_result);
    auto field = std::move(terrain_result.value());
    float minimum_height = std::numeric_limits<float>::max();
    float maximum_height = std::numeric_limits<float>::lowest();
    for (const float height : field.samples()) {
        minimum_height = std::min(minimum_height, height);
        maximum_height = std::max(maximum_height, height);
    }
    assert(maximum_height - minimum_height > 4.0F);

    hydrology::HydrologySpec hydrology_spec{};
    hydrology_spec.seed = 0xC0FFEEU;
    hydrology_spec.map_size_m = 512U;
    hydrology_spec.cells_x = 64U;
    hydrology_spec.cells_z = 64U;
    hydrology_spec.cell_size_m = 8.0F;
    hydrology_spec.mode = hydrology::HydrologyMode::Forced;
    hydrology_spec.main_river_min = 1U;
    hydrology_spec.main_river_max = 1U;
    hydrology_spec.tributary_density = hydrology::TributaryDensity::Low;
    hydrology_spec.river_width_min_m = 20.0F;
    hydrology_spec.river_width_max_m = 20.0F;
    hydrology_spec.valley_width_min_m = 60.0F;
    hydrology_spec.valley_width_max_m = 60.0F;

    const hydrology::HydrologyTerrainView view{
        field.width(), field.height(), field.cellSize(), static_cast<float>(field.originX()),
        static_cast<float>(field.originZ()), field.samples()};
    const auto first = hydrology::HydrologyGenerator::generate(hydrology_spec, view);
    const auto second = hydrology::HydrologyGenerator::generate(hydrology_spec, view);
    assert(first && second);
    assert(first.value().valid());
    assert(first.value().content_hash == second.value().content_hash);
    assert(first.value().river_points.size() == second.value().river_points.size());
    for (std::size_t index = 0U; index < first.value().river_points.size(); ++index) {
        assert(first.value().river_points[index].x == second.value().river_points[index].x);
        assert(first.value().river_points[index].y == second.value().river_points[index].y);
        assert(first.value().river_points[index].z == second.value().river_points[index].z);
    }
    assert(!first.value().rivers.empty());

    const auto& artifact = first.value();
    bool has_connected_tributary = false;
    bool has_boundary_outlet = false;
    for (const hydrology::RiverPath& river : artifact.rivers) {
        const std::size_t end = river.point_offset + river.point_count;
        assert(end <= artifact.river_points.size());
        for (std::size_t index = river.point_offset + 1U; index < end; ++index)
        {
            const auto& from = artifact.river_points[index - 1U];
            const auto& to = artifact.river_points[index];
            const float segment_length = std::hypot(to.x - from.x, to.z - from.z);
            const float drop = from.y - to.y;
            assert(drop > 0.0F);
            assert(drop <= std::max(river.depth_m * 1.5F, segment_length * 0.045F) +
                               0.001F);
        }
        if (river.parent_river_id != 0U) {
            const auto parent = std::find_if(artifact.rivers.begin(), artifact.rivers.end(),
                [&river](const hydrology::RiverPath& candidate) {
                    return candidate.id == river.parent_river_id;
                });
            assert(parent != artifact.rivers.end());
            const auto endpoint = artifact.river_points[river.point_offset + river.point_count - 1U];
            const auto parent_begin = artifact.river_points.begin() + parent->point_offset;
            const auto parent_end = parent_begin + parent->point_count;
            assert(std::find_if(parent_begin, parent_end, [&endpoint](foundation::Vec3 point) {
                return point.x == endpoint.x && point.y == endpoint.y && point.z == endpoint.z;
            }) != parent_end);
            has_connected_tributary = true;
        } else {
            const auto outlet = artifact.river_points[river.point_offset + river.point_count - 1U];
            const float half_extent = static_cast<float>(hydrology_spec.map_size_m) * 0.5F;
            const float boundary_distance = std::min({std::abs(outlet.x + half_extent),
                                                       std::abs(outlet.x - half_extent),
                                                       std::abs(outlet.z + half_extent),
                                                       std::abs(outlet.z - half_extent)});
            assert(boundary_distance <= hydrology_spec.cell_size_m * 1.1F);
            has_boundary_outlet = true;
        }
    }
    assert(has_connected_tributary);
    assert(has_boundary_outlet);
    const auto water_cells = std::count(artifact.water_mask.begin(), artifact.water_mask.end(), 1U);
    const auto flood_cells = std::count(artifact.flood_mask.begin(), artifact.flood_mask.end(), 1U);
    assert(water_cells > 0);
    assert(flood_cells > water_cells);

    const hydrology::RiverPath& river = artifact.rivers.front();
    const auto center = artifact.river_points[river.point_offset + river.point_count / 2U];
    const float before = field.sampleBilinear(center.x, center.z);
    const terrain::TerrainChannel channel{
        river.id,
        std::span<const foundation::Vec3>(artifact.river_points)
            .subspan(river.point_offset, river.point_count),
        river.width_m, river.depth_m, river.valley_width_m};
    const auto carved = terrain::TerrainGenerator::carveChannels(
        field, std::span<const terrain::TerrainChannel>(&channel, 1U));
    assert(carved);
    const float after = field.sampleBilinear(center.x, center.z);
    assert(after < before);
    assert(after <= center.y - river.depth_m + 0.1F);
    const float maximum_cut = std::max(
        river.depth_m * 2.5F,
        std::min(river.valley_width_m * 0.11F, river.depth_m * 4.0F + 2.0F));
    assert(after >= before - maximum_cut - 0.1F);

    const auto water = artifact.sampleWater(center.x, center.z);
    assert(water.has_water);
    assert(water.depth_m == river.depth_m);
    assert(water.surface_y > after);
    return 0;
}
