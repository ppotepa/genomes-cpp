#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/terrain/HeightField.hpp>
#include <genomes/terrain/TerrainSpec.hpp>

namespace genomes::terrain {

class TerrainGenerator final {
public:
    [[nodiscard]] static foundation::Result<HeightField, foundation::Error> generate(
        const TerrainSpec& spec);
};

} // namespace genomes::terrain
