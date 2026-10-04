#include <genomes/gameplay/BattlefieldSession.hpp>
#include <genomes/simulation/EntityController.hpp>

#include <cassert>

int main() {
    using namespace genomes;

    gameplay::InfantryMassBattleConfig config{};
    config.units_per_team = 1U;
    auto session_result = gameplay::BattlefieldSession::startMassBattle(config, nullptr);
    assert(session_result);
    auto& session = *session_result.value();
    assert(session.isMassBattle());
    session.setSceneEpoch(17U);

    const auto& initial = session.presentationSnapshot();
    assert(initial.states.size() == 2U);
    const auto first = initial.states.front();

    simulation::EntityOrder order{};
    order.entity = first.entity;
    order.kind = simulation::EntityOrderKind::MoveTo;
    order.destination = {first.position.x + 30.0F, first.position.y, first.position.z + 30.0F};
    order.expires_tick = 20U;
    api::CommandEnvelope command{};
    command.module = foundation::stable_id("module.infantry");
    command.verb = foundation::stable_id("units.issue");
    command.target_tick = foundation::SimulationTick{2U};
    command.source = foundation::stable_id("test.session.input");
    command.payload = api::EncodedValue::binary(simulation::encodeEntityOrder(order));
    assert(session.submit(std::move(command)).accepted);

    const simulation::TickContext tick_one{
        foundation::SimulationTick{1U}, 1.0 / 60.0, simulation::SessionSimulationTickRateHz};
    const simulation::TickContext tick_two{
        foundation::SimulationTick{2U}, 1.0 / 60.0, simulation::SessionSimulationTickRateHz};
    assert(session.advance(tick_one));
    const auto first_view = session.snapshotView();
    assert(first_view.owner && first_view.tick.value == 1U && first_view.scene_epoch == 17U);
    assert(session.advance(tick_two));

    const auto& published = session.presentationSnapshot();
    assert(published.metadata.tick == 2U);
    assert(published.metadata.scene_epoch == 17U);
    assert(published.states.size() == 2U);
    assert(session.snapshotView().semantic_hash != first_view.semantic_hash);
    return 0;
}
