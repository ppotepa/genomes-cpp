#include <genomes/spatial/SpatialGrid.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace genomes::spatial {

namespace {

[[nodiscard]] std::uint64_t mix32(std::uint32_t value) noexcept {
    std::uint64_t x = value;
    x ^= x >> 16U;
    x *= 0x7feb352dULL;
    x ^= x >> 15U;
    x *= 0x846ca68bULL;
    x ^= x >> 16U;
    return x;
}

[[nodiscard]] bool finite_position(foundation::Vec3 position) noexcept {
    return std::isfinite(position.x) && std::isfinite(position.y) &&
           std::isfinite(position.z);
}

} // namespace

UniformGrid::UniformGrid(float cell_size_m) noexcept
    : cell_size_m_{cell_size_m}, valid_{std::isfinite(cell_size_m) && cell_size_m > 0.0F} {}

void UniformGrid::clear() noexcept {
    lookup_.clear();
    for (Cell& cell : cells_) {
        cell.entities.clear();
    }
    cells_.clear();
}

void UniformGrid::reserve(std::size_t expected_entities) {
    if (!valid_) {
        return;
    }
    lookup_.reserve(expected_entities);
    cells_.reserve(expected_entities);
}

void UniformGrid::insert(simulation::EntityId entity, foundation::Vec3 position) noexcept {
    if (!valid_ || !entity.isValid() || !finite_position(position)) {
        return;
    }
    const CellKey key = keyFor(position);
    const auto found = lookup_.find(key);
    if (found != lookup_.end()) {
        cells_[found->second].entities.push_back(entity);
        return;
    }
    const std::size_t index = cells_.size();
    cells_.push_back(Cell{key, {entity}});
    lookup_.emplace(key, index);
}

void UniformGrid::queryRadius(foundation::Vec3 center,
                              float radius_m,
                              std::vector<simulation::EntityId>& output) const {
    if (!valid_ || !finite_position(center) || !std::isfinite(radius_m) || radius_m < 0.0F) {
        return;
    }
    const CellKey minimum = keyFor({center.x - radius_m, center.y, center.z - radius_m});
    const CellKey maximum = keyFor({center.x + radius_m, center.y, center.z + radius_m});
    constexpr std::int64_t max_cells_per_axis = 4096;
    if (static_cast<std::int64_t>(maximum.x) - minimum.x > max_cells_per_axis ||
        static_cast<std::int64_t>(maximum.z) - minimum.z > max_cells_per_axis) {
        return;
    }
    const std::size_t original_size = output.size();
    for (std::int64_t z = minimum.z; z <= maximum.z; ++z) {
        for (std::int64_t x = minimum.x; x <= maximum.x; ++x) {
            if (x < std::numeric_limits<std::int32_t>::min() ||
                x > std::numeric_limits<std::int32_t>::max() ||
                z < std::numeric_limits<std::int32_t>::min() ||
                z > std::numeric_limits<std::int32_t>::max()) {
                continue;
            }
            const CellKey key{static_cast<std::int32_t>(x), static_cast<std::int32_t>(z)};
            const auto found = lookup_.find(key);
            if (found == lookup_.end()) {
                continue;
            }
            const Cell& cell = cells_[found->second];
            for (const simulation::EntityId entity : cell.entities) {
                // The grid is a broad phase.  Position filtering happens in
                // the owning system because it already has the hot position
                // arrays and can apply domain-specific vertical rules.
                output.push_back(entity);
            }
        }
    }
    if (output.size() == original_size) {
        return;
    }
    std::sort(output.begin() + static_cast<std::ptrdiff_t>(original_size), output.end(),
              [](simulation::EntityId left, simulation::EntityId right) noexcept {
                  return left.packed() < right.packed();
              });
    output.erase(std::unique(output.begin() + static_cast<std::ptrdiff_t>(original_size),
                             output.end()),
                 output.end());
}

std::size_t UniformGrid::CellKeyHash::operator()(CellKey key) const noexcept {
    const auto x = static_cast<std::uint32_t>(key.x);
    const auto z = static_cast<std::uint32_t>(key.z);
    const std::uint64_t mixed = mix32(x) ^ (mix32(z) << 1U);
    return static_cast<std::size_t>(mixed ^ (mixed >> 32U));
}

UniformGrid::CellKey UniformGrid::keyFor(foundation::Vec3 position) const noexcept {
    const auto to_cell = [this](float value) noexcept {
        const double scaled = std::floor(static_cast<double>(value) /
                                          static_cast<double>(cell_size_m_));
        if (scaled <= static_cast<double>(std::numeric_limits<std::int32_t>::min())) {
            return std::numeric_limits<std::int32_t>::min();
        }
        if (scaled >= static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
            return std::numeric_limits<std::int32_t>::max();
        }
        return static_cast<std::int32_t>(scaled);
    };
    return {to_cell(position.x), to_cell(position.z)};
}

std::size_t UniformGrid::cellIndex(CellKey key) const noexcept {
    const auto found = lookup_.find(key);
    return found == lookup_.end() ? std::numeric_limits<std::size_t>::max() : found->second;
}

} // namespace genomes::spatial
