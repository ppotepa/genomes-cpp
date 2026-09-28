#include <genomes/terrain/TerrainMesh.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace genomes::terrain {

foundation::Result<TerrainMesh, foundation::Error> TerrainMeshBuilder::build(
    const HeightField& field, const TerrainMeshSpec& spec) {
    if (!spec.valid()) {
        return foundation::Result<TerrainMesh, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid terrain mesh specification"});
    }
    if (field.width() < 2 || field.height() < 2) {
        return foundation::Result<TerrainMesh, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "height field is too small for a mesh"});
    }

    const std::uint32_t output_width =
        (field.width() - 1) / spec.sample_step + 1;
    const std::uint32_t output_height =
        (field.height() - 1) / spec.sample_step + 1;
    const std::uint64_t vertex_count = static_cast<std::uint64_t>(output_width) *
                                       static_cast<std::uint64_t>(output_height);
    const std::uint64_t cell_count = static_cast<std::uint64_t>(output_width - 1) *
                                     static_cast<std::uint64_t>(output_height - 1);
    const std::uint64_t index_count = cell_count * 6U;
    if (vertex_count > spec.max_vertices || index_count > spec.max_indices ||
        vertex_count > std::numeric_limits<std::uint32_t>::max()) {
        return foundation::Result<TerrainMesh, foundation::Error>::failure(
            {foundation::ErrorCode::OutOfRange, "terrain mesh exceeds its vertex budget"});
    }

    TerrainMesh mesh{};
    mesh.source_width = field.width();
    mesh.source_height = field.height();
    mesh.sample_step = spec.sample_step;
    mesh.vertices.reserve(static_cast<std::size_t>(vertex_count));
    mesh.indices.reserve(static_cast<std::size_t>(index_count));

    for (std::uint32_t z = 0; z < output_height; ++z) {
        const std::uint32_t source_z = std::min(
            z * spec.sample_step, field.height() - 1);
        for (std::uint32_t x = 0; x < output_width; ++x) {
            const std::uint32_t source_x = std::min(
                x * spec.sample_step, field.width() - 1);
            const double world_x = field.originX() +
                                   static_cast<double>(source_x) * field.cellSize();
            const double world_z = field.originZ() +
                                   static_cast<double>(source_z) * field.cellSize();
            const float u = output_width > 1
                                ? static_cast<float>(x) /
                                      static_cast<float>(output_width - 1)
                                : 0.0F;
            const float v = output_height > 1
                                ? static_cast<float>(z) /
                                      static_cast<float>(output_height - 1)
                                : 0.0F;
            mesh.vertices.push_back({
                {static_cast<float>(world_x), field.at(source_x, source_z),
                 static_cast<float>(world_z)},
                field.normal(world_x, world_z),
                {u, v}});
        }
    }

    for (std::uint32_t z = 0; z + 1 < output_height; ++z) {
        for (std::uint32_t x = 0; x + 1 < output_width; ++x) {
            const std::uint32_t top_left = z * output_width + x;
            const std::uint32_t top_right = top_left + 1;
            const std::uint32_t bottom_left = (z + 1) * output_width + x;
            const std::uint32_t bottom_right = bottom_left + 1;
            mesh.indices.insert(mesh.indices.end(),
                                {top_left, bottom_left, top_right,
                                 top_right, bottom_left, bottom_right});
        }
    }

    return foundation::Result<TerrainMesh, foundation::Error>::success(std::move(mesh));
}

} // namespace genomes::terrain
