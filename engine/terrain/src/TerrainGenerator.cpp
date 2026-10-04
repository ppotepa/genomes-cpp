#include <genomes/terrain/TerrainGenerator.hpp>

#include <genomes/foundation/StableHash.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace genomes::terrain {

namespace {

[[nodiscard]] float latticeValue(proc::Seed seed, std::int64_t x, std::int64_t z) noexcept {
    std::uint64_t hash = foundation::stableHashU64(seed);
    hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(x));
    hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(z));
    hash = foundation::stableHashU64(hash);
    constexpr double denominator = static_cast<double>(UINT64_MAX);
    return static_cast<float>(static_cast<double>(hash) / denominator * 2.0 - 1.0);
}

[[nodiscard]] float smooth(float value) noexcept {
    return value * value * (3.0F - 2.0F * value);
}

[[nodiscard]] float valueNoise(proc::Seed seed, float x, float z) noexcept {
    const auto x0 = static_cast<std::int64_t>(std::floor(x));
    const auto z0 = static_cast<std::int64_t>(std::floor(z));
    const float fx = smooth(x - static_cast<float>(x0));
    const float fz = smooth(z - static_cast<float>(z0));
    const float a = latticeValue(seed, x0, z0);
    const float b = latticeValue(seed, x0 + 1, z0);
    const float c = latticeValue(seed, x0, z0 + 1);
    const float d = latticeValue(seed, x0 + 1, z0 + 1);
    const float ab = std::lerp(a, b, fx);
    const float cd = std::lerp(c, d, fx);
    return std::lerp(ab, cd, fz);
}

