#include <genomes/navigation/NavigationWorld.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <utility>

namespace genomes::navigation {

namespace {

struct OpenNode final {
    std::uint32_t cell{0};
    float score{0.0F};
    float heuristic{0.0F};
};

struct OpenNodeCompare final {
    [[nodiscard]] bool operator()(const OpenNode& left, const OpenNode& right) const noexcept {
        if (left.score != right.score) {
            return left.score > right.score;
        }
        if (left.heuristic != right.heuristic) {
            return left.heuristic > right.heuristic;
        }
        return left.cell > right.cell;
    }
};

} // namespace

bool PathRequest::valid() const noexcept {
    return std::isfinite(start.x) && std::isfinite(start.y) && std::isfinite(start.z) &&
           std::isfinite(goal.x) && std::isfinite(goal.y) && std::isfinite(goal.z) &&
           max_nodes > 0;
}

bool NavGridSpec::valid() const noexcept {
    return width > 0 && height > 0 && width <= 4096 && height <= 4096 &&
           std::isfinite(cell_size) && cell_size > 0.0F && std::isfinite(origin.x) &&
           std::isfinite(origin.y) && std::isfinite(origin.z);
}

GridNavigationWorld::GridNavigationWorld(NavGridSpec spec) : spec_{spec}, valid_{spec.valid()} {
    if (valid_) {
        blocked_.assign(static_cast<std::size_t>(spec.width) * spec.height, 0);
        blocked_count_ = 0;
    }
}

bool GridNavigationWorld::setBlocked(std::uint32_t x, std::uint32_t z, bool blocked) noexcept {
    if (!valid_ || x >= spec_.width || z >= spec_.height) {
        return false;
    }
    const std::size_t cell = index(x, z);
    const bool was_blocked = blocked_[cell] != 0;
    if (was_blocked == blocked) {
        return true;
    }
    blocked_[cell] = blocked ? 1u : 0u;
    if (blocked) {
        ++blocked_count_;
    } else {
        --blocked_count_;
    }
    return true;
}

bool GridNavigationWorld::isBlocked(std::uint32_t x, std::uint32_t z) const noexcept {
    return !valid_ || x >= spec_.width || z >= spec_.height || blocked_[index(x, z)] != 0;
}

foundation::Result<PathResult, foundation::Error> GridNavigationWorld::findPath(
    const PathRequest& request) const {
    if (!valid_) {
        return foundation::Result<PathResult, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "navigation grid is invalid"});
    }
    if (!request.valid()) {
        return foundation::Result<PathResult, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid navigation path request"});
    }

    std::uint32_t start_x = 0;
    std::uint32_t start_z = 0;
    std::uint32_t goal_x = 0;
    std::uint32_t goal_z = 0;
    if (!toCell(request.start, start_x, start_z) || !toCell(request.goal, goal_x, goal_z)) {
        return foundation::Result<PathResult, foundation::Error>::failure(
            {foundation::ErrorCode::OutOfRange, "navigation request is outside the grid"});
    }

    const std::uint32_t start = index(start_x, start_z);
    const std::uint32_t goal = index(goal_x, goal_z);
    PathResult result{};
    if (isBlocked(start_x, start_z) || isBlocked(goal_x, goal_z)) {
        result.status = PathStatus::NoPath;
        return foundation::Result<PathResult, foundation::Error>::success(std::move(result));
    }
    if (start == goal) {
        result.status = PathStatus::Complete;
        result.points.push_back(toWorld(start_x, start_z));
        return foundation::Result<PathResult, foundation::Error>::success(std::move(result));
    }
    if (blocked_count_ == 0) {
        result.status = PathStatus::Complete;
        result.points = {toWorld(start_x, start_z), toWorld(goal_x, goal_z)};
        return foundation::Result<PathResult, foundation::Error>::success(std::move(result));
    }

    const std::size_t cell_count = static_cast<std::size_t>(spec_.width) * spec_.height;
    const float infinity = std::numeric_limits<float>::infinity();
    std::vector<float> cost(cell_count, infinity);
    std::vector<std::uint32_t> parent(cell_count,
                                      std::numeric_limits<std::uint32_t>::max());
    std::vector<std::uint8_t> closed(cell_count, 0);
    std::priority_queue<OpenNode, std::vector<OpenNode>, OpenNodeCompare> open;
    const auto heuristic = [this, goal_x, goal_z](std::uint32_t x, std::uint32_t z) {
        const float dx = static_cast<float>(x > goal_x ? x - goal_x : goal_x - x);
        const float dz = static_cast<float>(z > goal_z ? z - goal_z : goal_z - z);
        return (dx + dz) * spec_.cell_size;
    };
    cost[start] = 0.0F;
    open.push({start, heuristic(start_x, start_z), heuristic(start_x, start_z)});

    constexpr int directions[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    while (!open.empty()) {
        const OpenNode current = open.top();
        open.pop();
        if (closed[current.cell] != 0) {
            continue;
        }
        closed[current.cell] = 1;
        ++result.expanded_nodes;
        if (result.expanded_nodes > request.max_nodes) {
            result.status = PathStatus::NodeBudgetExceeded;
            return foundation::Result<PathResult, foundation::Error>::success(std::move(result));
        }
        if (current.cell == goal) {
            std::vector<std::uint32_t> cells;
            for (std::uint32_t cell = goal;; cell = parent[cell]) {
                cells.push_back(cell);
                if (cell == start) {
                    break;
                }
            }
            std::reverse(cells.begin(), cells.end());
            result.points.reserve(cells.size());
            for (const std::uint32_t cell : cells) {
                result.points.push_back(toWorld(cell % spec_.width, cell / spec_.width));
            }
            result.status = PathStatus::Complete;
            return foundation::Result<PathResult, foundation::Error>::success(std::move(result));
        }

        const std::uint32_t current_x = current.cell % spec_.width;
        const std::uint32_t current_z = current.cell / spec_.width;
        for (const auto& direction : directions) {
            const int next_x = static_cast<int>(current_x) + direction[0];
            const int next_z = static_cast<int>(current_z) + direction[1];
            if (next_x < 0 || next_z < 0 || next_x >= static_cast<int>(spec_.width) ||
                next_z >= static_cast<int>(spec_.height)) {
                continue;
            }
            const auto x = static_cast<std::uint32_t>(next_x);
            const auto z = static_cast<std::uint32_t>(next_z);
            const std::uint32_t next = index(x, z);
            if (closed[next] != 0 || isBlocked(x, z)) {
                continue;
            }
            const float next_cost = cost[current.cell] + spec_.cell_size;
            if (next_cost >= cost[next]) {
                continue;
            }
            cost[next] = next_cost;
            parent[next] = current.cell;
            const float h = heuristic(x, z);
            open.push({next, next_cost + h, h});
        }
    }

    result.status = PathStatus::NoPath;
    return foundation::Result<PathResult, foundation::Error>::success(std::move(result));
}

bool GridNavigationWorld::toCell(foundation::Vec3 position,
                                  std::uint32_t& x,
                                  std::uint32_t& z) const noexcept {
    const float local_x = (position.x - spec_.origin.x) / spec_.cell_size;
    const float local_z = (position.z - spec_.origin.z) / spec_.cell_size;
    if (!std::isfinite(local_x) || !std::isfinite(local_z) || local_x < 0.0F ||
        local_z < 0.0F || local_x >= static_cast<float>(spec_.width) ||
        local_z >= static_cast<float>(spec_.height)) {
        return false;
    }
    x = static_cast<std::uint32_t>(local_x);
    z = static_cast<std::uint32_t>(local_z);
    return true;
}

foundation::Vec3 GridNavigationWorld::toWorld(std::uint32_t x, std::uint32_t z) const noexcept {
    return {spec_.origin.x + (static_cast<float>(x) + 0.5F) * spec_.cell_size, spec_.origin.y,
            spec_.origin.z + (static_cast<float>(z) + 0.5F) * spec_.cell_size};
}

} // namespace genomes::navigation
