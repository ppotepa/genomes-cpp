#include <genomes/geometry/IndexOrderOptimizer.hpp>
#include <genomes/foundation/StableHash.hpp>

#if GENOMES_HAS_MESHOPTIMIZER
#include <meshoptimizer.h>
#endif

#include <limits>
#include <utility>

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
    return foundation::stable_id(
        "meshoptimizer.9e1f07b159d3cb777f1c67ed31fc11fd117986f4.index-order.v1");
#else
    return 0U;
#endif
}

foundation::Result<IndexOptimizationResult, foundation::Error> optimizeIndexOrder(
    std::span<const std::uint32_t> indices,
    std::size_t vertex_count,
    std::span<const IndexOptimizationRange> ranges) {
    using Result = foundation::Result<IndexOptimizationResult, foundation::Error>;
    if (vertex_count > std::numeric_limits<std::uint32_t>::max() ||
        indices.size() % 3U != 0U) {
        return Result::failure({foundation::ErrorCode::InvalidArgument,
                                "invalid triangle stream for index optimization"});
    }
    for (const auto index : indices) {
        if (index >= vertex_count) {
            return Result::failure({foundation::ErrorCode::OutOfRange,
                                    "index optimizer vertex is out of range"});
        }
    }
    std::size_t covered = 0U;
    for (const auto& range : ranges) {
        if (range.first_index != covered || range.index_count == 0U ||
            range.index_count % 3U != 0U ||
            range.index_count > indices.size() - covered) {
            return Result::failure({foundation::ErrorCode::InvalidArgument,
                                    "index ranges must partition complete triangles"});
        }
        covered += range.index_count;
    }
    if (covered != indices.size()) {
        return Result::failure({foundation::ErrorCode::InvalidArgument,
                                "index ranges do not cover the mesh"});
    }

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
        {
            // The upstream C API uses unsigned int. Copying explicitly avoids
            // aliasing assumptions about std::uint32_t on another platform.
            const auto input = indices.subspan(range.first_index, range.index_count);
            std::vector<unsigned int> scratch(input.begin(), input.end());
            meshopt_optimizeVertexCache(scratch.data(), scratch.data(),
                                         scratch.size(), vertex_count);
            for (std::size_t index = 0U; index < scratch.size(); ++index) {
                result.indices[range.first_index + index] =
                    static_cast<std::uint32_t>(scratch[index]);
            }
            ++result.stats.optimized_ranges;
            continue;
        }
#endif
    }
    for (std::size_t index = 0U; index < indices.size(); index += 3U) {
        if (indices[index] != result.indices[index] ||
            indices[index + 1U] != result.indices[index + 1U] ||
            indices[index + 2U] != result.indices[index + 2U]) {
            ++result.stats.changed_triangles;
        }
    }
    return Result::success(std::move(result));
}

} // namespace genomes::geometry
