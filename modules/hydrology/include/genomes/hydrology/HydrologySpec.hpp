#pragma once

#include <genomes/proc/Seed.hpp>

#include <cmath>
#include <cstdint>

namespace genomes::hydrology {

enum class HydrologyMode : std::uint8_t {
    Off,
    SeededOptional,
    Forced,
};

struct HydrologySpec final {
    proc::Seed seed{0x5EED2026ull};
    std::uint32_t map_size_m{600};
    std::uint32_t cells_x{0};
    std::uint32_t cells_z{0};
    float cell_size_m{8.0F};
    HydrologyMode mode{HydrologyMode::SeededOptional};
    float river_probability{0.35F};
    float base_width_m{8.0F};

    [[nodiscard]] bool valid() const noexcept {
        return map_size_m >= 128 && map_size_m <= 4096 && cells_x >= 2 && cells_z >= 2 &&
               std::isfinite(cell_size_m) && cell_size_m > 0.0F &&
               std::isfinite(river_probability) && river_probability >= 0.0F &&
               river_probability <= 1.0F && std::isfinite(base_width_m) && base_width_m > 0.0F;
    }
};

} // namespace genomes::hydrology
