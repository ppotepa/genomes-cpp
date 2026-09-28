#pragma once

#include <genomes/destruction/RubbleTile.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <cstdint>
#include <map>
#include <vector>

namespace genomes::destruction {

inline constexpr std::uint32_t RubbleFieldVersion = 1;

struct TerrainHeightProvider final {
    void* context{nullptr};
    float (*sample)(void*, float, float) noexcept{nullptr};
};

struct RubbleFieldSpec final {
    float cell_size_m{0.5F};
    float tile_size_m{4.0F};
    std::uint32_t cells_per_tile{8};
    std::uint32_t max_tiles{256};
    std::uint32_t max_chunk_instances{1024};
    float max_slope{0.78F};
    float packing_factor{0.62F};
    std::uint32_t relaxation_iterations{4};

    [[nodiscard]] bool valid() const noexcept;
};

struct RubbleDeposit final {
    foundation::StableId source_id{0};
    foundation::Vec3 position{};
    float radius_m{0.0F};
    std::vector<MaterialVolume> material_volumes;

    [[nodiscard]] double totalVolume() const noexcept;
};

struct RubbleDepositResult final {
    double deposited_volume{0.0};
    std::uint32_t affected_cells{0};
    bool tile_overflow_merged{false};
};

struct RubbleRemoveRequest final {
    foundation::Vec3 position{};
    float radius_m{0.0F};
    double volume{0.0};
};

struct RubbleRemoveResult final {
    double removed_volume{0.0};
    std::vector<MaterialVolume> material_volumes;
};

struct RubbleRelaxResult final {
    std::uint32_t transfers{0};
    double transferred_volume{0.0};
};

struct RubbleNavigationSample final {
    float surface_height{0.0F};
    float movement_cost{1.0F};
    bool blocked{false};
};

struct RubbleCoverSample final {
    float height{0.0F};
    float opacity{0.0F};
    bool valid{false};
};

class RubbleField final {
public:
    [[nodiscard]] static foundation::Result<RubbleField, foundation::Error> create(
        RubbleFieldSpec spec = {}, TerrainHeightProvider terrain = {});

    [[nodiscard]] foundation::Result<RubbleDepositResult, foundation::Error> deposit(
        const RubbleDeposit&);
    [[nodiscard]] foundation::Result<RubbleRemoveResult, foundation::Error> remove(
        const RubbleRemoveRequest&);
    [[nodiscard]] RubbleRelaxResult relax() noexcept;

    [[nodiscard]] float pileHeightAt(float x, float z) const noexcept;
    [[nodiscard]] float surfaceHeightAt(float x, float z) const noexcept;
    [[nodiscard]] float pileSlopeAt(float x, float z) const noexcept;
    [[nodiscard]] float movementCostAt(float x, float z) const noexcept;
    [[nodiscard]] RubbleNavigationSample sampleNavigation(float x, float z) const noexcept;
    [[nodiscard]] RubbleCoverSample coverAt(float x, float z) const noexcept;

    [[nodiscard]] std::uint32_t version() const noexcept { return version_; }
    [[nodiscard]] const RubbleFieldSpec& spec() const noexcept { return spec_; }
    [[nodiscard]] const std::map<TileCoord, RubbleTile>& tiles() const noexcept { return tiles_; }
    [[nodiscard]] double totalVolume() const noexcept;
    [[nodiscard]] double materialVolume(MaterialId) const noexcept;
    [[nodiscard]] const std::vector<TileCoord>& dirtyTiles() const noexcept { return dirty_tiles_; }
    [[nodiscard]] std::vector<TileCoord> takeDirtyTiles() noexcept;

    // Public so bounded helper algorithms can carry a resolved cell location
    // without exposing the field's storage or mutation helpers.
    struct CellLocation final {
        CellCoord global{};
        TileCoord tile{};
        std::uint32_t local_x{0};
        std::uint32_t local_z{0};
    };

private:

    [[nodiscard]] CellLocation locate(float x, float z) const noexcept;
    [[nodiscard]] RubbleTile* ensureTile(TileCoord, bool* merged);
    [[nodiscard]] RubbleTile* tile(TileCoord) noexcept;
    [[nodiscard]] const RubbleTile* tile(TileCoord) const noexcept;
    void markDirty(TileCoord);
    void mergeTile(TileCoord source, TileCoord target);
    [[nodiscard]] double baseHeight(float x, float z) const noexcept;
    [[nodiscard]] double cellSurfaceHeight(CellCoord) const noexcept;
    [[nodiscard]] double cellVolume(CellCoord) const noexcept;
    [[nodiscard]] RubbleTile* tileForCell(CellCoord, bool* merged);

    std::uint32_t version_{RubbleFieldVersion};
    RubbleFieldSpec spec_{};
    TerrainHeightProvider terrain_{};
    std::map<TileCoord, RubbleTile> tiles_;
    std::vector<TileCoord> dirty_tiles_;
};

} // namespace genomes::destruction
