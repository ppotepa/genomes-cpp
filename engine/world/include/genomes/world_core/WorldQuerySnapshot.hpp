#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/world_core/DirtyBounds.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace genomes::world_core {

inline constexpr std::uint32_t WorldQuerySnapshotVersion = 1;

enum class QueryCompleteness : std::uint8_t {
    Complete,
    PartialUnloaded,
    Missing,
};

enum class QuerySourceKind : std::uint8_t {
    Terrain,
    Static,
    Dynamic,
    Rubble,
};

struct QueryCandidate final {
    foundation::StableId id{0};
    Aabb3 bounds{};
    QuerySourceKind source{QuerySourceKind::Static};
    RegionId region_id{};
    std::uint64_t revision{0};

    [[nodiscard]] bool valid() const noexcept;
};

struct QueryRegion final {
    RegionCoord coordinate{};
    RegionId id{};
    std::uint64_t revision{0};
    bool resident{false};
    std::vector<QueryCandidate> candidates;
    float (*terrain_sample)(void*, float, float) noexcept{nullptr};
    void* terrain_context{nullptr};

    [[nodiscard]] bool valid() const noexcept;
};

struct QueryAabbResult final {
    QueryCompleteness completeness{QueryCompleteness::Missing};
    std::vector<QueryCandidate> candidates;
};

struct QuerySegmentHit final {
    QueryCandidate candidate{};
    float t{0.0F};
    foundation::Vec3 point{};
};

struct QuerySegmentResult final {
    QueryCompleteness completeness{QueryCompleteness::Missing};
    std::vector<QuerySegmentHit> hits;
};

struct TerrainSampleResult final {
    QueryCompleteness completeness{QueryCompleteness::Missing};
    float height{0.0F};
};

class WorldQuerySnapshot final {
public:
    [[nodiscard]] static foundation::Result<WorldQuerySnapshot, foundation::Error> create(
        WorldId world, WorldCoordinateConfig config, std::vector<QueryRegion> regions);

    [[nodiscard]] QueryAabbResult queryAabb(const Aabb3&, bool stable_order = true) const;
    [[nodiscard]] QuerySegmentResult querySegment(foundation::Vec3 origin,
                                                   foundation::Vec3 end,
                                                   bool stable_order = true) const;
    [[nodiscard]] TerrainSampleResult terrainHeight(float x, float z) const noexcept;
    [[nodiscard]] std::optional<RegionCoord> regionAt(foundation::Vec3) const noexcept;

    [[nodiscard]] WorldId world() const noexcept { return world_; }
    [[nodiscard]] const WorldCoordinateConfig& coordinateConfig() const noexcept {
        return coordinate_config_;
    }
    [[nodiscard]] std::uint32_t version() const noexcept { return version_; }
    [[nodiscard]] const std::vector<QueryRegion>& regions() const noexcept { return regions_; }

private:
    [[nodiscard]] std::vector<const QueryRegion*> overlapping(const Aabb3&) const;

    std::uint32_t version_{WorldQuerySnapshotVersion};
    WorldId world_{};
    WorldCoordinateConfig coordinate_config_{};
    std::vector<QueryRegion> regions_;
};

} // namespace genomes::world_core
