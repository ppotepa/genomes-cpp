#include <genomes/geometry/MeshOptimizer.hpp>
#include <genomes/geometry/MeshOptimizerDetail.hpp>
#if GENOMES_HAS_MESHOPTIMIZER
#include <meshoptimizer.h>
#endif

#include <limits>
#include <utility>
#include <vector>

namespace genomes::geometry {
namespace detail {
foundation::Result<IndexOptimizationResult, foundation::Error> optimizeIndexOrderCore(
    std::span<const std::uint32_t> indices,
    std::size_t vertex_count,
    std::span<const IndexOptimizationRange> ranges) {
    using Result = foundation::Result<IndexOptimizationResult, foundation::Error>;
    if (vertex_count > std::numeric_limits<std::uint32_t>::max() || indices.size() % 3U != 0U)
        return Result::failure({foundation::ErrorCode::InvalidArgument, "invalid triangle stream for index optimization"});
    for (const auto index : indices)
        if (index >= vertex_count) return Result::failure({foundation::ErrorCode::OutOfRange, "index optimizer vertex is out of range"});
    std::size_t covered = 0U;
    for (const auto& range : ranges) {
        if (range.first_index != covered || range.index_count == 0U || range.index_count % 3U != 0U || range.index_count > indices.size() - covered)
            return Result::failure({foundation::ErrorCode::InvalidArgument, "index ranges must partition complete triangles"});
        covered += range.index_count;
    }
    if (covered != indices.size()) return Result::failure({foundation::ErrorCode::InvalidArgument, "index ranges do not cover the mesh"});
    IndexOptimizationResult result;
    result.indices.assign(indices.begin(), indices.end());
    result.stats.triangle_count = indices.size() / 3U;
    result.stats.optimizer_available = indexOptimizerAvailable();
    for (const auto& range : ranges) {
        if (range.preserve_order || range.index_count <= 3U || !indexOptimizerAvailable()) {
            ++result.stats.preserved_ranges;
            continue;
        }
#if GENOMES_HAS_MESHOPTIMIZER
        const auto input = indices.subspan(range.first_index, range.index_count);
        std::vector<unsigned int> scratch(input.begin(), input.end());
        meshopt_optimizeVertexCache(scratch.data(), scratch.data(), scratch.size(), vertex_count);
        for (std::size_t index = 0U; index < scratch.size(); ++index)
            result.indices[range.first_index + index] = static_cast<std::uint32_t>(scratch[index]);
        ++result.stats.optimized_ranges;
#endif
    }
    for (std::size_t index = 0U; index < indices.size(); index += 3U)
        if (indices[index] != result.indices[index] || indices[index + 1U] != result.indices[index + 1U] || indices[index + 2U] != result.indices[index + 2U])
            ++result.stats.changed_triangles;
    return Result::success(std::move(result));
}
} // namespace detail

foundation::Result<MeshOptimizationResult, foundation::Error> optimizeMesh(const MeshData& source, OptimizationPolicy policy) {
    using Result=foundation::Result<MeshOptimizationResult, foundation::Error>;
    if(!source.valid()) return Result::failure({foundation::ErrorCode::InvalidArgument,"cannot optimize invalid mesh"});
    std::vector<IndexOptimizationRange> ranges; ranges.reserve(source.submeshes.size());
    if(source.submeshes.empty()) ranges.push_back({0U,source.indices.size(),true});
    else for(const auto range:source.submeshes) ranges.push_back({range.first_index,range.index_count,true});
    auto optimized=optimizeIndexOrder(source.indices,source.vertices.empty()?source.positions.size():source.vertices.size(),ranges);
    if(!optimized) return Result::failure(optimized.error());
    MeshOptimizationResult result{}; result.mesh=source; result.mesh.indices=std::move(optimized.value().indices); result.report.index_stats=optimized.value().stats;
    // Vertex remapping is deliberately disabled until all optional streams and
    // skinned attributes have a proven complete remap implementation.
    result.report.vertices_remapped=false;
    (void)policy;
    return Result::success(std::move(result));
}
}
