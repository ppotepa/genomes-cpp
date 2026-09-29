#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace genomes::geometry {

// Index ranges describe separate draws. Never move a triangle across a range.
// Transparent/order-sensitive draws must set preserve_order.
struct IndexOptimizationRange final {
    std::size_t first_index{0};
    std::size_t index_count{0};
    bool preserve_order{true};
};

struct IndexOptimizationStats final {
    std::size_t triangle_count{0};
    std::size_t optimized_ranges{0};
    std::size_t preserved_ranges{0};
    std::size_t changed_triangles{0};
    bool optimizer_available{false};
};

struct IndexOptimizationResult final {
    std::vector<std::uint32_t> indices;
    IndexOptimizationStats stats;
};

[[nodiscard]] bool indexOptimizerAvailable() noexcept;
// Includes the pinned upstream and local policy versions; zero = identity path.
[[nodiscard]] std::uint64_t indexOptimizerFingerprint() noexcept;

// Pure CPU operation: changes only triangle order, never vertex IDs, winding,
// triangle membership, vertex count or any vertex-associated stream.
// Ranges must be a complete, ordered, triangle-aligned partition. A non-empty
// stream without explicit ranges is rejected rather than guessing materials.
[[nodiscard]] foundation::Result<IndexOptimizationResult, foundation::Error>
optimizeIndexOrder(std::span<const std::uint32_t> indices,
                   std::size_t vertex_count,
                   std::span<const IndexOptimizationRange> ranges);

} // namespace genomes::geometry
