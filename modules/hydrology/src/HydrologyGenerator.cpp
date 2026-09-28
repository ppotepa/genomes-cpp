#include <genomes/hydrology/HydrologyArtifact.hpp>

#include <genomes/foundation/StableHash.hpp>
#include <genomes/proc/RandomStream.hpp>
#include <genomes/proc/SeedPath.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <utility>

namespace genomes::hydrology {

namespace {

constexpr float kPi = 3.14159265358979323846F;

[[nodiscard]] float segment_distance_squared(float px,
                                             float pz,
                                             foundation::Vec3 a,
                                             foundation::Vec3 b) noexcept {
    const float dx = b.x - a.x;
    const float dz = b.z - a.z;
    const float length_squared = dx * dx + dz * dz;
    const float t = length_squared > 0.000001F
                        ? std::clamp(((px - a.x) * dx + (pz - a.z) * dz) / length_squared,
                                     0.0F, 1.0F)
                        : 0.0F;
    const float x = a.x + t * dx;
    const float z = a.z + t * dz;
    const float ox = px - x;
    const float oz = pz - z;
    return ox * ox + oz * oz;
}

[[nodiscard]] std::uint64_t hash_point(std::uint64_t hash,
                                       foundation::Vec3 point) noexcept {
    hash = foundation::stableHashCombine(hash, std::bit_cast<std::uint32_t>(point.x));
    hash = foundation::stableHashCombine(hash, std::bit_cast<std::uint32_t>(point.z));
    return foundation::stableHashCombine(hash, std::bit_cast<std::uint32_t>(point.y));
}

} // namespace

bool HydrologyArtifact::valid() const noexcept {
    const std::size_t expected = static_cast<std::size_t>(cells_x) * cells_z;
    return generator_version != 0 && cells_x >= 2 && cells_z >= 2 &&
           std::isfinite(cell_size_m) && cell_size_m > 0.0F &&
           water_mask.size() == expected && water_distance.size() == expected &&
           (!enabled || !rivers.empty());
}

float HydrologyArtifact::waterDistance(float x, float z) const noexcept {
    if (cells_x == 0 || cells_z == 0 || water_distance.empty()) {
        return std::numeric_limits<float>::infinity();
    }
    const int ix = static_cast<int>(std::floor((x - origin_x) / cell_size_m));
    const int iz = static_cast<int>(std::floor((z - origin_z) / cell_size_m));
    const std::uint32_t clamped_x = static_cast<std::uint32_t>(
        std::clamp(ix, 0, static_cast<int>(cells_x) - 1));
    const std::uint32_t clamped_z = static_cast<std::uint32_t>(
        std::clamp(iz, 0, static_cast<int>(cells_z) - 1));
    return water_distance[static_cast<std::size_t>(clamped_z) * cells_x + clamped_x];
}

bool HydrologyArtifact::isWater(float x, float z) const noexcept {
    if (cells_x == 0 || cells_z == 0 || water_mask.empty()) {
        return false;
    }
    const int ix = static_cast<int>(std::floor((x - origin_x) / cell_size_m));
    const int iz = static_cast<int>(std::floor((z - origin_z) / cell_size_m));
    const std::uint32_t clamped_x = static_cast<std::uint32_t>(
        std::clamp(ix, 0, static_cast<int>(cells_x) - 1));
    const std::uint32_t clamped_z = static_cast<std::uint32_t>(
        std::clamp(iz, 0, static_cast<int>(cells_z) - 1));
    return water_mask[static_cast<std::size_t>(clamped_z) * cells_x + clamped_x] != 0;
}

HydrologyArtifact HydrologyArtifact::translated(foundation::Vec3 offset) const {
    HydrologyArtifact result = *this;
    result.origin_x += offset.x;
    result.origin_z += offset.z;
    for (foundation::Vec3& point : result.river_points) {
        point.x += offset.x;
        point.y += offset.y;
        point.z += offset.z;
    }
    return result;
}

foundation::Result<HydrologyArtifact, foundation::Error> HydrologyGenerator::generate(
    const HydrologySpec& spec) {
    if (!spec.valid()) {
        return foundation::Result<HydrologyArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid hydrology specification"});
    }

    HydrologyArtifact artifact{};
    artifact.seed = spec.seed;
    artifact.cells_x = spec.cells_x;
    artifact.cells_z = spec.cells_z;
    artifact.cell_size_m = spec.cell_size_m;
    artifact.origin_x = -static_cast<float>(spec.map_size_m) * 0.5F;
    artifact.origin_z = -static_cast<float>(spec.map_size_m) * 0.5F;
    const std::size_t cell_count = static_cast<std::size_t>(spec.cells_x) * spec.cells_z;
    artifact.water_mask.assign(cell_count, 0);
    artifact.water_distance.assign(cell_count, static_cast<float>(spec.map_size_m));

    const proc::SeedPath root(spec.seed);
    proc::RandomStream random(root.child("river", 0));
    const bool present = spec.mode == HydrologyMode::Forced ||
                         (spec.mode == HydrologyMode::SeededOptional &&
                          random.uniform01() <= static_cast<double>(spec.river_probability));
    artifact.enabled = present;
    if (!present || spec.mode == HydrologyMode::Off) {
        artifact.content_hash = foundation::stableHashCombine(spec.seed, 0);
        return foundation::Result<HydrologyArtifact, foundation::Error>::success(
            std::move(artifact));
    }

    const float half = static_cast<float>(spec.map_size_m) * 0.5F;
    const float margin = std::max(8.0F, half * 0.08F);
    const bool along_x = random.uniform01() < 0.5;
    const float lateral = static_cast<float>(random.uniformRange(-half * 0.22, half * 0.22));
    const float bend = static_cast<float>(random.uniformRange(-half * 0.18, half * 0.18));
    const float width = static_cast<float>(
        random.uniformRange(static_cast<double>(spec.base_width_m) * 0.75,
                            static_cast<double>(spec.base_width_m) * 1.35));
    constexpr std::uint32_t point_count = 17;
    artifact.river_points.reserve(point_count);
    for (std::uint32_t index = 0; index < point_count; ++index) {
        const float t = static_cast<float>(index) / static_cast<float>(point_count - 1);
        const float longitudinal = -half + margin + (2.0F * (half - margin) * t);
        const float wave = std::sin(t * kPi * 2.0F) * bend;
        artifact.river_points.push_back(along_x
                                            ? foundation::Vec3{longitudinal, 0.0F,
                                                               lateral + wave}
                                            : foundation::Vec3{lateral + wave, 0.0F,
                                                               longitudinal});
    }
    artifact.rivers.push_back({foundation::stableHashCombine(spec.seed,
                                                               foundation::stable_id("river.0")),
                               0, point_count, width, width * 0.16F});

    const float half_width = width * 0.5F;
    const float flood_width = width * 2.5F;
    for (std::uint32_t z = 0; z < spec.cells_z; ++z) {
        for (std::uint32_t x = 0; x < spec.cells_x; ++x) {
            const float world_x = artifact.origin_x +
                                  (static_cast<float>(x) + 0.5F) * spec.cell_size_m;
            const float world_z = artifact.origin_z +
                                  (static_cast<float>(z) + 0.5F) * spec.cell_size_m;
            float nearest_squared = std::numeric_limits<float>::max();
            for (std::size_t point = 1; point < artifact.river_points.size(); ++point) {
                nearest_squared = std::min(
                    nearest_squared,
                    segment_distance_squared(world_x, world_z,
                                             artifact.river_points[point - 1],
                                             artifact.river_points[point]));
            }
            const float distance = std::sqrt(nearest_squared);
            const std::size_t cell = static_cast<std::size_t>(z) * spec.cells_x + x;
            artifact.water_distance[cell] = std::max(0.0F, distance - half_width);
            artifact.water_mask[cell] = distance <= flood_width ? 1 : 0;
        }
    }

    std::uint64_t hash = foundation::stableHashU64(HydrologyGeneratorVersion);
    hash = foundation::stableHashCombine(hash, spec.seed);
    hash = foundation::stableHashCombine(hash, along_x ? 1 : 0);
    hash = foundation::stableHashCombine(hash, std::bit_cast<std::uint32_t>(width));
    for (const foundation::Vec3 point : artifact.river_points) {
        hash = hash_point(hash, point);
    }
    artifact.content_hash = hash == 0 ? 1 : hash;
    return foundation::Result<HydrologyArtifact, foundation::Error>::success(std::move(artifact));
}

} // namespace genomes::hydrology
