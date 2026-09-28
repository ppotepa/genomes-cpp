#include <genomes/destruction/StructuralGraph.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace genomes::destruction {

namespace {

[[nodiscard]] bool valid_node(const StructuralNode& node) noexcept {
    return node.id != 0 && node.component_id != 0 && std::isfinite(node.capacity) &&
           node.capacity > 0.0F;
}

[[nodiscard]] bool valid_edge(const StructuralEdge& edge) noexcept {
    return edge.id != 0 && edge.from != 0 && edge.to != 0 && edge.from != edge.to &&
           std::isfinite(edge.capacity) && edge.capacity > 0.0F;
}

} // namespace

foundation::Result<StructuralGraph, foundation::Error> StructuralGraph::create(
    std::vector<StructuralNode> nodes,
    std::vector<StructuralEdge> edges) {
    if (nodes.empty()) {
        return foundation::Result<StructuralGraph, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "structural graph has no nodes"});
    }
    if (!std::all_of(nodes.begin(), nodes.end(), valid_node) ||
        !std::all_of(edges.begin(), edges.end(), valid_edge)) {
        return foundation::Result<StructuralGraph, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid structural graph element"});
    }
    std::sort(nodes.begin(), nodes.end(),
              [](const StructuralNode& left, const StructuralNode& right) {
                  return left.id < right.id;
              });
    std::sort(edges.begin(), edges.end(),
              [](const StructuralEdge& left, const StructuralEdge& right) {
                  return left.id < right.id;
              });
    const auto duplicate_nodes = std::adjacent_find(
        nodes.begin(), nodes.end(), [](const StructuralNode& left, const StructuralNode& right) {
            return left.id == right.id;
        });
    const auto duplicate_edges = std::adjacent_find(
        edges.begin(), edges.end(), [](const StructuralEdge& left, const StructuralEdge& right) {
            return left.id == right.id;
        });
    if (duplicate_nodes != nodes.end() || duplicate_edges != edges.end()) {
        return foundation::Result<StructuralGraph, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "duplicate structural graph identity"});
    }

    StructuralGraph graph;
    graph.nodes_ = std::move(nodes);
    graph.edges_ = std::move(edges);
    graph.node_states_.reserve(graph.nodes_.size());
    for (const StructuralNode& node : graph.nodes_) {
        graph.node_states_.push_back({node.id, 0.0F, false, false});
    }
    graph.edge_states_.reserve(graph.edges_.size());
    graph.adjacency_offsets_.assign(graph.nodes_.size() + 1U, 0U);
    for (const StructuralEdge& edge : graph.edges_) {
        const std::size_t from = graph.nodeIndex(edge.from);
        const std::size_t to = graph.nodeIndex(edge.to);
        if (from == graph.nodes_.size() || to == graph.nodes_.size()) {
            return foundation::Result<StructuralGraph, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument, "structural edge references missing node"});
        }
        ++graph.adjacency_offsets_[from + 1U];
        ++graph.adjacency_offsets_[to + 1U];
        graph.edge_states_.push_back({edge.id, 0.0F, false});
    }
    for (std::size_t index = 1; index < graph.adjacency_offsets_.size(); ++index) {
        graph.adjacency_offsets_[index] += graph.adjacency_offsets_[index - 1U];
    }
    graph.adjacency_edges_.assign(graph.adjacency_offsets_.back(), 0U);
    std::vector<std::uint32_t> cursors = graph.adjacency_offsets_;
    for (std::uint32_t edge_index = 0; edge_index < graph.edges_.size(); ++edge_index) {
        const StructuralEdge& edge = graph.edges_[edge_index];
        const std::size_t from = graph.nodeIndex(edge.from);
        const std::size_t to = graph.nodeIndex(edge.to);
        graph.adjacency_edges_[cursors[from]++] = edge_index;
        graph.adjacency_edges_[cursors[to]++] = edge_index;
    }
    return foundation::Result<StructuralGraph, foundation::Error>::success(std::move(graph));
}

