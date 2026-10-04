#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/hydrology/HydrologySpec.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace genomes::hydrology {

inline constexpr std::uint32_t HydrologyGeneratorVersion = 7;

struct RiverPath final {
    foundation::StableId id{0};
    std::uint32_t point_offset{0};
    std::uint32_t point_count{0};
    float width_m{8.0F};
    float depth_m{1.0F};
    float valley_width_m{24.0F};
    // Zero denotes a main river. A tributary ends at a point of this parent.
    foundation::StableId parent_river_id{0U};
};

enum class CrossingType : std::uint8_t { Culvert, Ford, Bridge };

struct RiverCrossing final {
    foundation::StableId id{0U};
    foundation::StableId river_id{0U};
    foundation::StableId road_id{0U};
    foundation::Vec3 position{};
    float deck_width_m{0.0F};
    CrossingType type{CrossingType::Bridge};
};

struct WaterSample final {
    bool has_water{false};
    float surface_y{0.0F};
    float depth_m{0.0F};
    float distance_m{0.0F};
    float wetness{0.0F};
    foundation::Vec3 flow_direction{};
};

struct HydrologyArtifact final {
    std::uint32_t generator_version{HydrologyGeneratorVersion};
    proc::Seed seed{0};
    std::uint32_t cells_x{0};
    std::uint32_t cells_z{0};
    float cell_size_m{1.0F};
    float origin_x{0.0F};
    float origin_z{0.0F};
    bool enabled{false};
    std::vector<std::uint8_t> water_mask;
    std::vector<std::uint8_t> shore_mask;
    std::vector<std::uint8_t> flood_mask;
    std::vector<float> wetness;
    std::vector<float> water_distance;
    std::vector<foundation::Vec3> river_points;
    std::vector<RiverPath> rivers;
    std::vector<RiverCrossing> crossings;
    std::uint64_t content_hash{0};

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool isWater(float x, float z) const noexcept;
    [[nodiscard]] bool isFloodplain(float x, float z) const noexcept;
    [[nodiscard]] float waterDistance(float x, float z) const noexcept;
    [[nodiscard]] WaterSample sampleWater(float x, float z) const noexcept;
    [[nodiscard]] HydrologyArtifact translated(foundation::Vec3 offset) const;
};

class HydrologyGenerator final {
public:
    [[nodiscard]] static foundation::Result<HydrologyArtifact, foundation::Error> generate(
        const HydrologySpec&, HydrologyTerrainView terrain = {});
};

} // namespace genomes::hydrology
