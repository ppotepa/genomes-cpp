#include <genomes/infantry/InfantrySimulation.hpp>
#include <genomes/navigation/NavigationWorld.hpp>
#include <genomes/simulation/EntityStore.hpp>

#include <cassert>
#include <utility>

namespace {

class WaypointNavigation final : public genomes::navigation::NavigationWorld {
public:
    [[nodiscard]] genomes::foundation::Result<genomes::navigation::PathResult,
                                               genomes::foundation::Error>
    findPath(const genomes::navigation::PathRequest& request) const override {
        genomes::navigation::PathResult result{};
        result.status = genomes::navigation::PathStatus::Complete;
        result.points = {request.start, {2.0F, request.start.y, request.start.z}, request.goal};
        return genomes::foundation::Result<genomes::navigation::PathResult,
                                           genomes::foundation::Error>::success(std::move(result));
    }
};

void nearbyWaypointDoesNotMakeDistantTargetEngage() {
    genomes::simulation::EntityStore entities;
    WaypointNavigation navigation;
    genomes::infantry::InfantrySimulation infantry(entities, &navigation);
    const auto generated = genomes::infantry::InfantryGenome::generate(1);
    assert(generated);
    auto genome = generated.value();
    genome.perception_radius = 200.0F;
    genome.attack_range = 35.0F;

    const auto observer = infantry.spawn(
        {genomes::infantry::Team::Blue, {}, genome, {{genomes::infantry::Team::Blue, 1}}});
    const auto target = infantry.spawn(
        {genomes::infantry::Team::Red,
         {100.0F, 0.0F, 0.0F},
         genome,
         {{genomes::infantry::Team::Red, 2}}});
    assert(observer && target);

    infantry.fixedUpdate(1.0 / 60.0, {});
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

    infantry.fixedUpdate(1.0 / 60.0, {});
    infantry.fixedUpdate(1.0 / 60.0, {1});
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

} // namespace

int main() {
    nearbyWaypointDoesNotMakeDistantTargetEngage();
    squadContactsRequireMatchingSideAndExplicitMembership();
    return 0;
}
