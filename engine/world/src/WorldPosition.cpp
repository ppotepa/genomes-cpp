#include <genomes/world_core/WorldPosition.hpp>

#include <genomes/foundation/StableHash.hpp>

#include <cmath>

namespace genomes::world_core {

RegionCoord regionCoordFor(WorldPosition position,
                           const WorldCoordinateConfig& config) noexcept {
    if (!config.valid()) {
        return {};
    }
    return {static_cast<std::int64_t>(std::floor(position.x / config.region_size_m)),
            static_cast<std::int64_t>(std::floor(position.z / config.region_size_m)), 0};
}

WorldPosition regionOrigin(RegionCoord coord,
                           const WorldCoordinateConfig& config) noexcept {
    return {static_cast<double>(coord.x) * config.region_size_m, 0.0,
            static_cast<double>(coord.z) * config.region_size_m};
}

RegionLocalPosition toLocal(WorldPosition position,
                            RegionCoord coord,
                            const WorldCoordinateConfig& config) noexcept {
    const WorldPosition origin = regionOrigin(coord, config);
    return {static_cast<float>(position.x - origin.x), static_cast<float>(position.y - origin.y),
            static_cast<float>(position.z - origin.z)};
}

WorldPosition toGlobal(RegionLocalPosition position,
                       RegionCoord coord,
                       const WorldCoordinateConfig& config) noexcept {
    const WorldPosition origin = regionOrigin(coord, config);
    return {origin.x + static_cast<double>(position.x),
            origin.y + static_cast<double>(position.y),
            origin.z + static_cast<double>(position.z)};
}

RegionId regionId(WorldId world, RegionCoord coord) noexcept {
    std::uint64_t hash = foundation::stableHashCombine(world.value(),
                                                       static_cast<std::uint64_t>(coord.x));
    hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(coord.z));
    hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(
                                                        static_cast<std::int64_t>(coord.layer)));
    return RegionId(hash == 0 ? 1 : hash);
}

} // namespace genomes::world_core
