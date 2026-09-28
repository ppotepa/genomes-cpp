#include <genomes/roads/RoadGraph.hpp>

#include <genomes/foundation/StableHash.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <utility>

namespace genomes::roads {

namespace {

[[nodiscard]] bool finite_vec(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] std::uint64_t hash_vec(std::uint64_t hash, foundation::Vec3 value) noexcept {
    hash = foundation::stableHashCombine(hash, std::bit_cast<std::uint32_t>(value.x));
    hash = foundation::stableHashCombine(hash, std::bit_cast<std::uint32_t>(value.y));
    return foundation::stableHashCombine(hash, std::bit_cast<std::uint32_t>(value.z));
}

[[nodiscard]] bool has_id(const std::vector<RoadNode>& nodes,
                          foundation::StableId id) noexcept {
    return std::any_of(nodes.begin(), nodes.end(),
                       [id](const RoadNode& node) { return node.id == id; });
}

[[nodiscard]] bool has_id(const std::vector<RoadEdge>& edges,
                          foundation::StableId id) noexcept {
    return std::any_of(edges.begin(), edges.end(),
                       [id](const RoadEdge& edge) { return edge.id == id; });
}

} // namespace

bool RoadCenterline::valid() const noexcept {
    if (!finite_vec(start) || !finite_vec(end) ||
        (start.x == end.x && start.z == end.z)) {
        return false;
    }
    if (kind == RoadCenterlineKind::Straight) {
        return points.empty();
    }
    if (points.empty()) {
        return false;
    }
    for (const foundation::Vec3 point : points) {
        if (!finite_vec(point)) {
            return false;
        }
    }
    return true;
}

std::vector<foundation::Vec3> RoadCenterline::samplePoints() const {
    std::vector<foundation::Vec3> result;
    if (kind == RoadCenterlineKind::Straight) {
        result.reserve(2);
        result.push_back(start);
        result.push_back(end);
        return result;
    }
    result.reserve(points.size() + 2);
    result.push_back(start);
    result.insert(result.end(), points.begin(), points.end());
    result.push_back(end);
    return result;
}

bool RoadEdge::valid() const noexcept {
    return id != 0 && from != 0 && to != 0 && from != to && std::isfinite(width) &&
           width > 0.0F && std::isfinite(speed_limit) && speed_limit >= 0.0F &&
           std::isfinite(capacity) && capacity >= 0.0F && centerline.valid();
}

const RoadNode* RoadGraph::findNode(foundation::StableId id) const noexcept {
    const auto iterator = std::lower_bound(
        nodes_.begin(), nodes_.end(), id,
        [](const RoadNode& node, foundation::StableId value) { return node.id < value; });
    return iterator != nodes_.end() && iterator->id == id ? &*iterator : nullptr;
}

const RoadEdge* RoadGraph::findEdge(foundation::StableId id) const noexcept {
    const auto iterator = std::lower_bound(
        edges_.begin(), edges_.end(), id,
        [](const RoadEdge& edge, foundation::StableId value) { return edge.id < value; });
    return iterator != edges_.end() && iterator->id == id ? &*iterator : nullptr;
}

std::size_t RoadGraph::degree(foundation::StableId id) const noexcept {
    const auto iterator = std::lower_bound(
        nodes_.begin(), nodes_.end(), id,
        [](const RoadNode& node, foundation::StableId value) { return node.id < value; });
    if (iterator == nodes_.end() || iterator->id != id) {
        return 0;
    }
    const std::size_t index = static_cast<std::size_t>(iterator - nodes_.begin());
    if (index + 1 >= adjacency_offsets_.size()) {
        return 0;
    }
    return adjacency_offsets_[index + 1] - adjacency_offsets_[index];
}

RoadGraph RoadGraph::translated(foundation::Vec3 offset) const {
    RoadGraph result = *this;
    for (RoadNode& node : result.nodes_) {
        node.position.x += offset.x;
        node.position.y += offset.y;
        node.position.z += offset.z;
    }
    for (RoadEdge& edge : result.edges_) {
        edge.centerline.start.x += offset.x;
        edge.centerline.start.y += offset.y;
        edge.centerline.start.z += offset.z;
        edge.centerline.end.x += offset.x;
        edge.centerline.end.y += offset.y;
        edge.centerline.end.z += offset.z;
        for (foundation::Vec3& point : edge.centerline.points) {
            point.x += offset.x;
            point.y += offset.y;
            point.z += offset.z;
        }
    }
    return result;
}

std::uint64_t RoadGraph::contentHash() const noexcept {
    std::uint64_t hash = foundation::stableHashU64(nodes_.size());
    for (const RoadNode& node : nodes_) {
        hash = foundation::stableHashCombine(hash, node.id);
        hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(node.type));
        hash = hash_vec(hash, node.position);
    }
    for (const RoadEdge& edge : edges_) {
        hash = foundation::stableHashCombine(hash, edge.id);
        hash = foundation::stableHashCombine(hash, edge.from);
        hash = foundation::stableHashCombine(hash, edge.to);
        hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(edge.road_class));
        hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(edge.surface));
        hash = foundation::stableHashCombine(hash, std::bit_cast<std::uint32_t>(edge.width));
        for (const foundation::Vec3 point : edge.centerline.samplePoints()) {
            hash = hash_vec(hash, point);
        }
    }
    return hash == 0 ? 1 : hash;
}

