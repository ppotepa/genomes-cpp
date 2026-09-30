#include <genomes/geometry/MeshOptimizer.hpp>
#include <utility>
#include <vector>

namespace genomes::geometry {
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
