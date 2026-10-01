#include <genomes/world_core/WorldQuery.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <unordered_map>
#include <utility>

namespace genomes::world_core {

namespace {

[[nodiscard]] bool finite_position(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool segmentAabb(foundation::Vec3 origin,
                               foundation::Vec3 delta,
                               const Aabb3& bounds,
                               float& enter) noexcept {
    float minimum_t = 0.0F;
    float maximum_t = 1.0F;
    const float origins[3] = {origin.x, origin.y, origin.z};
    const float directions[3] = {delta.x, delta.y, delta.z};
    const float minimums[3] = {bounds.minimum.x, bounds.minimum.y, bounds.minimum.z};
    const float maximums[3] = {bounds.maximum.x, bounds.maximum.y, bounds.maximum.z};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (std::abs(directions[axis]) <= 1.0e-8F) {
            if (origins[axis] < minimums[axis] || origins[axis] > maximums[axis]) {
                return false;
            }
            continue;
        }
        const float reciprocal = 1.0F / directions[axis];
        float near_t = (minimums[axis] - origins[axis]) * reciprocal;
        float far_t = (maximums[axis] - origins[axis]) * reciprocal;
        if (near_t > far_t) {
            std::swap(near_t, far_t);
        }
        minimum_t = std::max(minimum_t, near_t);
        maximum_t = std::min(maximum_t, far_t);
        if (minimum_t > maximum_t) {
            return false;
        }
    }
    enter = minimum_t;
    return true;
}

} // namespace

bool QueryCandidate::valid() const noexcept {
    return id != 0 && bounds.valid() && region_id.isValid() && revision != 0;
}

bool QueryRegion::valid() const noexcept {
    if (!id.isValid() || revision == 0 || (resident && terrain_sample != nullptr &&
                                            terrain_context == nullptr)) {
        return false;
    }
    for (const QueryCandidate& candidate : candidates) {
        if (!candidate.valid() || candidate.region_id != id || candidate.revision != revision) {
            return false;
        }
    }
    return true;
}

foundation::Result<WorldQuerySnapshot, foundation::Error> WorldQuerySnapshot::create(
    WorldId world, WorldCoordinateConfig config, std::vector<QueryRegion> regions) {
    if (!world.isValid() || !config.valid() || regions.empty()) {
        return foundation::Result<WorldQuerySnapshot, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid world query snapshot"});
    }
    std::sort(regions.begin(), regions.end(), [](const QueryRegion& left, const QueryRegion& right) {
        return left.coordinate < right.coordinate;
    });
    for (std::size_t index = 0; index < regions.size(); ++index) {
        if (!regions[index].valid() || (index > 0 &&
                                        regions[index - 1].coordinate == regions[index].coordinate)) {
            return foundation::Result<WorldQuerySnapshot, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument,
                 "invalid or duplicate world query region"});
        }
    }
    WorldQuerySnapshot snapshot;
    snapshot.world_ = world;
    snapshot.coordinate_config_ = config;
    snapshot.regions_ = std::move(regions);
    return foundation::Result<WorldQuerySnapshot, foundation::Error>::success(
        std::move(snapshot));
}

std::vector<const QueryRegion*> WorldQuerySnapshot::overlapping(const Aabb3& bounds) const {
    std::vector<const QueryRegion*> result;
    if (!bounds.valid()) {
        return result;
    }
    const RegionCoord minimum = regionCoordFor(
        {bounds.minimum.x, 0.0, bounds.minimum.z}, coordinate_config_);
    const RegionCoord maximum = regionCoordFor(
        {bounds.maximum.x, 0.0, bounds.maximum.z}, coordinate_config_);
    for (const QueryRegion& region : regions_) {
        if (region.coordinate.x >= minimum.x && region.coordinate.x <= maximum.x &&
            region.coordinate.z >= minimum.z && region.coordinate.z <= maximum.z) {
            result.push_back(&region);
        }
    }
    return result;
}

