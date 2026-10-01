#include <genomes/gameplay/BattlefieldScenario.hpp>
#include <genomes/gameplay/BattlefieldRuntime.hpp>

#include <cassert>

int main() {
    using namespace genomes;

    gameplay::BattlefieldScenarioConfig invalid{};
    invalid.tactical_ai_profile.observation_period_ticks = 0U;
    assert(!gameplay::startBattlefieldScenario(invalid));

    gameplay::BattlefieldScenarioConfig configured{};
    configured.tactical_ai_profile.observation_period_ticks = 6U;
    configured.tactical_ai_profile.memory_ticks = 90U;
    configured.tactical_ai_profile.target_switch_ratio = 0.75F;
    configured.tactical_ai_profile.fire_alignment_cos = 0.95F;
    auto scenario = gameplay::startBattlefieldScenario(configured);
    assert(scenario);
    const auto& profile = scenario.value()->tacticalProfile();
    assert(profile.observation_period_ticks == 6U);
    assert(profile.memory_ticks == 90U);
    assert(profile.target_switch_ratio == 0.75F);
    assert(profile.fire_alignment_cos == 0.95F);
    auto runtime = gameplay::BattlefieldRuntime::start(configured);
    assert(runtime);
    runtime.value()->fixedUpdate();
    assert(runtime.value()->snapshot().tick == 1U);
    assert(runtime.value()->snapshot().physics_steps == 1U);
    runtime.value()->fixedUpdate(simulation::TickContext{
        foundation::SimulationTick{2U}, 1.0 / 60.0,
        simulation::SessionSimulationTickRateHz});
    assert(runtime.value()->snapshot().tick == 2U);
    runtime.value()->fixedUpdate(simulation::TickContext{
        foundation::SimulationTick{2U}, 1.0 / 60.0,
        simulation::SessionSimulationTickRateHz});
    assert(runtime.value()->snapshot().tick == 2U);
    runtime.value()->fixedUpdate(simulation::TickContext{
        foundation::SimulationTick{1U}, 1.0 / 60.0,
        simulation::SessionSimulationTickRateHz});
    assert(runtime.value()->snapshot().tick == 2U);
    runtime.value()->fixedUpdate(simulation::TickContext{
        foundation::SimulationTick{3U}, 1.0 / 60.0, 0U});
    assert(runtime.value()->snapshot().tick == 2U);
    return 0;
}
