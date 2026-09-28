#pragma once

#include <genomes/world/WorldQuerySnapshot.hpp>

#include <memory>
#include <span>
#include <vector>

namespace genomes::world {

class WorldQueryService final {
public:
    [[nodiscard]] foundation::Result<void, foundation::Error> publish(
        WorldQuerySnapshot snapshot);
    [[nodiscard]] const WorldQuerySnapshot* snapshot() const noexcept { return snapshot_.get(); }

private:
    std::unique_ptr<WorldQuerySnapshot> snapshot_;
};

// Batch requests preserve input order.  The snapshot remains immutable while
// a batch is evaluated, so callers can safely partition requests across jobs.
struct QuerySegmentRequest final {
    foundation::Vec3 origin{};
    foundation::Vec3 end{};
};

[[nodiscard]] std::vector<QuerySegmentResult> querySegments(
    const WorldQuerySnapshot&, std::span<const QuerySegmentRequest>, bool stable_order = true);

} // namespace genomes::world
