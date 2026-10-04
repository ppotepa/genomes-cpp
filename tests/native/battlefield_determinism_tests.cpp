#include <genomes/gameplay/BattlefieldRuntime.hpp>
#include <genomes/gameplay/ProductionGenerators.hpp>
#include <genomes/proc/ProceduralRuntime.hpp>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <utility>

namespace {

struct BattlefieldRunResult final {
    genomes::gameplay::BattlefieldScenarioSnapshot snapshot{};
    genomes::gameplay::BattlefieldRuntimeState state{
        genomes::gameplay::BattlefieldRuntimeState::Running};
    genomes::weapons::WeaponPoseTasks last_weapon_pose{};
    std::uint64_t simulation_hash{0U};
};

BattlefieldRunResult runBattlefield(genomes::gameplay::BattlefieldScenarioConfig config,
                                    std::uint32_t worker_count,
                                    genomes::gameplay::BattlefieldExecutionMode mode,
                                    bool use_procedural_runtime = false) {
    config.max_ticks = 8U;
    config.tactical_ai_profile.observation_period_ticks = 1U;
    config.tactical_ai_profile.memory_ticks = 8U;
    config.tactical_ai_profile.fire_alignment_cos = -1.0F;

    genomes::jobs::JobSystem jobs{worker_count};
    std::optional<genomes::proc::GeneratorRegistry> registry;
    std::optional<genomes::proc::ProceduralRuntime> procedural_runtime;
    if (use_procedural_runtime) {
        auto production = genomes::gameplay::makeProductionGeneratorRegistry();
        assert(production);
        registry.emplace(std::move(production.value()));
        procedural_runtime.emplace(*registry, jobs);
    }
    auto runtime = genomes::gameplay::BattlefieldRuntime::start(
        config, jobs, mode, procedural_runtime ? &*procedural_runtime : nullptr);
    if (!runtime) {
        std::fprintf(stderr, "battlefield start failed: %.*s\n",
                     static_cast<int>(runtime.error().message.size()),
                     runtime.error().message.data());
    }
    assert(runtime);
    runtime.value()->setSceneEpoch(17U);
    while (!runtime.value()->complete()) {
        runtime.value()->fixedUpdate();
    }
    const auto& snapshot = runtime.value()->snapshot();
    if (snapshot.fired == 0U) {
        std::fprintf(stderr, "battlefield produced no fire: tick=%llu perceived=%zu intents=%zu error=%.*s\n",
                     static_cast<unsigned long long>(snapshot.tick), snapshot.perceived,
                     snapshot.intents, static_cast<int>(snapshot.error.size()),
                     snapshot.error.data());
    }
    assert(snapshot.fired > 0U);
    const auto* pose = runtime.value()->weaponPoseTasks(snapshot.last_shot_source);
    assert(pose != nullptr && pose->valid());
    assert(pose->primary.owner == genomes::weapons::HandOwnership::Primary);
    assert(pose->support.owner == genomes::weapons::HandOwnership::Support);
    assert(pose->readiness >= 0.72F);
    assert(pose->recoil > 0.0F);
    assert(snapshot.fired <= snapshot.intents);
    const auto published = runtime.value()->simulationSnapshotExchange().acquireLatestRead();
    assert(published);
    assert(published.value().snapshot().metadata.tick == snapshot.tick);
    assert(published.value().snapshot().metadata.scene_epoch == 17U);
    assert(published.value().snapshot().entities.size() == snapshot.ecs_entities);
    assert(published.value().snapshot().semantic_hash != 0U);
    const auto published_presentation = runtime.value()->presentationSnapshotExchange().acquireLatestRead();
    assert(published_presentation);
    assert(published_presentation.value().snapshot().metadata.tick == snapshot.tick);
    assert(published_presentation.value().snapshot().metadata.scene_epoch == 17U);
    assert(published_presentation.value().snapshot().states.size() == snapshot.ecs_entities);
    runtime.value()->setSceneEpoch(18U);
    assert(runtime.value()->presentationSnapshot().states.empty());
    assert(runtime.value()->simulationSnapshot().entities.empty());
    runtime.value()->setSceneEpoch(17U);
    assert(runtime.value()->sceneEpoch() == 18U);
    return {snapshot, runtime.value()->state(), *pose,
            published.value().snapshot().semantic_hash};
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
    assert(expected.last_shot_source == actual.last_shot_source);
    assert(expected.last_impact_source == actual.last_impact_source);
    assert(expected.last_impact_target == actual.last_impact_target);
    assert(expected.complete == actual.complete);
    assert(expected.error == actual.error);
}

void assertSameWeaponPose(const genomes::weapons::WeaponPoseTasks& expected,
                          const genomes::weapons::WeaponPoseTasks& actual) {
    assert(expected.weapon_id == actual.weapon_id);
    assert(expected.primary.owner == actual.primary.owner);
    assert(expected.support.owner == actual.support.owner);
    assert(std::abs(expected.recoil - actual.recoil) < 1.0e-6F);
    assert(std::abs(expected.readiness - actual.readiness) < 1.0e-6F);
    assert(expected.aim_direction.x == actual.aim_direction.x);
    assert(expected.aim_direction.y == actual.aim_direction.y);
    assert(expected.aim_direction.z == actual.aim_direction.z);
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
        config, 4U, gameplay::BattlefieldExecutionMode::Parallel, true);
    const auto two_worker_result = runBattlefield(
        config, 2U, gameplay::BattlefieldExecutionMode::Parallel);

    assert(inline_result.state == gameplay::BattlefieldRuntimeState::Completed);
    assert(one_worker_result.state == gameplay::BattlefieldRuntimeState::Completed);
    assert(many_worker_result.state == gameplay::BattlefieldRuntimeState::Completed);
    assert(two_worker_result.state == gameplay::BattlefieldRuntimeState::Completed);
    assertSameSnapshot(inline_result.snapshot, one_worker_result.snapshot);
    assertSameSnapshot(inline_result.snapshot, many_worker_result.snapshot);
    assertSameSnapshot(inline_result.snapshot, two_worker_result.snapshot);
    assertSameWeaponPose(inline_result.last_weapon_pose, one_worker_result.last_weapon_pose);
    assertSameWeaponPose(inline_result.last_weapon_pose, many_worker_result.last_weapon_pose);
    assertSameWeaponPose(inline_result.last_weapon_pose, two_worker_result.last_weapon_pose);
    assert(inline_result.simulation_hash == one_worker_result.simulation_hash);
    assert(inline_result.simulation_hash == two_worker_result.simulation_hash);
    assert(inline_result.simulation_hash == many_worker_result.simulation_hash);
    return 0;
}
