#pragma once

#include <genomes/foundation/Types.hpp>

#include <cstdint>
#include <vector>

namespace genomes::geometry {

struct MeshVertex final {
    foundation::Vec3 position{};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
    foundation::Vec2 uv{};
};

struct MeshData final {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;

    [[nodiscard]] bool empty() const noexcept {
        return vertices.empty() || indices.empty();
    }
    [[nodiscard]] bool valid() const noexcept;
};

struct MeshTransform final {
    foundation::Vec3 translation{};
    foundation::Vec3 scale{1.0F, 1.0F, 1.0F};
    float rotation_y{0.0F};
};

void appendTransformed(MeshData& destination, const MeshData& source,
                       const MeshTransform& transform = {});

} // namespace genomes::geometry