QueryAabbResult WorldQuerySnapshot::queryAabb(const Aabb3& bounds, bool stable_order) const {
    QueryAabbResult result{};
    if (!bounds.valid()) {
        return result;
    }
    const std::vector<const QueryRegion*> touched = overlapping(bounds);
    if (touched.empty()) {
        result.completeness = QueryCompleteness::Missing;
        return result;
    }
    result.completeness = QueryCompleteness::Complete;
    std::map<foundation::StableId, QueryCandidate> unique;
    for (const QueryRegion* region : touched) {
        if (!region->resident) {
            result.completeness = QueryCompleteness::PartialUnloaded;
            continue;
        }
        for (const QueryCandidate& candidate : region->candidates) {
            if (candidate.bounds.overlaps(bounds)) {
                unique.emplace(candidate.id, candidate);
            }
        }
    }
    result.candidates.reserve(unique.size());
    for (const auto& [id, candidate] : unique) {
        (void)id;
        result.candidates.push_back(candidate);
    }
    if (!stable_order) {
        return result;
    }
    std::sort(result.candidates.begin(), result.candidates.end(),
              [](const QueryCandidate& left, const QueryCandidate& right) {
                  return left.id < right.id;
              });
    return result;
}

QuerySegmentResult WorldQuerySnapshot::querySegment(foundation::Vec3 origin,
                                                     foundation::Vec3 end,
                                                     bool stable_order) const {
    QuerySegmentResult result{};
    if (!finite_position(origin) || !finite_position(end)) {
        return result;
    }
    const Aabb3 envelope{{std::min(origin.x, end.x), std::min(origin.y, end.y),
                          std::min(origin.z, end.z)},
                         {std::max(origin.x, end.x), std::max(origin.y, end.y),
                          std::max(origin.z, end.z)}};
    const QueryAabbResult broadphase = queryAabb(envelope, true);
    result.completeness = broadphase.completeness;
    const foundation::Vec3 delta{end.x - origin.x, end.y - origin.y, end.z - origin.z};
    for (const QueryCandidate& candidate : broadphase.candidates) {
        float t = 0.0F;
        if (segmentAabb(origin, delta, candidate.bounds, t)) {
            result.hits.push_back({candidate,
                                   t,
                                   {origin.x + delta.x * t, origin.y + delta.y * t,
                                    origin.z + delta.z * t}});
        }
    }
    std::sort(result.hits.begin(), result.hits.end(), [stable_order](const auto& left,
                                                                       const auto& right) {
        if (left.t != right.t) {
            return left.t < right.t;
        }
        return stable_order ? left.candidate.id < right.candidate.id : false;
    });
    return result;
}

TerrainSampleResult WorldQuerySnapshot::terrainHeight(float x, float z) const noexcept {
    if (!std::isfinite(x) || !std::isfinite(z)) {
        return {};
    }
    const RegionCoord coordinate = regionCoordFor({x, 0.0, z}, coordinate_config_);
    const auto iterator = std::lower_bound(
        regions_.begin(), regions_.end(), coordinate, [](const QueryRegion& region, RegionCoord key) {
            return region.coordinate < key;
        });
    if (iterator == regions_.end() || iterator->coordinate != coordinate) {
        return {};
    }
    if (!iterator->resident) {
        return {QueryCompleteness::PartialUnloaded, 0.0F};
    }
    if (iterator->terrain_sample == nullptr) {
        return {QueryCompleteness::Complete, 0.0F};
    }
    const float sampled = iterator->terrain_sample(iterator->terrain_context, x, z);
    return std::isfinite(sampled) ? TerrainSampleResult{QueryCompleteness::Complete, sampled}
                                  : TerrainSampleResult{QueryCompleteness::Missing, 0.0F};
}

std::optional<RegionCoord> WorldQuerySnapshot::regionAt(foundation::Vec3 position) const noexcept {
    if (!finite_position(position)) {
        return std::nullopt;
    }
    return regionCoordFor({position.x, position.y, position.z}, coordinate_config_);
}

foundation::Result<void, foundation::Error> WorldQueryService::publish(
    WorldQuerySnapshot snapshot) {
    if (snapshot.world().isValid() == false || snapshot.regions().empty()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid world query snapshot"});
    }
    snapshot_ = std::make_unique<WorldQuerySnapshot>(std::move(snapshot));
    return foundation::Result<void, foundation::Error>::success();
}

std::vector<QuerySegmentResult> querySegments(const WorldQuerySnapshot& snapshot,
                                              std::span<const QuerySegmentRequest> requests,
                                              bool stable_order) {
    std::vector<QuerySegmentResult> results;
    results.resize(requests.size());
    for (std::size_t index = 0; index < requests.size(); ++index) {
        results[index] = snapshot.querySegment(requests[index].origin,
                                               requests[index].end,
                                               stable_order);
    }
    return results;
}

} // namespace genomes::world_core
