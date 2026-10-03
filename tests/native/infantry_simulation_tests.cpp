#include <genomes/infantry/InfantrySimulation.hpp>
#include <genomes/navigation/NavigationWorld.hpp>
#include <genomes/physics/PhysicsWorld.hpp>
#include <genomes/simulation/EntityStore.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <utility>

namespace {

void advanceTicks(genomes::infantry::InfantrySimulation& infantry,
                  std::uint64_t count) {
    for (std::uint64_t tick = 1U; tick <= count; ++tick) {
        infantry.fixedUpdate(1.0 / 60.0, {tick});
    }
}

class WaypointNavigation final : public genomes::navigation::NavigationWorld {
public:
    [[nodiscard]] genomes::foundation::Result<genomes::navigation::PathResult,
                                               genomes::foundation::Error>
    findPath(const genomes::navigation::PathRequest& request) const override {
        ++path_calls;
        genomes::navigation::PathResult result{};
        result.status = genomes::navigation::PathStatus::Complete;
        result.points = {request.start, {2.0F, request.start.y, request.start.z}, request.goal};
        return genomes::foundation::Result<genomes::navigation::PathResult,
                                           genomes::foundation::Error>::success(std::move(result));
    }

    mutable std::size_t path_calls{0U};
};

void entityControllerResolvesOrdersAndMovesTowardTheirDestination() {
    using namespace genomes;

    constexpr simulation::EntityId entity{7U, 1U};
    const std::array<simulation::EntityOrder, 4U> orders{{
        {entity, simulation::EntityOrderKind::MoveTo, {}, {0.0F, 0.0F, 10.0F},
         foundation::stable_id("action.walk"), 3U, 4U, 8U, 1U},
        {entity, simulation::EntityOrderKind::Retreat, {}, {10.0F, 0.0F, 0.0F},
         foundation::stable_id("action.retreat"), 1U, 5U, 8U, 2U},
        {entity, simulation::EntityOrderKind::Hold, {}, {}, 0U,
         0U, 1U, 2U, 255U},
        {entity, simulation::EntityOrderKind::MoveTo, {}, {-10.0F, 0.0F, 0.0F},
         foundation::stable_id("action.other"), 2U, 5U, 8U, 2U},
    }};
    const auto selected = simulation::resolveEntityOrder(entity, orders, 5U);
    assert(selected);
    assert(selected->kind == simulation::EntityOrderKind::Retreat);
    assert(selected->action == foundation::stable_id("action.retreat"));

    infantry::InfantryUnitController infantry_controller;
    simulation::EntityController& controller = infantry_controller;
    const simulation::EntityReadView state{
        entity, {0.0F, 0.0F, 0.0F}, {}, 0.0F, 100.0F,
        simulation::EntityAlive, {}};
    const std::array<simulation::EntityControlRequest, 1U> requests{{
        {state, *selected, 3.0F, 4.0F, 6.0F, 2.0F, true},
    }};
    std::array<simulation::EntityControlCommand, 1U> commands{};
    const simulation::TickContext tick{
        foundation::SimulationTick{5U}, 1.0 / 60.0,
        simulation::SessionSimulationTickRateHz};
    const auto updated = controller.updateBatch(requests, commands, tick);
    assert(updated);
    assert(commands[0].entity == entity);
    assert(commands[0].action == foundation::stable_id("action.retreat"));
    assert(commands[0].position.x > 0.0F);
    assert(commands[0].velocity.x > 0.0F);
    assert(commands[0].heading_radians > 0.0F);
    assert(commands[0].position.z >= 0.0F);
}

void externalMoveOrderReachesSimulationMovementAndRenderState() {
    using namespace genomes;

    simulation::EntityStore entities;
    infantry::InfantrySimulation infantry(entities);
    const auto genome = infantry::InfantryGenome::generate(0xB4771E1DU);
    assert(genome);
    const auto spawned = infantry.spawn(
        {infantry::Team::Blue, {}, genome.value(), {}});
    assert(spawned);

    simulation::EntityOrder order{};
    order.entity = spawned.value();
    order.kind = simulation::EntityOrderKind::MoveTo;
    order.destination = {0.0F, 0.0F, 8.0F};
    order.action = foundation::stable_id("test.action.external-move");
    order.source = foundation::stable_id("test.external-order");
    order.issued_tick = 1U;
    order.expires_tick = 120U;
    order.priority = 150U;
    assert(infantry.setOrder(order));

    for (std::uint64_t tick = 1U; tick <= 90U; ++tick) {
        infantry.fixedUpdate(1.0 / 60.0, {tick});
    }

    const foundation::Vec3* position = entities.position(spawned.value());
    assert(position != nullptr);
    assert(position->z > 0.5F);
    const auto render_state = std::find_if(
        infantry.renderStates().begin(), infantry.renderStates().end(),
        [entity = spawned.value()](const infantry::InfantryRenderState& state) {
            return state.entity == entity;
        });
    assert(render_state != infantry.renderStates().end());
    assert(render_state->action == order.action);
}

void changedMoveOrderDiscardsThePreviousNavigationRoute() {
    using namespace genomes;

    simulation::EntityStore entities;
    WaypointNavigation navigation;
    infantry::InfantrySimulation infantry(entities, &navigation);
    const auto genome = infantry::InfantryGenome::generate(0xA11CEU);
    assert(genome);
    const auto spawned = infantry.spawn(
        {infantry::Team::Blue, {}, genome.value(), {}});
    assert(spawned);

    simulation::EntityOrder order{};
    order.entity = spawned.value();
    order.kind = simulation::EntityOrderKind::MoveTo;
    order.destination = {0.0F, 0.0F, 30.0F};
    order.source = foundation::stable_id("test.route-change");
    order.issued_tick = 1U;
    order.expires_tick = 200U;
    order.priority = 150U;
    assert(infantry.setOrder(order));

    std::uint64_t tick = 0U;
    while (navigation.path_calls == 0U && tick < 30U) {
        ++tick;
        infantry.fixedUpdate(1.0 / 60.0, {tick});
    }
    assert(navigation.path_calls > 0U);
    const auto previous_state = std::find_if(
        infantry.renderStates().begin(), infantry.renderStates().end(),
        [entity = spawned.value()](const infantry::InfantryRenderState& state) {
            return state.entity == entity;
        });
    assert(previous_state != infantry.renderStates().end());
    const float previous_heading = previous_state->heading;

    order.destination = {-30.0F, 0.0F, 0.0F};
    order.issued_tick = tick + 1U;
    assert(infantry.setOrder(order));
    infantry.fixedUpdate(1.0 / 60.0, {tick + 1U});

    const auto changed_state = std::find_if(
        infantry.renderStates().begin(), infantry.renderStates().end(),
        [entity = spawned.value()](const infantry::InfantryRenderState& state) {
            return state.entity == entity;
        });
    assert(changed_state != infantry.renderStates().end());
    assert(changed_state->heading < previous_heading);
}

void nearbyWaypointDoesNotMakeDistantTargetEngage() {
    genomes::simulation::EntityStore entities;
    WaypointNavigation navigation;
    genomes::infantry::InfantrySimulation infantry(entities, &navigation);
    const auto generated = genomes::infantry::InfantryGenome::generate(1);
    assert(generated);
    auto genome = generated.value();
    genome.perception_radius = 200.0F;
    genome.attack_range = 35.0F;

    const auto missing_weapon = infantry.spawn(
        {genomes::infantry::Team::Blue, {}, genome, {}, genomes::weapons::weapon_id("missing")});
    assert(!missing_weapon);

    const auto observer = infantry.spawn(
        {genomes::infantry::Team::Blue, {}, genome, {{genomes::infantry::Team::Blue, 1}}});
    const auto target = infantry.spawn(
        {genomes::infantry::Team::Red,
         {100.0F, 0.0F, 0.0F},
         genome,
         {{genomes::infantry::Team::Red, 2}}});
    assert(observer && target);

    advanceTicks(infantry, 24U);
    const auto& states = infantry.renderStates();
    assert(states.size() == 2);
    const auto observer_state = states[0].entity == observer.value() ? states[0] : states[1];
    assert(observer_state.state == genomes::infantry::AgentState::Advance);
}

void squadContactsRequireMatchingSideAndExplicitMembership() {
    genomes::simulation::EntityStore entities;
    genomes::infantry::InfantrySimulation infantry(entities);
    const auto generated = genomes::infantry::InfantryGenome::generate(2);
    assert(generated);
    auto genome = generated.value();
    genome.perception_radius = 20.0F;

    const auto squad_leader = infantry.spawn(
        {genomes::infantry::Team::Blue, {}, genome, {{genomes::infantry::Team::Blue, 1}}});
    assert(infantry.spawn({genomes::infantry::Team::Red, {10.0F, 0.0F, 0.0F}, genome, {}}));
    const auto squad_member = infantry.spawn(
        {genomes::infantry::Team::Blue, {100.0F, 0.0F, 0.0F}, genome, {{genomes::infantry::Team::Blue, 1}}});
    const auto opposing_local_one = infantry.spawn(
        {genomes::infantry::Team::Red, {200.0F, 0.0F, 0.0F}, genome, {{genomes::infantry::Team::Red, 1}}});
    assert(infantry.spawn({genomes::infantry::Team::Blue, {300.0F, 0.0F, 0.0F}, genome, {}}));
    assert(infantry.spawn({genomes::infantry::Team::Red, {310.0F, 0.0F, 0.0F}, genome, {}}));
    const auto no_squad_observer = infantry.spawn(
        {genomes::infantry::Team::Blue, {400.0F, 0.0F, 0.0F}, genome, {}});
    assert(squad_leader && squad_member && opposing_local_one && no_squad_observer);

    advanceTicks(infantry, 24U);
    const auto& states = infantry.renderStates();
    const auto state_of = [&states](genomes::simulation::EntityId entity) {
        for (const auto& state : states) {
            if (state.entity == entity) {
                return state.state;
            }
        }
        assert(false && "spawned entity has no render state");
        return genomes::infantry::AgentState::Dead;
    };
    assert(state_of(squad_member.value()) == genomes::infantry::AgentState::Advance);
    assert(state_of(opposing_local_one.value()) == genomes::infantry::AgentState::Idle);
    assert(state_of(no_squad_observer.value()) == genomes::infantry::AgentState::Idle);
}

void staleAgentSidecarCannotAttachToReusedEntityIndex() {
    genomes::simulation::EntityStore entities;
    genomes::infantry::InfantrySimulation infantry(entities);
    const auto generated = genomes::infantry::InfantryGenome::generate(3);
    assert(generated);
    const auto original = infantry.spawn({genomes::infantry::Team::Blue, {}, generated.value(), {}});
    assert(original);
    entities.destroy(original.value());
    const auto replacement = entities.create({{25.0F, 0.0F, 0.0F}, {}, 0.0F, 100.0F,
                                              genomes::simulation::EntityAlive});
    assert(replacement);
    assert(replacement.value().index == original.value().index);
    assert(replacement.value().generation != original.value().generation);

    infantry.fixedUpdate(1.0 / 60.0, {1});
    assert(infantry.activeCount() == 0);
    assert(infantry.renderStates().empty());
}

void externalPhysicsOwnerStepsExactlyOnce() {
    genomes::simulation::EntityStore entities;
    genomes::physics::SimplePhysicsWorld physics;
    genomes::infantry::InfantrySimulation infantry(entities, nullptr, &physics, nullptr, true);
    const auto generated = genomes::infantry::InfantryGenome::generate(4);
    assert(generated);
    assert(infantry.spawn({genomes::infantry::Team::Blue, {}, generated.value(), {}}));
    infantry.fixedUpdate(1.0 / 60.0, {1});
    assert(physics.stepCount() == 0U);
    infantry.stepPhysics(1.0 / 60.0);
    assert(physics.stepCount() == 1U);
}

} // namespace

int main() {
    entityControllerResolvesOrdersAndMovesTowardTheirDestination();
    externalMoveOrderReachesSimulationMovementAndRenderState();
    changedMoveOrderDiscardsThePreviousNavigationRoute();
    nearbyWaypointDoesNotMakeDistantTargetEngage();
    squadContactsRequireMatchingSideAndExplicitMembership();
    staleAgentSidecarCannotAttachToReusedEntityIndex();
    externalPhysicsOwnerStepsExactlyOnce();
    return 0;
}
