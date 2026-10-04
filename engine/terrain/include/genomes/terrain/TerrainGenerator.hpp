#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/terrain/HeightField.hpp>
#include <genomes/terrain/TerrainSpec.hpp>

#include <span>
#include <vector>

namespace genomes::terrain {

struct TerrainChannel final {
    foundation::StableId id{0U};
    std::span<const foundation::Vec3> centerline;
    float width_m{1.0F};
    float depth_m{0.5F};
    float valley_width_m{8.0F};
};

class TerrainGenerator final {
public:
    [[nodiscard]] static foundation::Result<HeightField, foundation::Error> generate(
        const TerrainSpec& spec);
    [[nodiscard]] static foundation::Result<void, foundation::Error> carveChannels(
        HeightField& field, std::span<const TerrainChannel> channels);
};

} // namespace genomes::terrain
