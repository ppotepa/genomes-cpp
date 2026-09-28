#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace genomes::destruction {

inline constexpr std::uint32_t StructuralGraphVersion = 1;

struct StructuralNode final {
    foundation::StableId id{0};
    foundation::StableId component_id{0};
    float capacity{1.0F};
    bool anchored{false};
};

struct StructuralEdge final {
    foundation::StableId id{0};
    foundation::StableId from{0};
    foundation::StableId to{0};
    float capacity{1.0F};
};

struct StructuralNodeState final {
    foundation::StableId id{0};
    float damage{0.0F};
    bool supported{false};
    bool detached{false};
};

struct StructuralEdgeState final {
    foundation::StableId id{0};
    float damage{0.0F};
    bool failed{false};
};

enum class StructuralStateChangeKind : std::uint8_t {
    DetachComponent,
};

struct StructuralStateChange final {
    StructuralStateChangeKind kind{StructuralStateChangeKind::DetachComponent};
    foundation::StableId node_id{0};
    foundation::StableId component_id{0};
};

class StructuralGraph final {
public:
    [[nodiscard]] static foundation::Result<StructuralGraph, foundation::Error> create(
        std::vector<StructuralNode> nodes,
        std::vector<StructuralEdge> edges);

    [[nodiscard]] bool applyNodeDamage(foundation::StableId node_id,
                                       float normalized_damage) noexcept;
    [[nodiscard]] bool applyEdgeDamage(foundation::StableId edge_id,
                                       float normalized_damage) noexcept;

    // Reads committed node/edge damage and returns deterministic detach
    // commands. It never removes nodes or mutates render/physics state.
    [[nodiscard]] std::vector<StructuralStateChange> evaluateSupport();

    [[nodiscard]] std::uint32_t version() const noexcept { return version_; }
    [[nodiscard]] const std::vector<StructuralNode>& nodes() const noexcept { return nodes_; }
    [[nodiscard]] const std::vector<StructuralEdge>& edges() const noexcept { return edges_; }
    [[nodiscard]] const std::vector<StructuralNodeState>& nodeStates() const noexcept {
        return node_states_;
    }
    [[nodiscard]] const std::vector<StructuralEdgeState>& edgeStates() const noexcept {
        return edge_states_;
    }

private:
    [[nodiscard]] std::size_t nodeIndex(foundation::StableId id) const noexcept;
    [[nodiscard]] std::size_t edgeIndex(foundation::StableId id) const noexcept;

    std::uint32_t version_{StructuralGraphVersion};
    std::vector<StructuralNode> nodes_;
    std::vector<StructuralEdge> edges_;
    std::vector<StructuralNodeState> node_states_;
    std::vector<StructuralEdgeState> edge_states_;
    std::vector<std::uint32_t> adjacency_offsets_;
    std::vector<std::uint32_t> adjacency_edges_;
    bool support_evaluated_{false};
};

} // namespace genomes::destruction
