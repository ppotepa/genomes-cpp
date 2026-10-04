#pragma once

#include <cmath>
#include <cstdint>

namespace genomes::world {

enum class TerrainPreset : std::uint8_t {
    Plains,
    RollingHills,
    Highlands,
    RiverValley,
};

enum class TributaryDensity : std::uint8_t {
    None,
    Low,
    Medium,
};

struct TerrainGenerationConfig final {
    TerrainPreset preset{TerrainPreset::RollingHills};
    // This is the authoritative terrain/hydrology sampling interval.  Navigation
    // may use a coarser grid, but it samples this field rather than maintaining
    // a second incompatible landscape.
    std::uint8_t sample_spacing_m{8U};
    float elevation_range_m{70.0F};
    float landform_scale_m{600.0F};
    float roughness{0.35F};

    [[nodiscard]] bool valid() const noexcept {
        const bool supported_spacing = sample_spacing_m == 1U || sample_spacing_m == 2U ||
                                       sample_spacing_m == 4U || sample_spacing_m == 8U ||
                                       sample_spacing_m == 16U;
        return supported_spacing && std::isfinite(elevation_range_m) && elevation_range_m >= 0.0F &&
               elevation_range_m <= 600.0F && std::isfinite(landform_scale_m) &&
               landform_scale_m >= 32.0F && landform_scale_m <= 4096.0F &&
               std::isfinite(roughness) && roughness >= 0.0F && roughness <= 1.0F;
    }
};

struct HydrologyGenerationConfig final {
    std::uint8_t main_river_min{0U};
    std::uint8_t main_river_max{1U};
    TributaryDensity tributary_density{TributaryDensity::Low};
    float stream_width_min_m{1.0F};
    float stream_width_max_m{4.0F};
    float river_width_min_m{5.0F};
    float river_width_max_m{14.0F};
    float depth_min_m{0.3F};
    float depth_max_m{2.0F};
    float meander_strength{0.4F};
    float valley_width_min_m{20.0F};
    float valley_width_max_m{80.0F};

    [[nodiscard]] bool valid() const noexcept {
        const auto finite_range = [](float minimum, float maximum, float floor) {
            return std::isfinite(minimum) && std::isfinite(maximum) &&
                   minimum >= floor && maximum >= minimum;
        };
        return main_river_min <= main_river_max && main_river_max <= 4U &&
               finite_range(stream_width_min_m, stream_width_max_m, 0.25F) &&
               finite_range(river_width_min_m, river_width_max_m, 1.0F) &&
               finite_range(depth_min_m, depth_max_m, 0.05F) &&
               finite_range(valley_width_min_m, valley_width_max_m, 2.0F) &&
               std::isfinite(meander_strength) && meander_strength >= 0.0F &&
               meander_strength <= 1.0F;
    }
};

} // namespace genomes::world
