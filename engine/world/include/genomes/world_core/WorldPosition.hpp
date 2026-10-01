#pragma once

#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StrongId.hpp>
#include <genomes/foundation/Types.hpp>

#include <compare>
#include <cmath>
#include <cstdint>

namespace genomes::world_core {

// Canonical neutral world coordinate contract. Product generation and
// presentation code may depend on these types without linking a generator.
struct WorldIdTag;
struct RegionIdTag;

using WorldId = foundation::StrongId<WorldIdTag>;
using RegionId = foundation::StrongId<RegionIdTag>;

struct WorldPosition final {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

struct RegionLocalPosition final {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
};

struct RegionCoord final {
    std::int64_t x{0};
    std::int64_t z{0};
    std::int32_t layer{0};

    friend constexpr auto operator<=>(const RegionCoord&, const RegionCoord&) noexcept = default;
};

struct WorldCoordinateConfig final {
    double region_size_m{256.0};

    [[nodiscard]] bool valid() const noexcept {
        return std::isfinite(region_size_m) && region_size_m > 0.0;
    }
};

[[nodiscard]] RegionCoord regionCoordFor(WorldPosition position,
                                         const WorldCoordinateConfig& config) noexcept;

[[nodiscard]] WorldPosition regionOrigin(RegionCoord coord,
                                          const WorldCoordinateConfig& config) noexcept;

[[nodiscard]] RegionLocalPosition toLocal(WorldPosition position,
                                          RegionCoord coord,
                                          const WorldCoordinateConfig& config) noexcept;

[[nodiscard]] WorldPosition toGlobal(RegionLocalPosition position,
                                     RegionCoord coord,
                                     const WorldCoordinateConfig& config) noexcept;

[[nodiscard]] RegionId regionId(WorldId world, RegionCoord coord) noexcept;

} // namespace genomes::world_core
