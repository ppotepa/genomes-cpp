#pragma once

#include <genomes/destruction/DebrisRecord.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace genomes::destruction {

struct TileCoord final {
    std::int32_t x{0};
    std::int32_t z{0};

    friend constexpr bool operator==(TileCoord, TileCoord) noexcept = default;
    friend constexpr bool operator<(TileCoord left, TileCoord right) noexcept {
        return left.x < right.x || (left.x == right.x && left.z < right.z);
    }
};

struct CellCoord final {
    std::int64_t x{0};
    std::int64_t z{0};

    friend constexpr bool operator==(CellCoord, CellCoord) noexcept = default;
    friend constexpr bool operator<(CellCoord left, CellCoord right) noexcept {
        return left.x < right.x || (left.x == right.x && left.z < right.z);
    }
};

class RubbleTile final {
public:
    RubbleTile(TileCoord coordinate, std::uint32_t cells_per_side);

    [[nodiscard]] TileCoord coordinate() const noexcept { return coordinate_; }
    [[nodiscard]] std::uint32_t cellsPerSide() const noexcept { return cells_per_side_; }
    [[nodiscard]] std::size_t cellCount() const noexcept { return total_volume_.size(); }
    [[nodiscard]] double totalVolume() const noexcept { return total_tile_volume_; }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    void markDirty() noexcept { dirty_ = true; }
    void clearDirty() noexcept { dirty_ = false; }

    [[nodiscard]] double cellVolume(std::uint32_t x, std::uint32_t z) const noexcept;
    [[nodiscard]] double materialVolume(std::uint32_t x,
                                        std::uint32_t z,
                                        MaterialId material) const noexcept;
    [[nodiscard]] const std::vector<MaterialId>& materials() const noexcept {
        return material_ids_;
    }
    [[nodiscard]] const std::vector<double>& materialChannels() const noexcept {
        return material_volumes_;
    }

private:
    friend class RubbleField;

    [[nodiscard]] std::size_t index(std::uint32_t x, std::uint32_t z) const noexcept;
    [[nodiscard]] std::size_t materialIndex(MaterialId material) const noexcept;
    [[nodiscard]] std::size_t ensureMaterial(MaterialId material);
    void add(std::uint32_t x, std::uint32_t z, MaterialId material, double volume);
    [[nodiscard]] double remove(std::uint32_t x,
                                std::uint32_t z,
                                MaterialId material,
                                double volume) noexcept;

    TileCoord coordinate_{};
    std::uint32_t cells_per_side_{0};
    std::vector<double> total_volume_;
    std::vector<MaterialId> material_ids_;
    std::vector<double> material_volumes_;
    double total_tile_volume_{0.0};
    bool dirty_{false};
};

} // namespace genomes::destruction
