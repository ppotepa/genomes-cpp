#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/math/Bounds.hpp>
#include <genomes/math/Vec.hpp>

#include <cstdint>
#include <vector>

namespace genomes::geometry {

enum class IndexFormat : std::uint8_t { UInt16, UInt32 };

struct SubmeshRange final {
    std::uint32_t first_index{0};
    std::uint32_t index_count{0};
    std::uint32_t material_index{0};
};

struct MeshVertex final {
    foundation::Vec3 position{};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
    foundation::Vec2 uv{};
};

struct MeshData final {
    // The legacy interleaved view remains source-compatible while migration is
    // in progress. New producers should fill the streams below.
    std::vector<MeshVertex> vertices;
    std::vector<foundation::Vec3> positions;
    std::vector<foundation::Vec3> normals;
    std::vector<math::Vec4> tangents;
    std::vector<foundation::Vec2> uvs;
    std::vector<foundation::Color> colors;
    std::vector<std::uint32_t> indices;
    IndexFormat index_format{IndexFormat::UInt32};
    std::vector<SubmeshRange> submeshes;
    math::Aabb bounds{};

    // Synchronizes the neutral SoA representation from the compatibility view.
    // It is explicit so validation remains read-only and deterministic.
    void rebuildStreams() noexcept;
    [[nodiscard]] bool hasStreams() const noexcept { return !positions.empty(); }

    [[nodiscard]] bool empty() const noexcept {
        return (vertices.empty() && positions.empty()) || indices.empty();
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
