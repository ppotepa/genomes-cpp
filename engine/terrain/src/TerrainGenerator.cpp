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

[[nodiscard]] float terrainHeight(proc::Seed seed, float x, float z) noexcept {
    float amplitude = 1.0F;
    float frequency = 1.0F / 180.0F;
    float sum = 0.0F;
    float normalization = 0.0F;
    for (int octave = 0; octave < 5; ++octave) {
        const proc::Seed octave_seed = foundation::stableHashCombine(
            seed, static_cast<std::uint64_t>(octave + 1));
        sum += valueNoise(octave_seed, x * frequency, z * frequency) * amplitude;
        normalization += amplitude;
        amplitude *= 0.5F;
        frequency *= 2.03F;
    }
    const float normalized = sum / normalization;
    const float basin = std::max(0.0F, 1.0F -
                                          std::sqrt((x * x + z * z) / (180.0F * 180.0F)));
    return normalized * 7.0F + basin * 1.5F;
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
            field.at(x, z) = terrainHeight(spec.seed_path.seed(), world_x, world_z);
        }
    }
    return foundation::Result<HeightField, foundation::Error>::success(std::move(field));
}

} // namespace genomes::terrain
