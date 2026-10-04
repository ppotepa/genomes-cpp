#pragma once

#include <genomes/proc/Seed.hpp>

#include <cmath>
#include <cstdint>
#include <span>

namespace genomes::hydrology {

enum class HydrologyMode : std::uint8_t {
    Off,
    SeededOptional,
    Forced,
};

enum class TributaryDensity : std::uint8_t {
    None,
    Low,
    Medium,
};

struct HydrologyTerrainView final {
    std::uint32_t samples_x{0U};
    std::uint32_t samples_z{0U};
    float cell_size_m{1.0F};
    float origin_x{0.0F};
    float origin_z{0.0F};
    std::span<const float> heights;

    [[nodiscard]] bool valid() const noexcept {
        return samples_x >= 2U && samples_z >= 2U && std::isfinite(cell_size_m) &&
               cell_size_m > 0.0F && heights.size() ==
                   static_cast<std::size_t>(samples_x) * samples_z;
    }
};

struct HydrologySpec final {
    proc::Seed seed{0U};
    std::uint32_t map_size_m{0U};
    std::uint32_t cells_x{0};
    std::uint32_t cells_z{0};
    float cell_size_m{8.0F};
    HydrologyMode mode{HydrologyMode::Off};
    float river_probability{0.0F};
    float base_width_m{8.0F};
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
            return std::isfinite(minimum) && std::isfinite(maximum) && minimum >= floor &&
                   maximum >= minimum;
        };
        return seed != 0U && map_size_m >= 128 && map_size_m <= 4096 &&
               cells_x >= 2 && cells_z >= 2 &&
               std::isfinite(cell_size_m) && cell_size_m > 0.0F &&
               std::isfinite(river_probability) && river_probability >= 0.0F &&
               river_probability <= 1.0F && std::isfinite(base_width_m) && base_width_m > 0.0F &&
               main_river_min <= main_river_max && main_river_max <= 4U &&
               finite_range(stream_width_min_m, stream_width_max_m, 0.25F) &&
               finite_range(river_width_min_m, river_width_max_m, 1.0F) &&
               finite_range(depth_min_m, depth_max_m, 0.05F) &&
               std::isfinite(meander_strength) && meander_strength >= 0.0F &&
               meander_strength <= 1.0F &&
               finite_range(valley_width_min_m, valley_width_max_m, 2.0F);
    }
};

} // namespace genomes::hydrology
