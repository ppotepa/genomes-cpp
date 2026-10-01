#pragma once

#include <genomes/foundation/Types.hpp>

#include <cstdint>

namespace genomes::world {

struct GridLayout final {
    foundation::Vec3 origin{};
    std::uint32_t cell_count{0U};
    std::uint32_t sample_count{0U};
    float spacing_m{8.0F};
    float extent_m{0.0F};

    [[nodiscard]] bool valid() const noexcept {
        return cell_count >= 2U && sample_count == cell_count + 1U && spacing_m > 0.0F &&
               extent_m == static_cast<float>(cell_count) * spacing_m;
    }

    [[nodiscard]] static GridLayout forMap(std::uint32_t map_size_m) noexcept {
        constexpr std::uint32_t spacing = 8U;
        if (map_size_m == 0U || map_size_m % spacing != 0U) return {};
        const std::uint32_t cells = map_size_m / spacing;
        return {{-static_cast<float>(map_size_m) * 0.5F, 0.0F,
                 -static_cast<float>(map_size_m) * 0.5F},
                cells, cells + 1U, static_cast<float>(spacing), static_cast<float>(map_size_m)};
    }
};

} // namespace genomes::world
