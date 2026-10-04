#include <genomes/gameplay/BattlefieldScenario.hpp>
#include <genomes/gameplay/BattlefieldRuntime.hpp>
#include <genomes/world/WorldArtifactRevision.hpp>
#include <genomes/jobs/JobSystem.hpp>

#include <cassert>
#include <cstdint>

int main() {
    using namespace genomes;

    jobs::JobSystem jobs(1);

    gameplay::BattlefieldScenarioConfig invalid{};
    invalid.tactical_ai_profile.observation_period_ticks = 0U;
    assert(!gameplay::startBattlefieldScenario(invalid, jobs));

    gameplay::BattlefieldScenarioConfig configured{};
    configured.tactical_ai_profile.observation_period_ticks = 6U;
    configured.tactical_ai_profile.memory_ticks = 90U;
    configured.tactical_ai_profile.target_switch_ratio = 0.75F;
    configured.tactical_ai_profile.fire_alignment_cos = 0.95F;
    auto scenario = gameplay::startBattlefieldScenario(configured, jobs);
    assert(scenario);
    const auto& profile = scenario.value()->tacticalProfile();
    assert(profile.observation_period_ticks == 6U);
    assert(profile.memory_ticks == 90U);
    assert(profile.target_switch_ratio == 0.75F);
    assert(profile.fire_alignment_cos == 0.95F);
    scenario.value()->fixedUpdate();
    assert(scenario.value()->snapshot().tick == 1U);
    assert(scenario.value()->snapshot().physics_steps == 1U);
    auto runtime = gameplay::BattlefieldRuntime::start(configured, jobs);
    assert(runtime);
    assert(!runtime.value()->bindWorldArtifactRevision(0U));
    constexpr world::WorldArtifactRevision artifact_revision{0xA11CEU};
    assert(runtime.value()->bindWorldArtifactRevision(artifact_revision));
    assert(runtime.value()->worldArtifactRevision() == artifact_revision);
    assert(runtime.value()->bindWorldArtifactRevision(artifact_revision));
    assert(!runtime.value()->bindWorldArtifactRevision(artifact_revision + 1U));
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

    // T10: one authoritative EntityId must survive the complete weapon ->
    // ballistics -> impact -> damage path. A one-tick AI cadence and relaxed
    // alignment make this a deterministic pipeline regression rather than a
    // timing/sleep test.
    gameplay::BattlefieldScenarioConfig pipeline_config{};
    pipeline_config.max_ticks = 8U;
    pipeline_config.tactical_ai_profile.observation_period_ticks = 1U;
    pipeline_config.tactical_ai_profile.memory_ticks = 8U;
    pipeline_config.tactical_ai_profile.fire_alignment_cos = -1.0F;
    auto pipeline_runtime = gameplay::BattlefieldRuntime::start(pipeline_config, jobs);
    assert(pipeline_runtime);
    for (std::uint32_t tick = 0U;
         tick < pipeline_config.max_ticks && !pipeline_runtime.value()->complete();
         ++tick) {
        pipeline_runtime.value()->fixedUpdate();
    }
    const auto& pipeline_snapshot = pipeline_runtime.value()->snapshot();
    assert(pipeline_snapshot.fired > 0U);
    assert(pipeline_snapshot.impacts > 0U);
    assert(pipeline_snapshot.accepted_damage > 0U);
    assert(pipeline_snapshot.physics_steps == pipeline_snapshot.tick);
    assert(pipeline_snapshot.last_shot_source.isValid());
    assert(pipeline_snapshot.last_impact_source.isValid());
    assert(pipeline_runtime.value()->ecs().contains(pipeline_snapshot.last_shot_source));
    assert(pipeline_runtime.value()->ecs().contains(pipeline_snapshot.last_impact_source));
    assert(pipeline_snapshot.last_impact_target.isValid());
    assert(pipeline_runtime.value()->ecs().contains(pipeline_snapshot.last_impact_target));
    assert(pipeline_snapshot.last_impact_target != pipeline_snapshot.last_impact_source);
    return 0;
}
