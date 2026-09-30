#pragma once

#include <genomes/geometry/IndexOrderOptimizer.hpp>

namespace genomes::geometry::detail {

[[nodiscard]] foundation::Result<IndexOptimizationResult, foundation::Error>
optimizeIndexOrderCore(std::span<const std::uint32_t> indices,
                       std::size_t vertex_count,
                       std::span<const IndexOptimizationRange> ranges);

} // namespace genomes::geometry::detail
