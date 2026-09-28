#include <genomes/destruction/DamageField.hpp>

#include <cassert>

int main() {
    using namespace genomes::destruction;
    DamageFieldSpec spec{};
    spec.seed = 22U;
    spec.minimum = {0.0F, 0.0F, 0.0F};
    spec.maximum = {1.0F, 1.0F, 1.0F};
    spec.cell_size = {0.25F, 0.25F, 0.25F};
    spec.cells_x = spec.cells_y = spec.cells_z = 4U;
    const auto created = DamageField::create(spec);
    assert(created);
    DamageField field = std::move(created.value());

    ImpactDamageCommand command{};
    command.event_id = 101U;
    command.local_point = {0.5F, 0.5F, 0.5F};
    command.direction = {0.0F, 1.0F, 0.0F};
    command.radius = 0.30F;
    command.depth = 0.20F;
    command.energy = 4.0F;
    command.rear_surface = true;
    command.through_channel = true;
    const DamageApplyResult first = field.apply(command);
    assert(first.affected_cells > 0U);
    assert(first.hole_added);
    assert(first.removed_volume_delta > 0.0F);
    assert(field.aggregateDamage() > 0.0F);
    assert(field.removedVolume() == first.removed_volume_delta);
    assert(field.cell(2U, 2U, 2U)->crush > 0.0F);
    assert(field.cell(0U, 0U, 0U)->crush == 0.0F);

    const DamageApplyResult duplicate = field.apply(command);
    assert(duplicate.affected_cells == first.affected_cells);
    assert(!duplicate.hole_added);
    assert(field.holes().size() == 1U);
    assert(field.aggregateDamage() > first.aggregate_damage_delta);

    command.event_id = 102U;
    command.local_point = {2.0F, 0.5F, 0.5F};
    const DamageApplyResult clipped = field.apply(command);
    assert(clipped.clipped);
    return 0;
}
