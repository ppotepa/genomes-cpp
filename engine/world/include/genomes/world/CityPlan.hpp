#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/proc/Seed.hpp>
#include <genomes/roads/RoadGraph.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

namespace genomes::world {

struct CityGenerationRequest final {
    proc::Seed seed{0x5EED2026ull};
    std::uint32_t map_size_m{600};
    float buildings{0.55F};
    float fenced_parcels{0.48F};

    [[nodiscard]] bool valid() const noexcept {
        const auto valid_density = [](float value) {
            return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
        };
        return map_size_m >= 128 && map_size_m <= 4096 && valid_density(buildings) &&
               valid_density(fenced_parcels);
    }
};

struct CityParcel final {
    foundation::StableId id{0};
    foundation::Vec3 position{};
    foundation::Vec3 parcel_scale{1.0F, 1.0F, 1.0F};
    foundation::Vec3 building_scale{1.0F, 1.0F, 1.0F};
    float rotation_y{0.0F};
    std::uint32_t parcel_variant{0};
    std::uint32_t building_variant{0};
    bool fenced{false};
};

struct CityPlan final {
    std::uint32_t generator_version{1};
    proc::Seed seed{0};
    std::uint32_t map_size_m{0};
    roads::RoadGraph road_graph{};
    std::vector<CityParcel> parcels;
    std::uint64_t content_hash{0};
};

class CityGenerator final {
public:
    [[nodiscard]] static foundation::Result<CityPlan, foundation::Error> generate(
        const CityGenerationRequest& request);
};

} // namespace genomes::world
