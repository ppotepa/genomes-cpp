#include <genomes/hydrology/HydrologyArtifact.hpp>

#include <genomes/foundation/StableHash.hpp>
#include <genomes/proc/RandomStream.hpp>
#include <genomes/proc/SeedPath.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <queue>
#include <utility>

namespace genomes::hydrology {
namespace {
constexpr float kPi = 3.14159265358979323846F;

struct SegmentSample final {
    float distance_squared{std::numeric_limits<float>::max()};
    float t{0.0F};
};

inline constexpr std::uint32_t kNoDrainageCell = std::numeric_limits<std::uint32_t>::max();

struct DrainageField final {
    std::uint32_t cells_x{0U};
    std::uint32_t cells_z{0U};
    float cell_size_m{0.0F};
    float origin_x{0.0F};
    float origin_z{0.0F};
    std::vector<float> terrain;
    std::vector<std::uint32_t> downstream;

    [[nodiscard]] bool valid() const noexcept {
        return cells_x >= 2U && cells_z >= 2U && cell_size_m > 0.0F &&
               terrain.size() == static_cast<std::size_t>(cells_x) * cells_z &&
               downstream.size() == terrain.size();
    }
};

[[nodiscard]] SegmentSample segmentSample(float px, float pz, foundation::Vec3 a,
                                          foundation::Vec3 b) noexcept {
    const float dx = b.x - a.x;
    const float dz = b.z - a.z;
    const float length_squared = dx * dx + dz * dz;
    const float t = length_squared > 0.000001F
                        ? std::clamp(((px - a.x) * dx + (pz - a.z) * dz) / length_squared,
                                     0.0F, 1.0F)
                        : 0.0F;
    const float ox = px - (a.x + t * dx);
    const float oz = pz - (a.z + t * dz);
    return {ox * ox + oz * oz, t};
}

[[nodiscard]] float sampleTerrain(const HydrologyTerrainView& terrain, float x,
                                  float z) noexcept {
    if (!terrain.valid()) return 0.0F;
    const float gx = (x - terrain.origin_x) / terrain.cell_size_m;
    const float gz = (z - terrain.origin_z) / terrain.cell_size_m;
    const int x0 = std::clamp(static_cast<int>(std::floor(gx)), 0,
                              static_cast<int>(terrain.samples_x) - 1);
    const int z0 = std::clamp(static_cast<int>(std::floor(gz)), 0,
                              static_cast<int>(terrain.samples_z) - 1);
    const int x1 = std::min(x0 + 1, static_cast<int>(terrain.samples_x) - 1);
    const int z1 = std::min(z0 + 1, static_cast<int>(terrain.samples_z) - 1);
    const float tx = std::clamp(gx - static_cast<float>(x0), 0.0F, 1.0F);
    const float tz = std::clamp(gz - static_cast<float>(z0), 0.0F, 1.0F);
    const auto at = [&terrain](int sx, int sz) {
        return terrain.heights[static_cast<std::size_t>(sz) * terrain.samples_x +
                               static_cast<std::size_t>(sx)];
    };
    return std::lerp(std::lerp(at(x0, z0), at(x1, z0), tx),
                     std::lerp(at(x0, z1), at(x1, z1), tx), tz);
}

[[nodiscard]] DrainageField makeDrainageField(const HydrologySpec& spec,
                                               const HydrologyTerrainView& terrain) {
    DrainageField result{};
    if (!terrain.valid()) return result;
    result.cells_x = spec.cells_x;
    result.cells_z = spec.cells_z;
    result.cell_size_m = spec.cell_size_m;
    result.origin_x = -static_cast<float>(spec.map_size_m) * 0.5F;
    result.origin_z = result.origin_x;
    const std::size_t count = static_cast<std::size_t>(result.cells_x) * result.cells_z;
    result.terrain.resize(count);
    result.downstream.assign(count, kNoDrainageCell);
    for (std::uint32_t z = 0U; z < result.cells_z; ++z) {
        for (std::uint32_t x = 0U; x < result.cells_x; ++x) {
            const float world_x = result.origin_x + (static_cast<float>(x) + 0.5F) *
                                                   result.cell_size_m;
            const float world_z = result.origin_z + (static_cast<float>(z) + 0.5F) *
                                                   result.cell_size_m;
            result.terrain[static_cast<std::size_t>(z) * result.cells_x + x] =
                sampleTerrain(terrain, world_x, world_z);
        }
    }

    std::vector<std::uint8_t> visited(count, 0U);
    using QueueEntry = std::pair<float, std::uint32_t>;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<>> frontier;
    const auto push_boundary = [&](std::uint32_t x, std::uint32_t z) {
        const std::uint32_t cell = z * result.cells_x + x;
        if (visited[cell] != 0U) return;
        visited[cell] = 1U;
        frontier.emplace(result.terrain[cell], cell);
    };
    for (std::uint32_t x = 0U; x < result.cells_x; ++x) {
        push_boundary(x, 0U);
        push_boundary(x, result.cells_z - 1U);
    }
    for (std::uint32_t z = 1U; z + 1U < result.cells_z; ++z) {
        push_boundary(0U, z);
        push_boundary(result.cells_x - 1U, z);
    }
    constexpr std::array<std::pair<int, int>, 8U> neighbours{{
        {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1},
    }};
    while (!frontier.empty()) {
        const auto [level, cell] = frontier.top();
        frontier.pop();
        const std::uint32_t x = cell % result.cells_x;
        const std::uint32_t z = cell / result.cells_x;
        for (const auto [offset_x, offset_z] : neighbours) {
            const int next_x = static_cast<int>(x) + offset_x;
            const int next_z = static_cast<int>(z) + offset_z;
            if (next_x < 0 || next_z < 0 || next_x >= static_cast<int>(result.cells_x) ||
                next_z >= static_cast<int>(result.cells_z)) {
                continue;
            }
            const std::uint32_t next = static_cast<std::uint32_t>(next_z) * result.cells_x +
                                       static_cast<std::uint32_t>(next_x);
            if (visited[next] != 0U) continue;
            visited[next] = 1U;
            result.downstream[next] = cell;
            frontier.emplace(std::max(result.terrain[next], level + 0.01F), next);
        }
    }
    return result;
}

[[nodiscard]] std::uint64_t hashPoint(std::uint64_t hash,
                                      foundation::Vec3 point) noexcept {
    hash = foundation::stableHashCombine(hash, std::bit_cast<std::uint32_t>(point.x));
    hash = foundation::stableHashCombine(hash, std::bit_cast<std::uint32_t>(point.z));
    return foundation::stableHashCombine(hash, std::bit_cast<std::uint32_t>(point.y));
}

void constrainChannelSurface(HydrologyArtifact& artifact, const HydrologyTerrainView& terrain,
                             std::uint32_t offset, std::uint32_t point_count, float depth,
                             float valley_width) {
    if (point_count < 2U || static_cast<std::size_t>(offset) + point_count >
                              artifact.river_points.size()) {
        return;
    }
    // Terrain carving applies the same bound.  Without this constraint a
    // downhill polyline can request a water surface far below a local ridge;
    // the bounded carver then leaves dry terrain above that surface.
    const float maximum_cut = std::max(depth * 2.5F,
        std::min(valley_width * 0.11F, depth * 4.0F + 2.0F));
    std::vector<float> surfaces(point_count);
    std::vector<float> maximum_drops(point_count - 1U);
    for (std::uint32_t index = 0U; index < point_count; ++index) {
        const foundation::Vec3& point = artifact.river_points[offset + index];
        const float terrain_floor = sampleTerrain(terrain, point.x, point.z) - maximum_cut +
                                    depth + 0.05F;
        surfaces[index] = std::max(point.y, terrain_floor);
        if (index == 0U) continue;
        const foundation::Vec3& previous = artifact.river_points[offset + index - 1U];
        const float segment_length = std::hypot(point.x - previous.x, point.z - previous.z);
        maximum_drops[index - 1U] = std::max(depth * 1.5F, segment_length * 0.045F);
    }
    // First pass propagates a high upstream floor downstream, so the later
    // strictly descending profile can still honour its maximum allowed drop.
    for (std::uint32_t index = 0U; index + 1U < point_count; ++index) {
        surfaces[index + 1U] = std::max(surfaces[index + 1U],
                                        surfaces[index] - maximum_drops[index]);
    }
    // Then impose the minimum flow slope in reverse.  The first pass makes
    // this compatible with the per-segment maximum.
    for (std::uint32_t reverse = point_count - 1U; reverse > 0U; --reverse) {
        const std::uint32_t index = reverse - 1U;
        const foundation::Vec3& from = artifact.river_points[offset + index];
        const foundation::Vec3& to = artifact.river_points[offset + index + 1U];
        const float minimum_drop = std::max(0.01F,
            std::hypot(to.x - from.x, to.z - from.z) * 0.0015F);
        surfaces[index] = std::max(surfaces[index], surfaces[index + 1U] + minimum_drop);
    }
    for (std::uint32_t index = 0U; index < point_count; ++index) {
        artifact.river_points[offset + index].y = surfaces[index];
    }
}

[[nodiscard]] bool appendDrainageRiver(HydrologyArtifact& artifact, const HydrologySpec& spec,
                                        const HydrologyTerrainView& terrain,
                                        const DrainageField& drainage,
                                        proc::RandomStream& random,
                                        std::uint32_t river_index) {
    if (!drainage.valid()) return false;
    const std::uint32_t margin = std::max(2U, std::min(drainage.cells_x, drainage.cells_z) / 10U);
    if (drainage.cells_x <= margin * 2U || drainage.cells_z <= margin * 2U) return false;
    const auto trace = [&drainage](std::uint32_t source) {
        std::vector<std::uint32_t> result;
        result.reserve(drainage.cells_x + drainage.cells_z);
        for (std::uint32_t cell = source; cell != kNoDrainageCell &&
                                       result.size() <= drainage.downstream.size();
             cell = drainage.downstream[cell]) {
            result.push_back(cell);
        }
        return result;
    };
    std::vector<std::uint32_t> selected;
    float selected_score = std::numeric_limits<float>::lowest();
    const std::uint32_t span_x = drainage.cells_x - margin * 2U;
    const std::uint32_t span_z = drainage.cells_z - margin * 2U;
    const std::size_t minimum_path = std::max<std::size_t>(12U,
        std::min(drainage.cells_x, drainage.cells_z) / 4U);
    const float center_x = (static_cast<float>(drainage.cells_x) - 1.0F) * 0.5F;
    const float center_z = (static_cast<float>(drainage.cells_z) - 1.0F) * 0.5F;
    for (std::uint32_t candidate = 0U; candidate < 256U; ++candidate) {
        // The first candidate anchors the selection to the active battle
        // space. The remaining candidates retain seed-derived variety; all
        // choices still trace the same terrain-derived drainage graph.
        const std::uint32_t x = candidate == 0U
            ? static_cast<std::uint32_t>(center_x)
            : margin + random.bounded(span_x);
        const std::uint32_t z = candidate == 0U
            ? static_cast<std::uint32_t>(center_z)
            : margin + random.bounded(span_z);
        const std::uint32_t source = z * drainage.cells_x + x;
        auto path = trace(source);
        if (path.size() < minimum_path) continue;
        float center_distance = std::numeric_limits<float>::max();
        for (const std::uint32_t cell : path) {
            const float path_x = static_cast<float>(cell % drainage.cells_x);
            const float path_z = static_cast<float>(cell / drainage.cells_x);
            center_distance = std::min(center_distance,
                std::hypot(path_x - center_x, path_z - center_z) * drainage.cell_size_m);
        }
        const float score = drainage.terrain[source] +
                            static_cast<float>(path.size()) * drainage.cell_size_m * 0.025F -
                            center_distance * 0.45F;
        if (score > selected_score) {
            selected_score = score;
            selected = std::move(path);
        }
    }
    if (selected.empty()) return false;

    const float requested_width = static_cast<float>(random.uniformRange(
        spec.river_width_min_m, spec.river_width_max_m));
    const float width = std::max(requested_width, spec.cell_size_m * 2.0F);
    const float depth = static_cast<float>(random.uniformRange(spec.depth_min_m, spec.depth_max_m));
    const float valley = std::max(width * 2.0F, static_cast<float>(random.uniformRange(
        spec.valley_width_min_m, spec.valley_width_max_m)));
    const std::size_t stride = std::max<std::size_t>(1U, selected.size() / 64U);
    std::vector<foundation::Vec3> sampled_centerline;
    sampled_centerline.reserve(selected.size() / stride + 2U);
    const auto append_exact_sample = [&](std::uint32_t cell) {
        sampled_centerline.push_back(
            {drainage.origin_x +
                 (static_cast<float>(cell % drainage.cells_x) + 0.5F) * drainage.cell_size_m,
             0.0F,
             drainage.origin_z +
                 (static_cast<float>(cell / drainage.cells_x) + 0.5F) * drainage.cell_size_m});
    };
    for (std::size_t index = 0U; index < selected.size(); index += stride) {
        if (index == 0U || index + stride >= selected.size()) {
            append_exact_sample(selected[index]);
            continue;
        }
        const std::size_t first = index - stride;
        const std::size_t last = std::min(selected.size() - 1U, index + stride);
        float cell_x = 0.0F;
        float cell_z = 0.0F;
        for (std::size_t sample = first; sample <= last; ++sample) {
            cell_x += static_cast<float>(selected[sample] % drainage.cells_x);
            cell_z += static_cast<float>(selected[sample] / drainage.cells_x);
        }
        const float sample_count = static_cast<float>(last - first + 1U);
        sampled_centerline.push_back(
            {drainage.origin_x + (cell_x / sample_count + 0.5F) * drainage.cell_size_m, 0.0F,
             drainage.origin_z + (cell_z / sample_count + 0.5F) * drainage.cell_size_m});
    }
    if (selected.size() > 1U &&
        selected.back() != selected[(selected.size() - 1U) / stride * stride]) {
        append_exact_sample(selected.back());
    }
    // Priority-flood drainage advances in grid-neighbour steps.  Relaxing the
    // compact control polyline twice removes those grid corners before both
    // channel carving and mesh compilation see it, while preserving the
    // deterministic source and boundary outlet exactly.
    for (std::uint32_t pass = 0U; pass < 2U && sampled_centerline.size() > 2U; ++pass) {
        std::vector<foundation::Vec3> relaxed = sampled_centerline;
        for (std::size_t index = 1U; index + 1U < sampled_centerline.size(); ++index) {
            relaxed[index].x = sampled_centerline[index - 1U].x * 0.25F +
                               sampled_centerline[index].x * 0.50F +
                               sampled_centerline[index + 1U].x * 0.25F;
            relaxed[index].z = sampled_centerline[index - 1U].z * 0.25F +
                               sampled_centerline[index].z * 0.50F +
                               sampled_centerline[index + 1U].z * 0.25F;
        }
        sampled_centerline = std::move(relaxed);
    }
    const std::uint32_t offset = static_cast<std::uint32_t>(artifact.river_points.size());
    const auto append_position = [&](float world_x, float world_z, float ground) {
        float surface = ground - depth * 0.28F;
        if (artifact.river_points.size() > offset) {
            const foundation::Vec3& previous = artifact.river_points.back();
            const float segment_length = std::hypot(world_x - previous.x, world_z - previous.z);
            const float minimum_drop = std::max(0.01F, segment_length * 0.0015F);
            const float maximum_drop = std::max(depth * 1.5F, segment_length * 0.045F);
            surface = std::clamp(surface, previous.y - maximum_drop,
                                 previous.y - minimum_drop);
        }
        artifact.river_points.push_back({world_x, surface, world_z});
    };
    for (const foundation::Vec3& point : sampled_centerline) {
        append_position(point.x, point.z, sampleTerrain(terrain, point.x, point.z));
    }
    const std::uint32_t point_count =
        static_cast<std::uint32_t>(artifact.river_points.size() - offset);
    if (point_count < 2U) return false;
    constrainChannelSurface(artifact, terrain, offset, point_count, depth, valley);
    foundation::StableId id = foundation::stableHashCombine(
        spec.seed, foundation::stableHashCombine(foundation::stable_id("river"), river_index));
    if (id == 0U) id = 1U;
    artifact.rivers.push_back({id, offset, point_count, width, depth, valley});
    return true;
}

void appendRiver(HydrologyArtifact& artifact, const HydrologySpec& spec,
                 const HydrologyTerrainView& terrain, proc::RandomStream& random,
                 std::uint32_t river_index, bool tributary) {
    const float half = static_cast<float>(spec.map_size_m) * 0.5F;
    const float margin = std::max(8.0F, half * 0.06F);
    const bool along_x = random.uniform01() < 0.5;
    const float lateral = static_cast<float>(random.uniformRange(-half * 0.34, half * 0.34));
    // Keep bends broad enough for a sampled height field. Narrow, high-amplitude
    // oscillations make a valid polyline look like a chain of angular ponds.
    const float meander = half * 0.08F * spec.meander_strength;
    const float phase = static_cast<float>(random.uniformRange(0.0, 2.0 * kPi));
    const float requested_width = tributary
                            ? static_cast<float>(random.uniformRange(spec.stream_width_min_m,
                                                                     spec.stream_width_max_m))
                            : static_cast<float>(random.uniformRange(spec.river_width_min_m,
                                                                     spec.river_width_max_m));
    // A water surface narrower than roughly two terrain cells cannot be kept
    // below the sampled banks. Publish a representable channel instead of a
    // dry-looking slit in an 8 m terrain mesh.
    const float width = std::max(
        requested_width, spec.cell_size_m * (tributary ? 1.5F : 2.0F));
    const float depth = static_cast<float>(random.uniformRange(spec.depth_min_m, spec.depth_max_m));
    const float valley = std::max(width * 2.0F, static_cast<float>(random.uniformRange(
                                             spec.valley_width_min_m,
                                             spec.valley_width_max_m)));
    constexpr std::uint32_t point_count = 33U;
    const std::uint32_t offset = static_cast<std::uint32_t>(artifact.river_points.size());
    artifact.river_points.reserve(artifact.river_points.size() + point_count);
    const float source_longitudinal = -half + margin;
    const float outlet_longitudinal = half - margin;
    const auto endpoint_height = [&](float longitudinal) {
        const float x = along_x ? longitudinal : lateral;
        const float z = along_x ? lateral : longitudinal;
        return sampleTerrain(terrain, x, z);
    };
    const bool forward = endpoint_height(source_longitudinal) >=
                         endpoint_height(outlet_longitudinal);
    float previous_ground = std::numeric_limits<float>::infinity();
    float previous_lateral = lateral;
    for (std::uint32_t index = 0U; index < point_count; ++index) {
        const float t = static_cast<float>(index) / static_cast<float>(point_count - 1U);
        const float flow_t = forward ? t : 1.0F - t;
        float longitudinal = std::lerp(source_longitudinal, outlet_longitudinal, flow_t);
        if (tributary) longitudinal = std::lerp(-half * 0.75F, half * 0.50F, flow_t);
        const float wave = std::sin(t * kPi * 2.0F + phase) * meander;
        const float desired_lateral = std::clamp(lateral + wave, -half + margin, half - margin);
        float best_lateral = desired_lateral;
        float best_height = std::numeric_limits<float>::max();
        float best_score = std::numeric_limits<float>::max();
        for (int candidate = -12; candidate <= 12; ++candidate) {
            const float candidate_lateral = std::clamp(
                desired_lateral + static_cast<float>(candidate) * spec.cell_size_m,
                -half + margin, half - margin);
            const float x = along_x ? longitudinal : candidate_lateral;
            const float z = along_x ? candidate_lateral : longitudinal;
            const float height = sampleTerrain(terrain, x, z);
            const float route_cost = std::abs(candidate_lateral - desired_lateral) * 0.40F;
            const float bend_cost = index > 0U
                ? std::abs(candidate_lateral - previous_lateral) * 0.80F : 0.0F;
            const float uphill_cost = std::isfinite(previous_ground)
                ? std::max(0.0F, height - previous_ground + 0.25F) * 10.0F : 0.0F;
            const float score = height + route_cost + bend_cost + uphill_cost;
            if (score < best_score) {
                best_score = score;
                best_height = height;
                best_lateral = candidate_lateral;
            }
        }
        const float x = along_x ? longitudinal : best_lateral;
        const float z = along_x ? best_lateral : longitudinal;
        float surface = best_height - depth * 0.28F;
        if (index > 0U) {
            const auto previous = artifact.river_points.back();
            const float segment_length = std::hypot(x - previous.x, z - previous.z);
            const float minimum_drop = std::max(0.01F, segment_length * 0.0015F);
            const float maximum_drop = std::max(depth * 1.5F, segment_length * 0.045F);
            surface = std::clamp(surface, previous.y - maximum_drop,
                                 previous.y - minimum_drop);
        }
        artifact.river_points.push_back({x, surface, z});
        previous_ground = best_height;
        previous_lateral = best_lateral;
    }
    constrainChannelSurface(artifact, terrain, offset, point_count, depth, valley);
    foundation::StableId id = foundation::stableHashCombine(
        spec.seed, foundation::stableHashCombine(foundation::stable_id("river"), river_index));
    if (id == 0U) id = 1U;
    artifact.rivers.push_back({id, offset, point_count, width, depth, valley});
}

void appendTributary(HydrologyArtifact& artifact, const HydrologySpec& spec,
                     const HydrologyTerrainView& terrain, proc::RandomStream& random,
                     std::uint32_t river_index) {
    std::vector<const RiverPath*> main_rivers;
    for (const RiverPath& river : artifact.rivers) {
        if (river.parent_river_id == 0U) main_rivers.push_back(&river);
    }
    if (main_rivers.empty()) return;

    const RiverPath& parent = *main_rivers[random.bounded(
        static_cast<std::uint32_t>(main_rivers.size()))];
    const std::uint32_t parent_span = parent.point_count - 2U;
    const std::uint32_t confluence_index = parent.point_offset + 1U + random.bounded(parent_span);
    const foundation::Vec3 confluence = artifact.river_points[confluence_index];
    const float half = static_cast<float>(spec.map_size_m) * 0.5F;
    const float margin = std::max(8.0F, half * 0.06F);
    const float lateral = static_cast<float>(random.uniformRange(-half * 0.34, half * 0.34));
    const std::array<foundation::Vec3, 4U> boundary_candidates{{
        {-half + margin, 0.0F, lateral}, {half - margin, 0.0F, lateral},
        {lateral, 0.0F, -half + margin}, {lateral, 0.0F, half - margin},
    }};
    foundation::Vec3 source = boundary_candidates.front();
    float source_height = sampleTerrain(terrain, source.x, source.z);
    for (const foundation::Vec3 candidate : boundary_candidates) {
        const float height = sampleTerrain(terrain, candidate.x, candidate.z);
        if (height > source_height) {
            source = candidate;
            source_height = height;
        }
    }

    const float dx = confluence.x - source.x;
    const float dz = confluence.z - source.z;
    const float line_length = std::hypot(dx, dz);
    if (!std::isfinite(line_length) || line_length <= spec.cell_size_m) return;
    const foundation::Vec3 perpendicular{-dz / line_length, 0.0F, dx / line_length};
    const float meander = half * 0.045F * spec.meander_strength;
    const float phase = static_cast<float>(random.uniformRange(0.0, 2.0 * kPi));
    const float requested_width = static_cast<float>(random.uniformRange(
        spec.stream_width_min_m, spec.stream_width_max_m));
    const float width = std::max(requested_width, spec.cell_size_m * 1.5F);
    const float depth = static_cast<float>(random.uniformRange(spec.depth_min_m, spec.depth_max_m));
    const float valley = std::max(width * 2.0F, static_cast<float>(random.uniformRange(
        spec.valley_width_min_m, spec.valley_width_max_m)));
    constexpr std::uint32_t point_count = 21U;
    std::vector<foundation::Vec3> route;
    route.reserve(point_count);
    float previous_offset = 0.0F;
    float previous_ground = std::numeric_limits<float>::infinity();
    for (std::uint32_t index = 0U; index < point_count; ++index) {
        const float t = static_cast<float>(index) / static_cast<float>(point_count - 1U);
        if (index == 0U || index + 1U == point_count) {
            const foundation::Vec3 endpoint = index == 0U ? source : confluence;
            route.push_back({endpoint.x, sampleTerrain(terrain, endpoint.x, endpoint.z), endpoint.z});
            previous_ground = route.back().y;
            continue;
        }
        const float envelope = std::sin(t * kPi);
        const float desired_offset = std::sin(t * kPi * 2.0F + phase) * meander * envelope;
        const foundation::Vec3 center{std::lerp(source.x, confluence.x, t), 0.0F,
                                      std::lerp(source.z, confluence.z, t)};
        float best_offset = desired_offset;
        float best_ground = std::numeric_limits<float>::max();
        float best_score = std::numeric_limits<float>::max();
        for (int candidate = -8; candidate <= 8; ++candidate) {
            const float offset = desired_offset + static_cast<float>(candidate) * spec.cell_size_m;
            const float x = center.x + perpendicular.x * offset;
            const float z = center.z + perpendicular.z * offset;
            const float ground = sampleTerrain(terrain, x, z);
            const float route_cost = std::abs(offset - desired_offset) * 0.40F;
            const float bend_cost = std::abs(offset - previous_offset) * 0.80F;
            const float uphill_cost = std::isfinite(previous_ground)
                ? std::max(0.0F, ground - previous_ground + 0.25F) * 10.0F : 0.0F;
            const float score = ground + route_cost + bend_cost + uphill_cost;
            if (score < best_score) {
                best_score = score;
                best_ground = ground;
                best_offset = offset;
            }
        }
        route.push_back({center.x + perpendicular.x * best_offset, best_ground,
                         center.z + perpendicular.z * best_offset});
        previous_offset = best_offset;
        previous_ground = best_ground;
    }

    std::vector<float> surfaces(point_count);
    // The last point is exact: it is the shared water level at the confluence.
    surfaces.back() = confluence.y;
    for (std::size_t reverse = point_count - 1U; reverse > 0U; --reverse) {
        const std::size_t index = reverse - 1U;
        const float segment_length = std::hypot(route[index + 1U].x - route[index].x,
                                                route[index + 1U].z - route[index].z);
        const float minimum_rise = std::max(0.01F, segment_length * 0.0015F);
        const float maximum_rise = std::max(depth * 1.5F, segment_length * 0.045F);
        const float desired_surface = route[index].y - depth * 0.28F;
        surfaces[index] = std::clamp(desired_surface, surfaces[index + 1U] + minimum_rise,
                                     surfaces[index + 1U] + maximum_rise);
    }

    const std::uint32_t offset = static_cast<std::uint32_t>(artifact.river_points.size());
    for (std::size_t index = 0U; index < route.size(); ++index) {
        artifact.river_points.push_back({route[index].x, surfaces[index], route[index].z});
    }
    foundation::StableId id = foundation::stableHashCombine(
        spec.seed, foundation::stableHashCombine(foundation::stable_id("river"), river_index));
    if (id == 0U) id = 1U;
    artifact.rivers.push_back({id, offset, point_count, width, depth, valley, parent.id});
}
} // namespace

bool HydrologyArtifact::valid() const noexcept {
    const std::size_t expected = static_cast<std::size_t>(cells_x) * cells_z;
    if (generator_version != HydrologyGeneratorVersion || cells_x < 2U || cells_z < 2U ||
        !std::isfinite(cell_size_m) || cell_size_m <= 0.0F || water_mask.size() != expected ||
        shore_mask.size() != expected || flood_mask.size() != expected ||
        wetness.size() != expected || water_distance.size() != expected ||
        (enabled && rivers.empty())) {
        return false;
    }
    for (const RiverPath& river : rivers) {
        const bool has_parent = river.parent_river_id == 0U || std::any_of(
            rivers.begin(), rivers.end(), [&river](const RiverPath& candidate) {
                return candidate.id == river.parent_river_id;
            });
        if (river.id == 0U || river.point_count < 2U || !has_parent ||
            static_cast<std::size_t>(river.point_offset) + river.point_count >
                river_points.size() ||
            !std::isfinite(river.width_m) || river.width_m <= 0.0F ||
            !std::isfinite(river.depth_m) || river.depth_m <= 0.0F ||
            !std::isfinite(river.valley_width_m) || river.valley_width_m < river.width_m)
            return false;
    }
    return true;
}

float HydrologyArtifact::waterDistance(float x, float z) const noexcept {
    if (cells_x == 0U || cells_z == 0U || water_distance.empty())
        return std::numeric_limits<float>::infinity();
    const int ix = static_cast<int>(std::floor((x - origin_x) / cell_size_m));
    const int iz = static_cast<int>(std::floor((z - origin_z) / cell_size_m));
    const auto cx = static_cast<std::uint32_t>(std::clamp(ix, 0, static_cast<int>(cells_x) - 1));
    const auto cz = static_cast<std::uint32_t>(std::clamp(iz, 0, static_cast<int>(cells_z) - 1));
    return water_distance[static_cast<std::size_t>(cz) * cells_x + cx];
}

bool HydrologyArtifact::isWater(float x, float z) const noexcept {
    if (cells_x == 0U || cells_z == 0U || water_mask.empty()) return false;
    const int ix = static_cast<int>(std::floor((x - origin_x) / cell_size_m));
    const int iz = static_cast<int>(std::floor((z - origin_z) / cell_size_m));
    const auto cx = static_cast<std::uint32_t>(std::clamp(ix, 0, static_cast<int>(cells_x) - 1));
    const auto cz = static_cast<std::uint32_t>(std::clamp(iz, 0, static_cast<int>(cells_z) - 1));
    return water_mask[static_cast<std::size_t>(cz) * cells_x + cx] != 0U;
}

bool HydrologyArtifact::isFloodplain(float x, float z) const noexcept {
    if (cells_x == 0U || cells_z == 0U || flood_mask.empty()) return false;
    const int ix = static_cast<int>(std::floor((x - origin_x) / cell_size_m));
    const int iz = static_cast<int>(std::floor((z - origin_z) / cell_size_m));
    const auto cx = static_cast<std::uint32_t>(std::clamp(ix, 0, static_cast<int>(cells_x) - 1));
    const auto cz = static_cast<std::uint32_t>(std::clamp(iz, 0, static_cast<int>(cells_z) - 1));
    return flood_mask[static_cast<std::size_t>(cz) * cells_x + cx] != 0U;
}

WaterSample HydrologyArtifact::sampleWater(float x, float z) const noexcept {
    WaterSample result{};
    result.distance_m = waterDistance(x, z);
    if (!enabled) return result;
    float nearest = std::numeric_limits<float>::max();
    for (const RiverPath& river : rivers) {
        const std::size_t begin = river.point_offset;
        const std::size_t end = begin + river.point_count;
        if (river.point_count < 2U || end > river_points.size()) continue;
        for (std::size_t point = begin + 1U; point < end; ++point) {
            const auto segment = segmentSample(x, z, river_points[point - 1U], river_points[point]);
            if (segment.distance_squared >= nearest) continue;
            nearest = segment.distance_squared;
            const auto from = river_points[point - 1U];
            const auto to = river_points[point];
            result.surface_y = std::lerp(from.y, to.y, segment.t);
            result.depth_m = river.depth_m;
            const float length = std::hypot(to.x - from.x, to.z - from.z);
            result.flow_direction = length > 0.0001F
                                        ? foundation::Vec3{(to.x - from.x) / length, 0.0F,
                                                           (to.z - from.z) / length}
                                        : foundation::Vec3{};
            result.has_water = std::sqrt(nearest) <= river.width_m * 0.5F;
        }
    }
    if (!wetness.empty()) {
        const auto ix = static_cast<std::uint32_t>(std::clamp(
            static_cast<int>(std::floor((x - origin_x) / cell_size_m)), 0,
            static_cast<int>(cells_x) - 1));
        const auto iz = static_cast<std::uint32_t>(std::clamp(
            static_cast<int>(std::floor((z - origin_z) / cell_size_m)), 0,
            static_cast<int>(cells_z) - 1));
        result.wetness = wetness[static_cast<std::size_t>(iz) * cells_x + ix];
    }
    return result;
}

HydrologyArtifact HydrologyArtifact::translated(foundation::Vec3 offset) const {
    HydrologyArtifact result = *this;
    result.origin_x += offset.x;
    result.origin_z += offset.z;
    for (foundation::Vec3& point : result.river_points) point = point + offset;
    for (RiverCrossing& crossing : result.crossings) crossing.position = crossing.position + offset;
    return result;
}

foundation::Result<HydrologyArtifact, foundation::Error> HydrologyGenerator::generate(
    const HydrologySpec& spec, HydrologyTerrainView terrain) {
    if (!spec.valid() || (!terrain.heights.empty() && !terrain.valid())) {
        return foundation::Result<HydrologyArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid hydrology specification"});
    }
    HydrologyArtifact artifact{};
    artifact.seed = spec.seed;
    artifact.cells_x = spec.cells_x;
    artifact.cells_z = spec.cells_z;
    artifact.cell_size_m = spec.cell_size_m;
    artifact.origin_x = -static_cast<float>(spec.map_size_m) * 0.5F;
    artifact.origin_z = artifact.origin_x;
    const std::size_t cell_count = static_cast<std::size_t>(spec.cells_x) * spec.cells_z;
    artifact.water_mask.assign(cell_count, 0U);
    artifact.shore_mask.assign(cell_count, 0U);
    artifact.flood_mask.assign(cell_count, 0U);
    artifact.wetness.assign(cell_count, 0.0F);
    artifact.water_distance.assign(cell_count, static_cast<float>(spec.map_size_m));

    proc::RandomStream random(proc::SeedPath(spec.seed).child("river", 0));
    const bool present = spec.mode == HydrologyMode::Forced ||
                         (spec.mode == HydrologyMode::SeededOptional &&
                          random.uniform01() <= static_cast<double>(spec.river_probability));
    artifact.enabled = present;
    if (!present || spec.mode == HydrologyMode::Off) {
        artifact.content_hash = foundation::stableHashCombine(spec.seed, 0U);
        return foundation::Result<HydrologyArtifact, foundation::Error>::success(std::move(artifact));
    }

    const std::uint32_t main_count = spec.main_river_min +
        random.bounded(static_cast<std::uint32_t>(spec.main_river_max - spec.main_river_min) + 1U);
    const std::uint32_t forced_count = std::max(1U, main_count);
    const DrainageField drainage = makeDrainageField(spec, terrain);
    for (std::uint32_t index = 0U; index < forced_count; ++index) {
        if (!appendDrainageRiver(artifact, spec, terrain, drainage, random, index)) {
            appendRiver(artifact, spec, terrain, random, index, false);
        }
    }
    const std::uint32_t tributary_count = spec.tributary_density == TributaryDensity::None
                                              ? 0U
                                              : (spec.tributary_density == TributaryDensity::Low ? 1U : 2U);
    for (std::uint32_t index = 0U; index < tributary_count; ++index)
        appendTributary(artifact, spec, terrain, random, forced_count + index);

    for (std::uint32_t z = 0U; z < spec.cells_z; ++z) {
        for (std::uint32_t x = 0U; x < spec.cells_x; ++x) {
            const float world_x = artifact.origin_x + (static_cast<float>(x) + 0.5F) * spec.cell_size_m;
            const float world_z = artifact.origin_z + (static_cast<float>(z) + 0.5F) * spec.cell_size_m;
            float nearest_edge = static_cast<float>(spec.map_size_m);
            float nearest_center = static_cast<float>(spec.map_size_m);
            float flood_distance = static_cast<float>(spec.map_size_m);
            for (const RiverPath& river : artifact.rivers) {
                const std::size_t begin = river.point_offset;
                const std::size_t end = begin + river.point_count;
                for (std::size_t point = begin + 1U; point < end; ++point) {
                    const float distance = std::sqrt(segmentSample(world_x, world_z,
                        artifact.river_points[point - 1U], artifact.river_points[point]).distance_squared);
                    nearest_center = std::min(nearest_center, distance);
                    nearest_edge = std::min(nearest_edge,
                        std::max(0.0F, distance - river.width_m * 0.5F));
                    flood_distance = std::min(flood_distance,
                        distance - river.valley_width_m * 0.5F);
                }
            }
            const std::size_t cell = static_cast<std::size_t>(z) * spec.cells_x + x;
            artifact.water_distance[cell] = nearest_edge;
            artifact.water_mask[cell] = nearest_edge <= 0.0F ? 1U : 0U;
            artifact.shore_mask[cell] = nearest_edge > 0.0F &&
                nearest_edge <= spec.cell_size_m * 1.5F ? 1U : 0U;
            artifact.flood_mask[cell] = flood_distance <= 0.0F ? 1U : 0U;
            artifact.wetness[cell] = std::clamp(1.0F - nearest_center /
                std::max(spec.valley_width_max_m, 1.0F), 0.0F, 1.0F);
        }
    }

    std::uint64_t hash = foundation::stableHashU64(HydrologyGeneratorVersion);
    hash = foundation::stableHashCombine(hash, spec.seed);
    for (const RiverPath& river : artifact.rivers) {
        hash = foundation::stableHashCombine(hash, river.id);
        hash = foundation::stableHashCombine(hash, river.parent_river_id);
        hash = foundation::stableHashCombine(hash, std::bit_cast<std::uint32_t>(river.width_m));
        hash = foundation::stableHashCombine(hash, std::bit_cast<std::uint32_t>(river.depth_m));
        hash = foundation::stableHashCombine(
            hash, std::bit_cast<std::uint32_t>(river.valley_width_m));
    }
    for (const foundation::Vec3 point : artifact.river_points) hash = hashPoint(hash, point);
    artifact.content_hash = hash == 0U ? 1U : hash;
    return foundation::Result<HydrologyArtifact, foundation::Error>::success(std::move(artifact));
}
} // namespace genomes::hydrology
