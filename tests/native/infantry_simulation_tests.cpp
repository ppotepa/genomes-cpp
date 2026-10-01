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

    const auto observer = infantry.spawn({genomes::infantry::Team::Blue, {}, genome, 1});
    const auto target = infantry.spawn(
        {genomes::infantry::Team::Red, {100.0F, 0.0F, 0.0F}, genome, 2});
    assert(observer && target);

    infantry.fixedUpdate(1.0 / 60.0, {});
    const auto& states = infantry.renderStates();
    assert(states.size() == 2);
    const auto observer_state = states[0].entity == observer.value() ? states[0] : states[1];
    assert(observer_state.state == genomes::infantry::AgentState::Advance);
}

} // namespace

int main() {
    nearbyWaypointDoesNotMakeDistantTargetEngage();
    return 0;
}
