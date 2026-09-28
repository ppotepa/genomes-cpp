#include <genomes/combat/LineOfSight.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace genomes::combat {

namespace {

thread_local LOSBatchStats g_last_stats{};

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] std::uint64_t snapshotRevision(const world::WorldQuerySnapshot& snapshot) noexcept {
    std::uint64_t revision = 0U;
    for (const world::QueryRegion& region : snapshot.regions()) {
        revision = std::max(revision, region.revision);
    }
    return revision;
}

[[nodiscard]] bool terrainBlocks(const world::WorldQuerySnapshot& snapshot,
                                 foundation::Vec3 eye,
                                 foundation::Vec3 aim,
                                 float& distance) noexcept {
    constexpr std::uint32_t samples = 32U;
    const foundation::Vec3 delta{aim.x - eye.x, aim.y - eye.y, aim.z - eye.z};
    const float segment_length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    for (std::uint32_t index = 1U; index < samples; ++index) {
        const float t = static_cast<float>(index) / static_cast<float>(samples);
        const foundation::Vec3 point{eye.x + delta.x * t, eye.y + delta.y * t,
                                     eye.z + delta.z * t};
        const world::TerrainSampleResult terrain = snapshot.terrainHeight(point.x, point.z);
        if (terrain.completeness == world::QueryCompleteness::PartialUnloaded ||
            terrain.completeness == world::QueryCompleteness::Missing) {
            continue;
        }
        if (point.y <= terrain.height + 1.0e-3F) {
            distance = segment_length * t;
            return true;
        }
    }
    return false;
}

} // namespace

bool LOSRequest::valid() const noexcept {
    const foundation::Vec3 delta{aim.x - eye.x, aim.y - eye.y, aim.z - eye.z};
    return observer_id != 0U && target_id != 0U && observer_id != target_id && finite(eye) &&
           finite(aim) && std::isfinite(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z) &&
           delta.x * delta.x + delta.y * delta.y + delta.z * delta.z > 1.0e-8F;
}

foundation::Result<LOSResult, foundation::Error> LineOfSight::queryOne(
    const world::WorldQuerySnapshot& snapshot, const LOSRequest& request) {
    if (!request.valid()) {
        return foundation::Result<LOSResult, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid LOS request"});
    }
    LOSResult result{};
    result.world_revision = snapshotRevision(snapshot);
    const world::QuerySegmentResult segment = snapshot.querySegment(request.eye, request.aim, true);
    result.completeness = segment.completeness;
    if (segment.completeness == world::QueryCompleteness::Missing ||
        segment.completeness == world::QueryCompleteness::PartialUnloaded) {
        return foundation::Result<LOSResult, foundation::Error>::success(result);
    }
    const foundation::Vec3 delta{request.aim.x - request.eye.x, request.aim.y - request.eye.y,
                                 request.aim.z - request.eye.z};
    const float segment_length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    for (const world::QuerySegmentHit& hit : segment.hits) {
        if (hit.candidate.id == request.observer_id && hit.t <= 1.0e-3F) {
            continue;
        }
        if (hit.candidate.id == request.target_id) {
            result.visible = true;
            result.hit_distance = hit.t * segment_length;
            return foundation::Result<LOSResult, foundation::Error>::success(result);
        }
        result.blocker = hit.candidate.id;
        result.hit_distance = hit.t * segment_length;
        return foundation::Result<LOSResult, foundation::Error>::success(result);
    }
    if (terrainBlocks(snapshot, request.eye, request.aim, result.hit_distance)) {
        return foundation::Result<LOSResult, foundation::Error>::success(result);
    }
    result.visible = true;
    result.hit_distance = segment_length;
    return foundation::Result<LOSResult, foundation::Error>::success(result);
}

foundation::Result<std::vector<LOSResult>, foundation::Error> LineOfSight::query(
    const world::WorldQuerySnapshot& snapshot, std::span<const LOSRequest> requests) {
    g_last_stats = {};
    g_last_stats.requests = static_cast<std::uint32_t>(requests.size());
    std::vector<LOSResult> results;
    results.reserve(requests.size());
    for (const LOSRequest& request : requests) {
        const auto result = queryOne(snapshot, request);
        if (!result) {
            return foundation::Result<std::vector<LOSResult>, foundation::Error>::failure(
                result.error());
        }
        results.push_back(result.value());
        if (result.value().visible) {
            ++g_last_stats.visible;
        } else if (result.value().completeness != world::QueryCompleteness::Complete) {
            ++g_last_stats.stale;
        } else {
            ++g_last_stats.blocked;
        }
    }
    return foundation::Result<std::vector<LOSResult>, foundation::Error>::success(
        std::move(results));
}

LOSBatchStats LineOfSight::lastStats() noexcept { return g_last_stats; }

} // namespace genomes::combat
