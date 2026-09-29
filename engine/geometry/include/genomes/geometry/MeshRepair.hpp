#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace genomes::geometry {

struct TriangleGroup final {
    std::uint32_t start{0U};
    std::uint32_t count{0U};
    std::uint16_t material{0U};
};

struct MeshRepairStats final {
    std::uint32_t removed_degenerate_triangles{0U};
    std::uint32_t repaired_normals{0U};
    std::uint32_t flipped_triangles{0U};
};

struct MeshRepairResult final {
    std::vector<foundation::Vec3> normals;
    std::vector<std::uint32_t> indices;
    std::vector<TriangleGroup> groups;
    MeshRepairStats stats{};
};

[[nodiscard]] foundation::Result<MeshRepairResult, foundation::Error> repairTriangleMesh(
    std::span<const foundation::Vec3> positions,
    std::span<const foundation::Vec3> source_normals,
    std::span<const std::uint32_t> source_indices,
    std::span<const TriangleGroup> source_groups = {});

} // namespace genomes::geometry
