#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/simulation/EntityStore.hpp>

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace genomes::spatial {

// A deterministic broad-phase index for dynamic simulation entities.  The
// index is rebuilt from the authoritative EntityStore at a simulation barrier;
// it never owns entity state and therefore cannot become a second source of
// truth.  The current implementation is a uniform XZ grid, while the public
// query contract leaves room for a BVH or tiled implementation later.
class UniformGrid final {
public:
    explicit UniformGrid(float cell_size_m = 16.0F) noexcept;

    [[nodiscard]] bool valid() const noexcept { return valid_; }
    [[nodiscard]] float cellSize() const noexcept { return cell_size_m_; }

    void clear() noexcept;
    void reserve(std::size_t expected_entities);
    void insert(simulation::EntityId entity, foundation::Vec3 position) noexcept;

    // Results are appended to output, sorted by packed EntityId and deduplicated
    // so perception and LOS batching receive a stable candidate order even if a
    // future query shape touches the same cell more than once.
    void queryRadius(foundation::Vec3 center,
                     float radius_m,
                     std::vector<simulation::EntityId>& output) const;

    [[nodiscard]] std::size_t cellCount() const noexcept { return cells_.size(); }

private:
    struct CellKey final {
        std::int32_t x{0};
        std::int32_t z{0};

        [[nodiscard]] bool operator==(const CellKey&) const noexcept = default;
    };

    struct CellKeyHash final {
        [[nodiscard]] std::size_t operator()(CellKey key) const noexcept;
    };

    struct Cell final {
        CellKey key{};
        std::vector<simulation::EntityId> entities;
    };

    [[nodiscard]] CellKey keyFor(foundation::Vec3 position) const noexcept;
    [[nodiscard]] std::size_t cellIndex(CellKey key) const noexcept;

    float cell_size_m_{16.0F};
    bool valid_{false};
    std::unordered_map<CellKey, std::size_t, CellKeyHash> lookup_;
    std::vector<Cell> cells_;
};

} // namespace genomes::spatial
