#include <genomes/navigation/FlowField.hpp>

#include <algorithm>
#include <limits>
#include <queue>
#include <string_view>

namespace genomes::navigation {

namespace {

constexpr std::uint32_t kMaxCells = 16U * 1024U * 1024U;
constexpr std::uint32_t kInfinity = std::numeric_limits<std::uint32_t>::max();
constexpr int kDirections[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

struct QueueEntry final {
    std::uint32_t cost{0U};
    std::uint32_t cell{0U};
};

struct QueueCompare final {
    [[nodiscard]] bool operator()(const QueueEntry& left,
                                  const QueueEntry& right) const noexcept {
        if (left.cost != right.cost) {
            return left.cost > right.cost;
        }
        return left.cell > right.cell;
    }
};

[[nodiscard]] foundation::Error error(foundation::ErrorCode code,
                                       std::string_view message) noexcept {
    return foundation::Error{code, message};
}

} // namespace

bool FlowFieldDescriptor::valid() const noexcept {
    const auto cell_count = static_cast<std::uint64_t>(key.width) * key.height;
    if (key.navigation_revision == 0U || key.cost_profile == 0U || key.goal_set_hash == 0U ||
        key.width == 0U || key.height == 0U || cell_count > kMaxCells || goal_cells.empty()) {
        return false;
    }
    for (std::size_t index = 0U; index < goal_cells.size(); ++index) {
        if (goal_cells[index] >= cell_count) {
            return false;
        }
        for (std::size_t other = index + 1U; other < goal_cells.size(); ++other) {
            if (goal_cells[index] == goal_cells[other]) {
                return false;
            }
        }
    }
    return key.goal_set_hash != 0U;
}

FlowSample FlowField::sample(std::uint32_t x, std::uint32_t z) const noexcept {
    if (x >= width() || z >= height()) {
        return {};
    }
    const auto cell = z * width() + x;
    if (blocked_[cell] != 0U || integration_costs_[cell] == kInfinity) {
        return {};
    }
    return FlowSample{integration_costs_[cell], directions_[cell], true, goals_[cell] != 0U};
}

foundation::Result<FlowField, foundation::Error> FlowFieldBuilder::build(
    const FlowFieldDescriptor& descriptor, std::span<const std::uint8_t> blocked,
    std::span<const std::uint32_t> traversal_cost) {
    if (!descriptor.valid()) {
        return foundation::Result<FlowField, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument, "invalid flow field descriptor"));
    }
    const auto cell_count = static_cast<std::size_t>(descriptor.key.width) * descriptor.key.height;
    if (blocked.size() != cell_count && !blocked.empty()) {
        return foundation::Result<FlowField, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument, "blocked grid size mismatch"));
    }
    if (!traversal_cost.empty() && traversal_cost.size() != cell_count) {
        return foundation::Result<FlowField, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument, "traversal cost size mismatch"));
    }

    FlowField result{};
    result.descriptor_ = descriptor;
    result.blocked_.assign(cell_count, 0U);
    if (!blocked.empty()) {
        result.blocked_.assign(blocked.begin(), blocked.end());
    }
    result.integration_costs_.assign(cell_count, kInfinity);
    result.directions_.assign(cell_count, FlowDirection{});
    result.goals_.assign(cell_count, 0U);

    std::priority_queue<QueueEntry, std::vector<QueueEntry>, QueueCompare> queue;
    for (const auto goal : descriptor.goal_cells) {
        if (result.blocked_[goal] != 0U) {
            continue;
        }
        result.goals_[goal] = 1U;
        if (result.integration_costs_[goal] == 0U) {
            continue;
        }
        result.integration_costs_[goal] = 0U;
        queue.push(QueueEntry{0U, goal});
    }
    if (queue.empty()) {
        return foundation::Result<FlowField, foundation::Error>::success(std::move(result));
    }

    const auto width = descriptor.key.width;
    const auto height = descriptor.key.height;
    while (!queue.empty()) {
        const auto current = queue.top();
        queue.pop();
        if (current.cost != result.integration_costs_[current.cell]) {
            continue;
        }
        const auto x = current.cell % width;
        const auto z = current.cell / width;
        for (const auto& direction : kDirections) {
            const auto next_x = static_cast<int>(x) + direction[0];
            const auto next_z = static_cast<int>(z) + direction[1];
            if (next_x < 0 || next_z < 0 || next_x >= static_cast<int>(width) ||
                next_z >= static_cast<int>(height)) {
                continue;
            }
            const auto next = static_cast<std::uint32_t>(next_z) * width +
                              static_cast<std::uint32_t>(next_x);
            if (result.blocked_[next] != 0U) {
                continue;
            }
            const auto step = traversal_cost.empty() ? 1U : traversal_cost[next];
            if (step == 0U || current.cost > kInfinity - step) {
                continue;
            }
            const auto candidate = current.cost + step;
            if (candidate >= result.integration_costs_[next]) {
                continue;
            }
            result.integration_costs_[next] = candidate;
            queue.push(QueueEntry{candidate, next});
        }
    }

    for (std::uint32_t z = 0U; z < height; ++z) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            const auto cell = z * width + x;
            if (result.blocked_[cell] != 0U || result.goals_[cell] != 0U ||
                result.integration_costs_[cell] == kInfinity) {
                continue;
            }
            auto best_cost = result.integration_costs_[cell];
            FlowDirection best{};
            for (const auto& direction : kDirections) {
                const auto next_x = static_cast<int>(x) + direction[0];
                const auto next_z = static_cast<int>(z) + direction[1];
                if (next_x < 0 || next_z < 0 || next_x >= static_cast<int>(width) ||
                    next_z >= static_cast<int>(height)) {
                    continue;
                }
                const auto next = static_cast<std::uint32_t>(next_z) * width +
                                  static_cast<std::uint32_t>(next_x);
                const auto next_cost = result.integration_costs_[next];
                if (next_cost < best_cost) {
                    best_cost = next_cost;
                    best = FlowDirection{static_cast<std::int8_t>(direction[0]),
                                         static_cast<std::int8_t>(direction[1])};
                }
            }
            result.directions_[cell] = best;
        }
    }

    return foundation::Result<FlowField, foundation::Error>::success(std::move(result));
}

bool FlowFieldCache::insert(FlowField field) {
    if (!field.descriptor().valid()) {
        return false;
    }
    fields_.insert_or_assign(field.descriptor().key, std::move(field));
    return true;
}

const FlowField* FlowFieldCache::find(const FlowFieldKey& key) const noexcept {
    const auto iterator = fields_.find(key);
    return iterator == fields_.end() ? nullptr : &iterator->second;
}

std::size_t FlowFieldCache::invalidateNavigationRevision(
    std::uint64_t navigation_revision) noexcept {
    std::size_t removed = 0U;
    for (auto iterator = fields_.begin(); iterator != fields_.end();) {
        if (iterator->first.navigation_revision != navigation_revision) {
            iterator = fields_.erase(iterator);
            ++removed;
        } else {
            ++iterator;
        }
    }
    return removed;
}

} // namespace genomes::navigation
