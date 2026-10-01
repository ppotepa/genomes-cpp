#include <genomes/gameplay/BattlefieldRuntime.hpp>

#include <cassert>
#include <cstdint>

namespace {

struct BattlefieldRunResult final {
    genomes::gameplay::BattlefieldScenarioSnapshot snapshot{};
    genomes::gameplay::BattlefieldRuntimeState state{
        genomes::gameplay::BattlefieldRuntimeState::Running};
};

BattlefieldRunResult runBattlefield(genomes::gameplay::BattlefieldScenarioConfig config,
                                    std::uint32_t worker_count,
                                    genomes::gameplay::BattlefieldExecutionMode mode) {
    config.max_ticks = 8U;
    config.tactical_ai_profile.observation_period_ticks = 1U;
    config.tactical_ai_profile.memory_ticks = 8U;
    config.tactical_ai_profile.fire_alignment_cos = -1.0F;

    genomes::jobs::JobSystem jobs{worker_count};
    auto runtime = genomes::gameplay::BattlefieldRuntime::start(config, &jobs, mode);
    assert(runtime);
    while (!runtime.value()->complete()) {
        runtime.value()->fixedUpdate();
    }
    return {runtime.value()->snapshot(), runtime.value()->state()};
}

void assertSameSnapshot(const genomes::gameplay::BattlefieldScenarioSnapshot& expected,
                        const genomes::gameplay::BattlefieldScenarioSnapshot& actual) {
    assert(expected.tick == actual.tick);
    assert(expected.map_size_m == actual.map_size_m);
    assert(expected.ecs_entities == actual.ecs_entities);
    assert(expected.spawned == actual.spawned);
    assert(expected.perceived == actual.perceived);
    assert(expected.intents == actual.intents);
    assert(expected.fired == actual.fired);
    assert(expected.active_projectiles == actual.active_projectiles);
    assert(expected.physics_steps == actual.physics_steps);
    assert(expected.impacts == actual.impacts);
    assert(expected.accepted_damage == actual.accepted_damage);
    assert(expected.deaths == actual.deaths);
    assert(expected.alive_units == actual.alive_units);
    assert(expected.destruction_damage == actual.destruction_damage);
    assert(expected.destruction_holes == actual.destruction_holes);
    assert(expected.complete == actual.complete);
    assert(expected.error == actual.error);
}

} // namespace

int main() {
    using namespace genomes;

    const gameplay::BattlefieldScenarioConfig config{};
    const auto inline_result = runBattlefield(
        config, 1U, gameplay::BattlefieldExecutionMode::Inline);
    const auto one_worker_result = runBattlefield(
        config, 1U, gameplay::BattlefieldExecutionMode::Parallel);
    const auto many_worker_result = runBattlefield(
        config, 4U, gameplay::BattlefieldExecutionMode::Parallel);

    assert(inline_result.state == gameplay::BattlefieldRuntimeState::Completed);
    assert(one_worker_result.state == gameplay::BattlefieldRuntimeState::Completed);
    assert(many_worker_result.state == gameplay::BattlefieldRuntimeState::Completed);
    assertSameSnapshot(inline_result.snapshot, one_worker_result.snapshot);
    assertSameSnapshot(inline_result.snapshot, many_worker_result.snapshot);
    return 0;
}
