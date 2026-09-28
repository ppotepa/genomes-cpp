#pragma once

#include <genomes/proc/SeedPath.hpp>
#include <genomes/world/WorldPosition.hpp>

#include <cmath>
#include <cstdint>

namespace genomes::terrain {

struct TerrainSpec final {
    world::WorldId world_id{};
    world::RegionCoord region{};
    world::WorldCoordinateConfig coordinates{};
    proc::SeedPath seed_path{};
    std::uint32_t samples_x{0};
    std::uint32_t samples_z{0};
    float cell_size_m{1.0F};
    double origin_offset_x{0.0};
    double origin_offset_z{0.0};

    [[nodiscard]] bool valid() const noexcept {
        constexpr std::uint64_t max_samples = 4'000'000;
        return world_id.isValid() && samples_x >= 2 && samples_z >= 2 &&
               static_cast<std::uint64_t>(samples_x) * samples_z <= max_samples &&
               cell_size_m > 0.0F && std::isfinite(origin_offset_x) &&
               std::isfinite(origin_offset_z) && coordinates.valid();
    }
};

} // namespace genomes::terrain
