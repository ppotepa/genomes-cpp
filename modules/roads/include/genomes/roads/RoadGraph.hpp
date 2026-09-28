#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/roads/RoadTypes.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace genomes::roads {

class RoadGraph final {
public:
    RoadGraph() = default;

    [[nodiscard]] const std::vector<RoadNode>& nodes() const noexcept { return nodes_; }
    [[nodiscard]] const std::vector<RoadEdge>& edges() const noexcept { return edges_; }
    [[nodiscard]] const std::vector<foundation::StableId>& adjacency() const noexcept {
        return adjacency_;
    }
    [[nodiscard]] const std::vector<std::uint32_t>& adjacencyOffsets() const noexcept {
        return adjacency_offsets_;
    }

    [[nodiscard]] const RoadNode* findNode(foundation::StableId id) const noexcept;
    [[nodiscard]] const RoadEdge* findEdge(foundation::StableId id) const noexcept;
    [[nodiscard]] std::size_t degree(foundation::StableId id) const noexcept;
    [[nodiscard]] bool empty() const noexcept { return nodes_.empty() && edges_.empty(); }

    // Region streaming translates an immutable graph without changing its
    // semantic IDs or edge topology.
    [[nodiscard]] RoadGraph translated(foundation::Vec3 offset) const;

    [[nodiscard]] std::uint64_t contentHash() const noexcept;

private:
    friend class RoadGraphBuilder;

    std::vector<RoadNode> nodes_;
    std::vector<RoadEdge> edges_;
    std::vector<foundation::StableId> adjacency_;
    std::vector<std::uint32_t> adjacency_offsets_;
};

class RoadGraphBuilder final {
public:
    [[nodiscard]] foundation::Result<void, foundation::Error> addNode(RoadNode node);
    [[nodiscard]] foundation::Result<void, foundation::Error> addEdge(RoadEdge edge);

    [[nodiscard]] foundation::Result<RoadGraph, foundation::Error> freeze() &&;

private:
    std::vector<RoadNode> nodes_;
    std::vector<RoadEdge> edges_;
};

} // namespace genomes::roads
