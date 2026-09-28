#include <genomes/ballistics/ProjectileState.hpp>

#include <cassert>
#include <cmath>

int main() {
    using namespace genomes::ballistics;

    const StrategyId strategy_id_value = strategy_id("validation.fmj");
    const CaliberId caliber_id_value = caliber_id("9mm");
    const VariantId variant_id_value = variant_id("standard");
    AmmunitionStrategy strategy{};
    strategy.id = strategy_id_value;
    strategy.caliber_id = caliber_id_value;
    strategy.variant_id = variant_id_value;
    strategy.drag_coefficient = 0.31F;
    strategy.contact_work_scale = 1.0F;
    strategy.explosive = true;
    strategy.breakup_energy_threshold = 10.0F;
    strategy.fragment_mass_fraction = 0.5F;
    strategy.max_fragments = 4U;

    AmmunitionCatalog catalog;
    assert(catalog.add({ammunition_id("ammo.9mm"),
                        strategy_id_value,
                        caliber_id_value,
                        variant_id_value,
                        0.008F,
                        0.009F,
                        0.009F,
                        360.0F,
                        0.25F,
                        1U,
                        "fixture"},
                       strategy));
    assert(catalog.freeze());
    assert(catalog.frozen());
    assert(catalog.size() == 1U);

    FireRequest request{};
    request.projectile_id = ProjectileId{101U};
    request.shot_id = ShotId{102U};
    request.trace_id = TraceId{103U};
    request.ammunition_id = ammunition_id("ammo.9mm");
    request.position = {1.0F, 2.0F, 3.0F};
    request.direction = {1.0F, 0.0F, 0.0F};
    request.seed = 0x1234U;
    const auto created = ProjectileState::create(request, catalog);
    assert(created);
    ProjectileState state = created.value();
    assert(state.valid());
    assert(std::abs(state.body_forward.x - 1.0F) < 1.0e-6F);
    assert(std::abs(state.body_forward.y) < 1.0e-6F);
    assert(std::abs(state.body_forward.z) < 1.0e-6F);
    assert(std::abs(state.velocity.x - 360.0F) < 1.0e-5F);
    const float initial_translational = state.translationalEnergy();
    assert(std::abs(initial_translational - 518.4F) < 1.0e-3F);

    state.angular_velocity = {2.0F, 1.0F, 0.5F};
    const float initial_rotational = state.rotationalEnergy();
    const auto transitioned = state.apply({{}, {}, -0.1F, 0.05F, -0.1F, 2.0F, 0.5F, 1.25F,
                                           2.0F, 0.5F, 1.0F, 0.0F, 0.0F, 1U});
    assert(transitioned);
    assert(state.impact_index == 1U);
    assert(state.ricochet_count == 1U);
    assert(state.mass_kg > 0.0F && state.diameter_m > 0.0F);
    assert(std::abs(state.rotationalEnergy() - initial_rotational) < 1.0e-5F);
    assert(state.energy.material_work == 2.0F);
    assert(state.energy.contact_loss == 0.5F);
    assert(std::abs(state.energyBalanceError(initial_translational, initial_rotational)) > 0.0F);

    assert(state.advanceAge(60U, 1.0F, 12.0F));
    assert(state.age_ticks == 60U);
    assert(std::abs(state.travel_distance_m - 12.0F) < 1.0e-6F);

    const BreakupPlan first = strategy.breakupPlan(state.projectile_id.value(), 1U, 20.0F);
    const BreakupPlan second = strategy.breakupPlan(state.projectile_id.value(), 1U, 20.0F);
    assert(first.detonated && first.pieces.size() == 4U);
    assert(first.pieces.size() == second.pieces.size());
    for (std::size_t index = 0; index < first.pieces.size(); ++index) {
        assert(first.pieces[index].child_id == second.pieces[index].child_id);
        assert(first.pieces[index].direction_offset.x == second.pieces[index].direction_offset.x);
        assert(first.pieces[index].direction_offset.y == second.pieces[index].direction_offset.y);
        assert(first.pieces[index].direction_offset.z == second.pieces[index].direction_offset.z);
    }
    assert(!strategy.breakupPlan(state.projectile_id.value(), 2U, 1.0F).detonated);
    return 0;
}