foundation::Result<void, foundation::Error> RoadGraphBuilder::addNode(RoadNode node) {
    if (node.id == 0 || !finite_vec(node.position) || has_id(nodes_, node.id)) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid or duplicate road node"});
    }
    nodes_.push_back(std::move(node));
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> RoadGraphBuilder::addEdge(RoadEdge edge) {
    if (!edge.valid() || has_id(edges_, edge.id) ||
        !has_id(nodes_, edge.from) || !has_id(nodes_, edge.to)) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid or duplicate road edge"});
    }
    const RoadNode* from = nullptr;
    const RoadNode* to = nullptr;
    for (const RoadNode& node : nodes_) {
        if (node.id == edge.from) {
            from = &node;
        } else if (node.id == edge.to) {
            to = &node;
        }
    }
    if (from == nullptr || to == nullptr || edge.centerline.start.x != from->position.x ||
        edge.centerline.start.y != from->position.y || edge.centerline.start.z != from->position.z ||
        edge.centerline.end.x != to->position.x || edge.centerline.end.y != to->position.y ||
        edge.centerline.end.z != to->position.z) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument,
             "road centerline endpoints must equal canonical node coordinates"});
    }
    edges_.push_back(std::move(edge));
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<RoadGraph, foundation::Error> RoadGraphBuilder::freeze() && {
    std::sort(nodes_.begin(), nodes_.end(),
              [](const RoadNode& left, const RoadNode& right) { return left.id < right.id; });
    std::sort(edges_.begin(), edges_.end(),
              [](const RoadEdge& left, const RoadEdge& right) { return left.id < right.id; });

    RoadGraph graph{};
    graph.nodes_ = std::move(nodes_);
    graph.edges_ = std::move(edges_);
    graph.adjacency_offsets_.assign(graph.nodes_.size() + 1, 0);
    for (const RoadEdge& edge : graph.edges_) {
        const auto from = std::lower_bound(
            graph.nodes_.begin(), graph.nodes_.end(), edge.from,
            [](const RoadNode& node, foundation::StableId value) { return node.id < value; });
        const auto to = std::lower_bound(
            graph.nodes_.begin(), graph.nodes_.end(), edge.to,
            [](const RoadNode& node, foundation::StableId value) { return node.id < value; });
        if (from == graph.nodes_.end() || to == graph.nodes_.end() || from->id != edge.from ||
            to->id != edge.to) {
            return foundation::Result<RoadGraph, foundation::Error>::failure(
                {foundation::ErrorCode::Internal, "road graph lost an edge endpoint"});
        }
        ++graph.adjacency_offsets_[static_cast<std::size_t>(from - graph.nodes_.begin()) + 1];
        ++graph.adjacency_offsets_[static_cast<std::size_t>(to - graph.nodes_.begin()) + 1];
    }
    for (std::size_t index = 1; index < graph.adjacency_offsets_.size(); ++index) {
        graph.adjacency_offsets_[index] += graph.adjacency_offsets_[index - 1];
    }
    graph.adjacency_.resize(graph.adjacency_offsets_.back());
    std::vector<std::uint32_t> cursors = graph.adjacency_offsets_;
    for (const RoadEdge& edge : graph.edges_) {
        const auto from = static_cast<std::size_t>(std::lower_bound(
            graph.nodes_.begin(), graph.nodes_.end(), edge.from,
            [](const RoadNode& node, foundation::StableId value) { return node.id < value; }) -
            graph.nodes_.begin());
        const auto to = static_cast<std::size_t>(std::lower_bound(
            graph.nodes_.begin(), graph.nodes_.end(), edge.to,
            [](const RoadNode& node, foundation::StableId value) { return node.id < value; }) -
            graph.nodes_.begin());
        graph.adjacency_[cursors[from]++] = edge.id;
        graph.adjacency_[cursors[to]++] = edge.id;
    }
    for (std::size_t index = 0; index < graph.nodes_.size(); ++index) {
        auto begin = graph.adjacency_.begin() + graph.adjacency_offsets_[index];
        auto end = graph.adjacency_.begin() + graph.adjacency_offsets_[index + 1];
        std::sort(begin, end);
    }
    return foundation::Result<RoadGraph, foundation::Error>::success(std::move(graph));
}

} // namespace genomes::roads
