#include <genomes/destruction/RubbleField.hpp>

#include <cassert>
#include <cmath>
#include <utility>

int main() {
    using namespace genomes::destruction;

    RubbleFieldSpec spec{};
    spec.cell_size_m = 0.5F;
    spec.tile_size_m = 2.0F;
    spec.cells_per_tile = 4U;
    spec.max_tiles = 8U;
    spec.relaxation_iterations = 3U;
    const auto created = RubbleField::create(spec);
    assert(created);
    RubbleField field = std::move(created.value());

    const MaterialId brick = MaterialId::fromName("brick");
    const MaterialId steel = MaterialId::fromName("steel");
    const RubbleDeposit deposit{42U,
                                {-0.25F, 0.0F, -0.25F},
                                0.0F,
                                {{brick, 1.0}, {steel, 0.5}}};
    const auto deposited = field.deposit(deposit);
    assert(deposited);
    assert(deposited.value().affected_cells == 1U);
    assert(std::abs(deposited.value().deposited_volume - 1.5) < 1.0e-12);
    assert(std::abs(field.totalVolume() - 1.5) < 1.0e-12);
    assert(std::abs(field.materialVolume(brick) - 1.0) < 1.0e-12);
    assert(std::abs(field.materialVolume(steel) - 0.5) < 1.0e-12);
    assert(field.tiles().find(TileCoord{-1, -1}) != field.tiles().end());
    assert(field.pileHeightAt(-0.25F, -0.25F) > 0.0F);
    assert(field.coverAt(-0.25F, -0.25F).valid);
    assert(!field.dirtyTiles().empty());

    const auto removed = field.remove({{-0.25F, 0.0F, -0.25F}, 0.0F, 0.25});
    assert(removed);
    assert(std::abs(removed.value().removed_volume - 0.25) < 1.0e-12);
    assert(std::abs(field.totalVolume() - 1.25) < 1.0e-12);
    assert(std::abs(field.materialVolume(brick) + field.materialVolume(steel) - 1.25) <
           1.0e-12);

    const auto second = field.deposit({43U,
                                       {0.75F, 0.0F, -0.25F},
                                       0.0F,
                                       {{brick, 4.0}}});
    assert(second);
    const double before_relax = field.totalVolume();
    const RubbleRelaxResult relaxed = field.relax();
    assert(relaxed.transfers > 0U);
    assert(relaxed.transferred_volume > 0.0);
    assert(std::abs(field.totalVolume() - before_relax) < 1.0e-9);

    const auto dirty = field.takeDirtyTiles();
    assert(!dirty.empty());
    assert(field.dirtyTiles().empty());

    RubbleFieldSpec capacity_spec{};
    capacity_spec.max_tiles = 1U;
    const auto capacity_created = RubbleField::create(capacity_spec);
    assert(capacity_created);
    RubbleField capacity_field = std::move(capacity_created.value());
    assert(capacity_field.deposit({100U,
                                   {0.0F, 0.0F, 0.0F},
                                   0.0F,
                                   {{brick, 1.0}}}));
    const auto overflow = capacity_field.deposit({101U,
                                                  {32.0F, 0.0F, 0.0F},
                                                  0.0F,
                                                  {{steel, 2.0}}});
    assert(overflow);
    assert(overflow.value().tile_overflow_merged);
    assert(capacity_field.tiles().size() == 1U);
    assert(std::abs(capacity_field.totalVolume() - 3.0) < 1.0e-12);
    assert(std::abs(capacity_field.materialVolume(brick) - 1.0) < 1.0e-12);
    assert(std::abs(capacity_field.materialVolume(steel) - 2.0) < 1.0e-12);
    return 0;
}
