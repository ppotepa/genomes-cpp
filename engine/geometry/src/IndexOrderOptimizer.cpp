#include <genomes/geometry/IndexOrderOptimizer.hpp>
#include <genomes/geometry/MeshOptimizerDetail.hpp>
#include <genomes/foundation/StableHash.hpp>

namespace genomes::geometry {

bool indexOptimizerAvailable() noexcept {
#if GENOMES_HAS_MESHOPTIMIZER
    return true;
#else
    return false;
#endif
}

std::uint64_t indexOptimizerFingerprint() noexcept {
#if GENOMES_HAS_MESHOPTIMIZER
    return foundation::stableHashString(
        "meshoptimizer.9e1f07b159d3cb777f1c67ed31fc11fd117986f4.index-order.v1");
#else
    return 0U;
#endif
}

foundation::Result<IndexOptimizationResult, foundation::Error> optimizeIndexOrder(
    std::span<const std::uint32_t> indices,
    std::size_t vertex_count,
    std::span<const IndexOptimizationRange> ranges) {
    return detail::optimizeIndexOrderCore(indices, vertex_count, ranges);
}

} // namespace genomes::geometry
