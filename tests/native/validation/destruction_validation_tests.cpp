#include <genomes/destruction/DamageField.hpp>
#include <genomes/destruction/DebrisLifecycle.hpp>
#include <genomes/destruction/DestructionInvalidation.hpp>
#include <genomes/destruction/DestructionPhysicsAdapter.hpp>
#include <genomes/destruction/ImpactSolver.hpp>
#include <genomes/destruction/MaterialAssembly.hpp>
#include <genomes/destruction/StructuralGraph.hpp>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct Validation final {
    std::uint32_t total{0};
    std::uint32_t passed{0};

    void check(bool condition, std::string_view id) {
        ++total;
        if (condition) {
            ++passed;
            std::cout << "PASS " << id << '\n';
        } else {
            std::cout << "FAIL " << id << '\n';
        }
    }
};

[[nodiscard]] bool near(double left, double right, double tolerance = 1.0e-8) noexcept {
    return std::abs(left - right) <= tolerance;
}

} // namespace

int main() {
    using namespace genomes;
    using namespace genomes::destruction;
    Validation validation;

    const MaterialCatalog catalog = MaterialCatalog::makeDefault();
    const PhysicalSolidId wall = PhysicalSolidId::fromName("validation-wall");
    const std::vector<Layer> layers = {
        {MaterialId::fromName("concrete"), wall, 0.0F, 0.30F, LayerKind::Solid},
        {MaterialId::fromName("brick"), wall, 0.30F, 0.60F, LayerKind::Solid},
        {{}, {}, 0.60F, 0.80F, LayerKind::Void},
        {MaterialId::fromName("steel"), PhysicalSolidId::fromName("validation-plate"),
         0.80F, 1.00F, LayerKind::Solid}};
    const auto assembly_result = MaterialAssembly::create(
        0x1234U, MaterialFrame::identity(), layers, catalog);
    validation.check(assembly_result && assembly_result.value().traverse(0.0F, 1.0F).size() == 4U,
                     "materials.forward_traversal");
    validation.check(assembly_result && assembly_result.value().traverse(1.0F, 0.0F).front().reversed,
                     "materials.reverse_traversal");

    if (assembly_result) {
        ImpactInput input{};
        input.mass_kg = 0.01F;
        input.diameter_m = 0.00556F;
        input.projectile_velocity = {0.0F, 120.0F, 0.0F};
        input.target_linear_velocity = {0.0F, 20.0F, 0.0F};
        input.projectile_center = {0.0F, -0.1F, 0.0F};
        input.contact_point = {0.2F, 0.0F, 0.0F};
        input.contact_normal = {0.0F, 1.0F, 0.0F};
        input.assembly = &assembly_result.value();
        input.entry_point = {0.0F, 0.0F, 0.0F};
        input.exit_point = {0.0F, 1.0F, 0.0F};
        const auto impact = ImpactSolver::solve(input, catalog);
        validation.check(impact &&
                             std::abs(impact.value().relative_incoming_velocity.y - 100.0F) <
                                 1.0e-4F,
                         "impact.relative_velocity");
        const auto void_assembly_result = MaterialAssembly::create(
            0x1235U,
            MaterialFrame::identity(),
            std::vector<Layer>{{{}, {}, 0.0F, 1.0F, LayerKind::Void}},
            catalog);
        bool energy_balanced = false;
        if (void_assembly_result) {
            auto energy_input = input;
            energy_input.target_linear_velocity = {};
            energy_input.assembly = &void_assembly_result.value();
            const auto void_impact = ImpactSolver::solve(energy_input, catalog);
            energy_balanced = void_impact &&
                              std::abs(void_impact.value().energy.balance_error) < 1.0e-3F;
        }
        validation.check(energy_balanced, "impact.energy_balance");
    } else {
        validation.check(false, "impact.relative_velocity");
        validation.check(false, "impact.energy_balance");
    }

    DamageFieldSpec damage_spec{};
    damage_spec.seed = 22U;
    damage_spec.minimum = {0.0F, 0.0F, 0.0F};
    damage_spec.maximum = {1.0F, 1.0F, 1.0F};
    damage_spec.cell_size = {0.25F, 0.25F, 0.25F};
    damage_spec.cells_x = damage_spec.cells_y = damage_spec.cells_z = 4U;
    const auto damage_result = DamageField::create(damage_spec);
    validation.check(static_cast<bool>(damage_result), "damage.create");
    if (damage_result) {
        DamageField damage = std::move(damage_result.value());
        ImpactDamageCommand command{};
        command.event_id = 101U;
        command.local_point = {0.5F, 0.5F, 0.5F};
        command.direction = {0.0F, 1.0F, 0.0F};
        command.radius = 0.30F;
        command.depth = 0.20F;
        command.energy = 4.0F;
        command.rear_surface = true;
        command.through_channel = true;
        const DamageApplyResult applied = damage.apply(command);
        const DamageApplyResult duplicate = damage.apply(command);
        validation.check(applied.hole_added && applied.removed_volume_delta > 0.0F,
                         "damage.local_hole");
        validation.check(!duplicate.hole_added && damage.holes().size() == 1U,
                         "damage.idempotent_event");
    } else {
        validation.check(false, "damage.local_hole");
        validation.check(false, "damage.idempotent_event");
    }

    const auto graph_result = StructuralGraph::create(
        {{1U, 10U, 1.0F, true}, {2U, 20U, 1.0F, false}, {3U, 30U, 1.0F, false}},
        {{11U, 1U, 2U, 1.0F}, {12U, 2U, 3U, 1.0F}});
    validation.check(static_cast<bool>(graph_result), "structural.create");
    if (graph_result) {
        StructuralGraph graph = std::move(graph_result.value());
        (void)graph.evaluateSupport();
        (void)graph.applyEdgeDamage(11U, 1.0F);
        const auto detached = graph.evaluateSupport();
        validation.check(detached.size() == 2U && graph.nodeStates()[2].detached,
                         "structural.detach_component");
    } else {
        validation.check(false, "structural.detach_component");
    }

    const auto lifecycle_result = DebrisLifecycle::create({8U, 4U, 16U, 2U});
    validation.check(static_cast<bool>(lifecycle_result), "lifecycle.create");
    if (lifecycle_result) {
        DebrisLifecycle lifecycle = std::move(lifecycle_result.value());
        DebrisRecord record{501U,
                            900U,
                            {{MaterialId::fromName("brick"), 1.0}},
                            {},
                            {},
                            {},
                            DebrisRepresentation::Cheap,
                            0U,
                            1.0F,
                            true,
                            true};
        (void)lifecycle.spawn(record);
        (void)lifecycle.sleep(501U);
        lifecycle.advance(2U);
        const auto settlement = lifecycle.prepareSettlement();
        const std::vector<foundation::StableId> ids{501U};
        const bool committed = lifecycle.commitSettlement(ids);
        validation.check(settlement.size() == 1U && committed &&
                             lifecycle.count(DebrisRepresentation::Baked) == 1U,
                         "lifecycle.settle_commit");
        validation.check(near(lifecycle.accounting().representedVolume(), 1.0),
                         "lifecycle.volume_conservation");
    } else {
        validation.check(false, "lifecycle.settle_commit");
        validation.check(false, "lifecycle.volume_conservation");
    }

    const auto rubble_result = RubbleField::create();
    validation.check(static_cast<bool>(rubble_result), "rubble.create");
    if (rubble_result) {
        RubbleField rubble = std::move(rubble_result.value());
        const MaterialId brick = MaterialId::fromName("brick");
        const MaterialId steel = MaterialId::fromName("steel");
        (void)rubble.deposit(
            {601U, {-0.25F, 0.0F, -0.25F}, 0.0F, {{brick, 1.0}, {steel, 0.5}}});
        const double before_remove = rubble.totalVolume();
        (void)rubble.remove({{-0.25F, 0.0F, -0.25F}, 0.0F, 0.25});
        const double after_remove = rubble.totalVolume();
        const double before_relax = rubble.totalVolume();
        (void)rubble.relax();
        validation.check(near(before_remove, 1.5) && near(after_remove, 1.25),
                         "rubble.mixed_material_remove");
        validation.check(near(rubble.totalVolume(), before_relax), "rubble.relax_conservation");
        validation.check(rubble.tiles().find({-1, -1}) != rubble.tiles().end(),
                         "rubble.negative_coordinates");
    } else {
        validation.check(false, "rubble.mixed_material_remove");
        validation.check(false, "rubble.relax_conservation");
        validation.check(false, "rubble.negative_coordinates");
    }

    const auto physics_rubble_result = RubbleField::create();
    validation.check(static_cast<bool>(physics_rubble_result),
                     "integration.rubble_physics_create");
    if (physics_rubble_result) {
        RubbleField rubble = std::move(physics_rubble_result.value());
        (void)rubble.deposit({701U,
                              {0.0F, 0.0F, 0.0F},
                              0.0F,
                              {{MaterialId::fromName("brick"), 2.0}}});
        physics::SimplePhysicsWorld physics;
        DestructionPhysicsAdapter adapter{physics, rubble};
        DestructionPhysicsCommandBuffer commands;
        commands.createHero({702U,
                             {0.0F, 2.0F, 0.0F},
                             {},
                             {0.5F, 0.5F, 0.5F},
                             1.0F,
                             true});
        commands.applyImpact(703U, 702U, {1.0F, 0.0F, 0.0F});
        commands.bakeRubbleTile({0, 0});
        const auto sync = adapter.sync(commands);
        validation.check(sync.heroes_created == 1U && sync.rubble_rebuilt == 1U,
                         "integration.physics_replacement");
        validation.check(sync.impacts_applied == 1U && adapter.impactWasApplied(703U),
                         "integration.single_impact");
    } else {
        validation.check(false, "integration.physics_replacement");
        validation.check(false, "integration.single_impact");
    }

    DestructionInvalidationConfig invalidation_config{};
    invalidation_config.coordinates.region_size_m = 4.0;
    invalidation_config.coalesce_distance_m = 0.25F;
    DestructionInvalidationQueue invalidation{invalidation_config};
    const world::WorldId world_id{77U};
    const bool emitted = invalidation.emit(
        world_id, 801U, 1U, {{3.5F, 0.0F, 0.5F}, {4.5F, 1.0F, 1.5F}},
        static_cast<world::DirtyReasonMask>(world::DirtyReason::StructuralGeometry) |
            static_cast<world::DirtyReasonMask>(world::DirtyReason::Traversability));
    validation.check(emitted && invalidation.pendingCount(InvalidationConsumer::Navigation) == 2U,
                     "invalidation.region_boundary");
    const auto pending = invalidation.pending(InvalidationConsumer::Navigation);
    validation.check(!pending.empty() && invalidation.acknowledge(
                                      InvalidationConsumer::Navigation, pending.front()),
                     "invalidation.retryable_ack");

    std::cout << "VALIDATION " << validation.passed << '/' << validation.total << '\n';
    return validation.passed == validation.total ? 0 : 1;
}
