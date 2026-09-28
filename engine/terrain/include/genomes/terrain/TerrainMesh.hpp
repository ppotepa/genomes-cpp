#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/terrain/HeightField.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace genomes::terrain {

struct TerrainMeshVertex final {
    foundation::Vec3 position{};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
    foundation::Vec2 uv{};
};

struct TerrainMesh final {
    std::uint32_t source_width{0};
    std::uint32_t source_height{0};
    std::uint32_t sample_step{1};
    std::vector<TerrainMeshVertex> vertices;
    std::vector<std::uint32_t> indices;

    [[nodiscard]] std::size_t triangle_count() const noexcept {
        return indices.size() / 3;
    }
};

struct TerrainMeshSpec final {
    std::uint32_t sample_step{1};
    std::uint32_t max_vertices{4'000'000};
    std::uint32_t max_indices{12'000'000};

    [[nodiscard]] bool valid() const noexcept {
        return sample_step > 0 && max_vertices >= 3 && max_indices >= 3;
    }
};

class TerrainMeshBuilder final {
public:
    [[nodiscard]] static foundation::Result<TerrainMesh, foundation::Error> build(
        const HeightField& field, const TerrainMeshSpec& spec = {});
};

} // namespace genomes::terrain