[[nodiscard]] float fbm(proc::Seed seed, float x, float z, float scale,
                        int octaves, float persistence, float lacunarity) noexcept {
    float amplitude = 1.0F;
    float frequency = 1.0F / std::max(1.0F, scale);
    float sum = 0.0F;
    float normalization = 0.0F;
    for (int octave = 0; octave < octaves; ++octave) {
        const proc::Seed octave_seed = foundation::stableHashCombine(
            seed, static_cast<std::uint64_t>(octave + 1));
        sum += valueNoise(octave_seed, x * frequency, z * frequency) * amplitude;
        normalization += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    return normalization > 0.0F ? sum / normalization : 0.0F;
}

[[nodiscard]] float smoothstep(float minimum, float maximum, float value) noexcept {
    const float denominator = std::max(maximum - minimum, 1.0e-5F);
    const float t = std::clamp((value - minimum) / denominator, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

[[nodiscard]] float terrainHeight(proc::Seed seed, float x, float z,
                                  const world::TerrainGenerationConfig& config) noexcept {
    const float roughness = std::clamp(config.roughness, 0.0F, 1.0F);
    const float landform_scale = std::max(32.0F, config.landform_scale_m);
    const proc::Seed warp_x_seed = foundation::stableHashCombine(
        seed, foundation::stableHashString("terrain.warp.x"));
    const proc::Seed warp_z_seed = foundation::stableHashCombine(
        seed, foundation::stableHashString("terrain.warp.z"));
    const float warp_amplitude = landform_scale * std::lerp(0.08F, 0.22F, roughness);
    const float warp_scale = landform_scale * 1.45F;
    const float warped_x = x + fbm(warp_x_seed, x, z, warp_scale, 3, 0.50F, 2.02F) *
                                   warp_amplitude;
    const float warped_z = z + fbm(warp_z_seed, x, z, warp_scale, 3, 0.50F, 2.02F) *
                                   warp_amplitude;
    const proc::Seed continental_seed = foundation::stableHashCombine(
        seed, foundation::stableHashString("terrain.continental"));
    const proc::Seed hills_seed = foundation::stableHashCombine(
        seed, foundation::stableHashString("terrain.hills"));
    const proc::Seed ridge_seed = foundation::stableHashCombine(
        seed, foundation::stableHashString("terrain.ridges"));
    const proc::Seed knoll_seed = foundation::stableHashCombine(
        seed, foundation::stableHashString("terrain.knolls"));
    const proc::Seed detail_seed = foundation::stableHashCombine(
        seed, foundation::stableHashString("terrain.detail"));
    const float continental = fbm(continental_seed, warped_x, warped_z,
                                  landform_scale * 2.55F, 5, 0.53F, 2.0F);
    const float hills = fbm(hills_seed, warped_x, warped_z, landform_scale * 0.68F,
                            5, std::lerp(0.42F, 0.58F, roughness), 2.08F);
    const float ridge = 1.0F - std::abs(fbm(ridge_seed, warped_x, warped_z,
                                            landform_scale * 0.95F, 4, 0.50F, 2.0F));
    const float knolls = fbm(knoll_seed, warped_x, warped_z, landform_scale * 0.64F,
                             4, std::lerp(0.38F, 0.56F, roughness), 2.04F);
    const float detail = fbm(detail_seed, warped_x, warped_z, landform_scale * 0.16F,
                             4, std::lerp(0.34F, 0.52F, roughness), 2.15F);
    const float continental_shape = std::copysign(
        std::pow(std::abs(continental), 1.35F), continental);
    const float hill_mask = smoothstep(-0.18F, 0.42F, continental);
    const float ridge_mask = smoothstep(-0.06F, 0.46F, continental);
    const float elevation = config.elevation_range_m;
    switch (config.preset) {
    case world::TerrainPreset::Plains:
        return elevation * (continental_shape * 0.10F + hills * 0.08F +
                            detail * std::lerp(0.008F, 0.028F, roughness));
    case world::TerrainPreset::RollingHills:
        return elevation * (continental_shape * 0.27F + hills *
                            std::lerp(0.21F, 0.35F, hill_mask) +
                            knolls * std::lerp(0.03F, 0.09F, hill_mask) +
                            (ridge - 0.53F) * 0.14F * ridge_mask +
                            detail * std::lerp(0.012F, 0.045F, roughness));
    case world::TerrainPreset::Highlands:
        return elevation * (continental_shape * 0.42F + hills *
                            std::lerp(0.28F, 0.44F, hill_mask) +
                            knolls * std::lerp(0.08F, 0.18F, hill_mask) +
                            std::max(0.0F, ridge - 0.42F) * 0.44F * ridge_mask +
                            detail * std::lerp(0.020F, 0.070F, roughness));
    case world::TerrainPreset::RiverValley:
        // Hydrology will cut the final valley. This broad, noise-shaped basin
        // creates room for it without tying every generated valley to a world axis.
        return elevation * (continental_shape * 0.20F + hills * 0.18F +
                            (ridge - 0.58F) * 0.08F * ridge_mask +
                            detail * std::lerp(0.010F, 0.032F, roughness));
    }
    return 0.0F;
}

[[nodiscard]] float segmentDistance(float x, float z, foundation::Vec3 a,
                                    foundation::Vec3 b, float& t) noexcept {
    const float dx = b.x - a.x;
    const float dz = b.z - a.z;
    const float length_squared = dx * dx + dz * dz;
    t = length_squared > 1.0e-6F
            ? std::clamp(((x - a.x) * dx + (z - a.z) * dz) / length_squared, 0.0F, 1.0F)
            : 0.0F;
    const float nearest_x = a.x + dx * t;
    const float nearest_z = a.z + dz * t;
    const float ox = x - nearest_x;
    const float oz = z - nearest_z;
    return std::sqrt(ox * ox + oz * oz);
}

} // namespace

foundation::Result<HeightField, foundation::Error> TerrainGenerator::generate(
    const TerrainSpec& spec) {
    auto field_result = HeightField::create(spec);
    if (!field_result) {
        return field_result;
    }

    HeightField field = std::move(field_result.value());
    for (std::uint32_t z = 0; z < field.height(); ++z) {
        for (std::uint32_t x = 0; x < field.width(); ++x) {
            const float world_x = static_cast<float>(field.originX()) +
                                  static_cast<float>(x) * field.cellSize();
            const float world_z = static_cast<float>(field.originZ()) +
                                  static_cast<float>(z) * field.cellSize();
            field.at(x, z) = terrainHeight(spec.seed_path.seed(), world_x, world_z,
                                           spec.generation);
        }
    }
    return foundation::Result<HeightField, foundation::Error>::success(std::move(field));
}

foundation::Result<void, foundation::Error> TerrainGenerator::carveChannels(
    HeightField& field, std::span<const TerrainChannel> channels) {
    for (const TerrainChannel& channel : channels) {
        if (channel.id == 0U || channel.centerline.size() < 2U ||
            !std::isfinite(channel.width_m) || channel.width_m <= 0.0F ||
            !std::isfinite(channel.depth_m) || channel.depth_m <= 0.0F ||
            !std::isfinite(channel.valley_width_m) ||
            channel.valley_width_m < channel.width_m) {
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument, "invalid terrain channel"});
        }
    }
    for (std::uint32_t z = 0U; z < field.height(); ++z) {
        for (std::uint32_t x = 0U; x < field.width(); ++x) {
            const float world_x = static_cast<float>(field.originX()) +
                                  static_cast<float>(x) * field.cellSize();
            const float world_z = static_cast<float>(field.originZ()) +
                                  static_cast<float>(z) * field.cellSize();
            const float original = field.at(x, z);
            float target = original;
            for (const TerrainChannel& channel : channels) {
                for (std::size_t point = 1U; point < channel.centerline.size(); ++point) {
                    float t = 0.0F;
                    const auto from = channel.centerline[point - 1U];
                    const auto to = channel.centerline[point];
                    const float distance = segmentDistance(world_x, world_z, from, to, t);
                    if (distance > channel.valley_width_m) continue;
                    const float surface = std::lerp(from.y, to.y, t);
                    const float half_width = channel.width_m * 0.5F;
                    const float half_valley = channel.valley_width_m * 0.5F;
                    if (distance > half_valley) continue;
                    const float valley_t = distance <= half_width
                                               ? 0.0F
                                               : smoothstep(half_width, half_valley, distance);
                    const float bed = surface - channel.depth_m;
                    const float shaped_bed = std::lerp(bed, original, valley_t);
                    // A channel must shape a valley, but it must never turn a
                    // locally imperfect flow path into a bottomless canyon.
                    const float maximum_cut = std::max(
                        channel.depth_m * 2.5F,
                        std::min(channel.valley_width_m * 0.11F,
                                 channel.depth_m * 4.0F + 2.0F));
                    const float bounded_bed = std::max(shaped_bed, original - maximum_cut);
                    target = std::min(target, bounded_bed);
                }
            }
            field.at(x, z) = target;
        }
    }
    return foundation::Result<void, foundation::Error>::success();
}

} // namespace genomes::terrain