bool StructuralGraph::applyNodeDamage(foundation::StableId node_id,
                                      float normalized_damage) noexcept {
    if (!std::isfinite(normalized_damage) || normalized_damage <= 0.0F) {
        return false;
    }
    const std::size_t index = nodeIndex(node_id);
    if (index == nodes_.size() || node_states_[index].detached) {
        return false;
    }
    node_states_[index].damage = std::clamp(node_states_[index].damage + normalized_damage,
                                             0.0F, 1.0F);
    return true;
}

bool StructuralGraph::applyEdgeDamage(foundation::StableId edge_id,
                                      float normalized_damage) noexcept {
    if (!std::isfinite(normalized_damage) || normalized_damage <= 0.0F) {
        return false;
    }
    const std::size_t index = edgeIndex(edge_id);
    if (index == edges_.size() || edge_states_[index].failed) {
        return false;
    }
    edge_states_[index].damage = std::clamp(edge_states_[index].damage + normalized_damage,
                                             0.0F, 1.0F);
    edge_states_[index].failed = edge_states_[index].damage >= 1.0F;
    return true;
}

std::vector<StructuralStateChange> StructuralGraph::evaluateSupport() {
    std::vector<StructuralStateChange> changes;
    std::vector<bool> supported(nodes_.size(), false);
    for (std::size_t index = 0; index < nodes_.size(); ++index) {
        const StructuralNode& node = nodes_[index];
        if (node.anchored && node_states_[index].damage < 1.0F &&
            !node_states_[index].detached) {
            supported[index] = true;
        }
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (std::size_t node_index = 0; node_index < nodes_.size(); ++node_index) {
            if (supported[node_index] || node_states_[node_index].damage >= 1.0F ||
                node_states_[node_index].detached) {
                continue;
            }
            for (std::uint32_t cursor = adjacency_offsets_[node_index];
                 cursor < adjacency_offsets_[node_index + 1U]; ++cursor) {
                const std::uint32_t edge_index = adjacency_edges_[cursor];
                const StructuralEdgeState& edge_state = edge_states_[edge_index];
                if (edge_state.failed || edge_state.damage >= 1.0F) {
                    continue;
                }
                const StructuralEdge& edge = edges_[edge_index];
                const std::size_t other = nodeIndex(edge.from == nodes_[node_index].id
                                                         ? edge.to
                                                         : edge.from);
                if (other < nodes_.size() && supported[other]) {
                    supported[node_index] = true;
                    changed = true;
                    break;
                }
            }
        }
    }
    for (std::size_t index = 0; index < nodes_.size(); ++index) {
        StructuralNodeState& state = node_states_[index];
        if (support_evaluated_ && state.supported && !supported[index] && !state.detached) {
            state.detached = true;
            changes.push_back({StructuralStateChangeKind::DetachComponent,
                               state.id, nodes_[index].component_id});
        }
        state.supported = supported[index];
    }
    support_evaluated_ = true;
    return changes;
}

std::size_t StructuralGraph::nodeIndex(foundation::StableId id) const noexcept {
    const auto iterator = std::lower_bound(
        nodes_.begin(), nodes_.end(), id,
        [](const StructuralNode& node, foundation::StableId value) { return node.id < value; });
    return iterator == nodes_.end() || iterator->id != id
               ? nodes_.size()
               : static_cast<std::size_t>(iterator - nodes_.begin());
}

std::size_t StructuralGraph::edgeIndex(foundation::StableId id) const noexcept {
    const auto iterator = std::lower_bound(
        edges_.begin(), edges_.end(), id,
        [](const StructuralEdge& edge, foundation::StableId value) { return edge.id < value; });
    return iterator == edges_.end() || iterator->id != id
               ? edges_.size()
               : static_cast<std::size_t>(iterator - edges_.begin());
}

} // namespace genomes::destruction
