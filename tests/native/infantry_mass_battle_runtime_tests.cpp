#include <genomes/gameplay/InfantryMassBattleRuntime.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <unordered_set>

namespace {

using Runtime = genomes::gameplay::InfantryMassBattleRuntime;
using Config = genomes::gameplay::InfantryMassBattleConfig;
using State = genomes::gameplay::InfantryMassBattleRenderState;

void assertSameStates(const std::vector<State>& expected,
                      const std::vector<State>& actual) {
    assert(expected.size() == actual.size());
    for (std::size_t index = 0U; index < expected.size(); ++index) {
        const State& lhs = expected[index];
        const State& rhs = actual[index];
        assert(lhs.entity == rhs.entity);
        assert(lhs.team == rhs.team);
        assert(lhs.position.x == rhs.position.x);
        assert(lhs.position.y == rhs.position.y);
        assert(lhs.position.z == rhs.position.z);
        assert(lhs.heading == rhs.heading);
        assert(lhs.animation_phase == rhs.animation_phase);
        assert(lhs.animation_speed == rhs.animation_speed);
        assert(lhs.animation_variant == rhs.animation_variant);
    }
}

} // namespace

int main() {
    using namespace genomes;

    Config config{};
    config.seed = 0xA11CE55U;
    config.map_size_m = 512U;
    auto first_result = Runtime::start(config);
    auto second_result = Runtime::start(config);
    jobs::JobSystem parallel_jobs(4);
    auto parallel_result = Runtime::start(config, &parallel_jobs);
    assert(first_result);
    assert(second_result);
    assert(parallel_result);

    Runtime& first = *first_result.value();
    Runtime& second = *second_result.value();
    Runtime& parallel = *parallel_result.value();
    first.setSceneEpoch(23U);
    second.setSceneEpoch(23U);
    parallel.setSceneEpoch(23U);
    assert(first.entities().size() == 2000U);
    assert(first.snapshot().total_units == 2000U);
    assert(first.snapshot().blue_units == 1000U);
    assert(first.snapshot().red_units == 1000U);
    assert(first.renderStates().size() == 2000U);
    assert(first.presentationSnapshot().states.size() == first.renderStates().size());
    assertSameStates(first.renderStates(), second.renderStates());

    constexpr float half_pi = 1.57079632679489661923F;
    for (const State& state : first.renderStates()) {
        if (state.team == infantry::Team::Blue) {
            assert(std::abs(state.heading - half_pi) < 1.0e-5F);
        } else {
            assert(std::abs(state.heading + half_pi) < 1.0e-5F);
        }
    }

    std::unordered_set<std::uint8_t> variants;
    float maximum_abs_x = 0.0F;
    float maximum_abs_z = 0.0F;
    for (const State& state : first.renderStates()) {
        variants.insert(state.animation_variant);
        const float half_map = static_cast<float>(config.map_size_m) * 0.5F;
        assert(state.position.x >= -half_map && state.position.x <= half_map);
        assert(state.position.z >= -half_map && state.position.z <= half_map);
        maximum_abs_x = std::max(maximum_abs_x, std::abs(state.position.x));
        maximum_abs_z = std::max(maximum_abs_z, std::abs(state.position.z));
    }
    assert(variants.size() == 8U);
    assert(maximum_abs_x < 200.0F);
    assert(maximum_abs_z < 60.0F);

    const auto initial_states = first.renderStates();
    for (std::uint64_t tick = 1U; tick <= 120U; ++tick) {
        const simulation::TickContext context{
            foundation::SimulationTick{tick},
            1.0 / 60.0,
            simulation::SessionSimulationTickRateHz};
        assert(first.fixedUpdate(context));
        assert(second.fixedUpdate(context));
        assert(parallel.fixedUpdate(context));
    }

    assert(first.snapshot().tick == 120U);
    assert(first.snapshot().direction_changes > 0U);
    assert(first.snapshot().average_speed_mps > 0.0F);
    assertSameStates(first.renderStates(), second.renderStates());
    assertSameStates(first.renderStates(), parallel.renderStates());
    assert(first.simulationSnapshot().metadata.tick == 120U);
    assert(first.simulationSnapshot().entities.size() == 2000U);
    assert(first.simulationSnapshot().semantic_hash != 0U);
    assert(first.simulationSnapshot().semantic_hash ==
           second.simulationSnapshot().semantic_hash);
    assert(first.simulationSnapshot().semantic_hash ==
           parallel.simulationSnapshot().semantic_hash);
    assert(first.presentationSnapshot().metadata.tick == 120U);
    assert(first.presentationSnapshot().metadata.scene_epoch == 23U);
    const auto published_presentation = first.presentationSnapshotExchange().acquireLatestRead();
    assert(published_presentation);
    assert(published_presentation.value().snapshot().metadata.tick == 120U);
    assert(published_presentation.value().snapshot().metadata.scene_epoch == 23U);
    assert(published_presentation.value().snapshot().states.size() == 2000U);
    const auto published = first.simulationSnapshotExchange().acquireLatestRead();
    assert(published);
    assert(published.value().snapshot().metadata.tick == 120U);
    assert(published.value().snapshot().metadata.scene_epoch == 23U);
    assert(published.value().snapshot().entities.size() == 2000U);
    assert(published.value().snapshot().semantic_hash ==
           first.simulationSnapshot().semantic_hash);
    assert(first.simulationSnapshot().metadata.generation ==
           published.value().snapshot().metadata.generation);

    bool prone_move_is_locomoting = false;
    for (const State& state : first.renderStates()) {
        if (state.animation_variant != 6U) continue;
        const auto entity = std::find_if(
            first.simulationSnapshot().entities.begin(),
            first.simulationSnapshot().entities.end(),
            [&state](const simulation::SimulationSnapshotEntity& candidate) {
                return candidate.entity == state.entity;
            });
        assert(entity != first.simulationSnapshot().entities.end());
        const float speed = std::sqrt(entity->velocity.x * entity->velocity.x +
                                      entity->velocity.z * entity->velocity.z);
        assert(speed > 0.05F && speed < 0.5F);
        prone_move_is_locomoting = true;
        break;
    }
    assert(prone_move_is_locomoting);

    bool moved = false;
    bool phase_advanced = false;
    bool changed_posture = false;
    bool turned = false;
    for (std::size_t index = 0U; index < initial_states.size(); ++index) {
        const State& before = initial_states[index];
        const State& after = first.renderStates()[index];
        if (before.position.x != after.position.x || before.position.z != after.position.z) {
            moved = true;
        }
        if (before.animation_phase != after.animation_phase) phase_advanced = true;
        changed_posture = changed_posture || before.animation_variant != after.animation_variant;
        turned = turned || std::abs(before.heading - after.heading) > 0.2F;
        const float dx = after.position.x - before.position.x;
        const float dz = after.position.z - before.position.z;
        if (dx * dx + dz * dz > 0.01F) {
            assert(dx * std::sin(after.heading) + dz * std::cos(after.heading) > -0.1F);
        }
    }
    assert(moved);
    assert(phase_advanced);
    assert(changed_posture);
    assert(turned);
    first.setSceneEpoch(24U);
    assert(first.presentationSnapshot().states.empty());
    assert(first.simulationSnapshot().entities.empty());
    first.setSceneEpoch(23U);
    const simulation::TickContext stale_epoch_context{
        foundation::SimulationTick{121U},
        1.0 / 60.0,
        simulation::SessionSimulationTickRateHz};
    assert(first.fixedUpdate(stale_epoch_context));
    assert(first.presentationSnapshot().metadata.scene_epoch == 24U);
    assert(first.simulationSnapshot().metadata.scene_epoch == 24U);

    Config variety_config = config;
    variety_config.units_per_team = 100U;
    auto variety_result = Runtime::start(variety_config);
    assert(variety_result);
    Runtime& variety = *variety_result.value();
    std::unordered_set<std::uint8_t> observed_variants;
    bool observed_turn = false;
    for (std::uint64_t tick = 1U; tick <= 720U; ++tick) {
        const simulation::TickContext context{
            foundation::SimulationTick{tick},
            1.0 / 60.0,
            simulation::SessionSimulationTickRateHz};
        assert(variety.fixedUpdate(context));
        for (const State& state : variety.renderStates()) {
            observed_variants.insert(state.animation_variant);
            observed_turn = observed_turn || std::abs(state.heading) < 1.0F;
            const float limit = static_cast<float>(variety_config.map_size_m) * 0.5F - 12.0F;
            assert(std::abs(state.position.x) <= limit);
            assert(std::abs(state.position.z) <= limit);
        }
    }
    assert(observed_variants.size() == 8U);
    assert(observed_turn);

    Config command_config = config;
    command_config.units_per_team = 1U;
    auto command_result = Runtime::start(command_config);
    auto baseline_command_result = Runtime::start(command_config);
    assert(command_result);
    assert(baseline_command_result);
    Runtime& commanded = *command_result.value();
    Runtime& baseline_commanded = *baseline_command_result.value();
    const auto command_entity = commanded.renderStates().front().entity;
    const auto before_command = commanded.renderStates().front().position;
    simulation::EntityOrder order{};
    order.entity = command_entity;
    order.kind = simulation::EntityOrderKind::MoveTo;
    order.destination = {before_command.x + 30.0F, before_command.y,
                         before_command.z + 30.0F};
    order.expires_tick = 20U;
    api::CommandEnvelope envelope{};
    envelope.module = foundation::stable_id("module.infantry");
    envelope.verb = foundation::stable_id("units.issue");
    envelope.target_tick = foundation::SimulationTick{2U};
    envelope.source = foundation::stable_id("test.input");
    const auto encoded_order = simulation::encodeEntityOrder(order);
    envelope.payload = api::EncodedValue::binary(encoded_order);
    assert(commanded.submit(envelope).accepted);
    assert(commanded.renderStates().front().position.x == before_command.x);
    const simulation::TickContext command_tick_one{
        foundation::SimulationTick{1U}, 1.0 / 60.0,
        simulation::SessionSimulationTickRateHz};
    const simulation::TickContext command_tick_two{
        foundation::SimulationTick{2U}, 1.0 / 60.0,
        simulation::SessionSimulationTickRateHz};
    assert(commanded.fixedUpdate(command_tick_one));
    assert(baseline_commanded.fixedUpdate(command_tick_one));
    assert(commanded.fixedUpdate(command_tick_two));
    assert(baseline_commanded.fixedUpdate(command_tick_two));
    assert(commanded.simulationSnapshot().semantic_hash !=
           baseline_commanded.simulationSnapshot().semantic_hash);

    auto world_command_result = Runtime::start(command_config);
    auto world_baseline_result = Runtime::start(command_config);
    assert(world_command_result && world_baseline_result);
    Runtime& world_commanded = *world_command_result.value();
    Runtime& world_baseline = *world_baseline_result.value();
    constexpr std::array<std::uint8_t, 3U> weather_value{4U, 2U, 7U};
    const std::array world_values{api::EncodedValue::string("weather.phase"),
                                  api::EncodedValue::binary(weather_value)};
    api::CommandEnvelope world_set{};
    world_set.module = foundation::stable_id("module.world");
    world_set.verb = foundation::stable_id("world.set");
    world_set.target_tick = foundation::SimulationTick{2U};
    world_set.source = foundation::stable_id("test.world");
    world_set.payload = api::EncodedValue::tuple(world_values);
    assert(world_set.payload.valid());
    assert(world_commanded.submit(std::move(world_set)).accepted);
    assert(world_commanded.fixedUpdate(command_tick_one));
    assert(world_baseline.fixedUpdate(command_tick_one));
    assert(world_commanded.simulationSnapshot().semantic_hash ==
           world_baseline.simulationSnapshot().semantic_hash);
    const auto immutable_view = world_commanded.snapshotView();
    assert(immutable_view.owner && immutable_view.tick.value == 1U && !immutable_view.bytes.empty());
    const std::vector<std::uint8_t> immutable_bytes{immutable_view.bytes.begin(),
                                                     immutable_view.bytes.end()};
    assert(world_commanded.fixedUpdate(command_tick_two));
    assert(world_baseline.fixedUpdate(command_tick_two));
    const std::vector<std::uint8_t> retained_bytes{immutable_view.bytes.begin(),
                                                   immutable_view.bytes.end()};
    assert(retained_bytes == immutable_bytes);
    const auto weather = world_commanded.query(
        foundation::stable_id("world.query"), api::EncodedValue::string("weather.phase"));
    assert(weather.owner && weather.tick.value == 2U);
    const std::vector<std::uint8_t> queried_weather{weather.bytes.begin(), weather.bytes.end()};
    assert(queried_weather == std::vector<std::uint8_t>({4U, 2U, 7U}));
    assert(world_commanded.simulationSnapshot().semantic_hash !=
           world_baseline.simulationSnapshot().semantic_hash);

    const auto runConcurrentWorldCommands = [&](std::uint32_t worker_count) {
        jobs::JobSystem command_jobs(worker_count);
        auto runtime_result = Runtime::start(command_config, &command_jobs);
        assert(runtime_result);
        auto& runtime = *runtime_result.value();
        constexpr std::array<std::uint8_t, 1U> first_value{1U};
        constexpr std::array<std::uint8_t, 1U> second_value{2U};
        constexpr std::array<std::uint8_t, 1U> third_value{3U};
        const std::array values{api::EncodedValue::binary(first_value),
                                api::EncodedValue::binary(second_value),
                                api::EncodedValue::binary(third_value)};
        std::array<api::CommandEnvelope, 3U> commands{};
        // Submission order is deliberately not source order. The envelope
        // key, rather than worker scheduling, chooses the final value.
        for (std::size_t index = 0U; index < commands.size(); ++index) {
            commands[index].module = foundation::stable_id("module.world");
            commands[index].verb = foundation::stable_id("world.set");
            commands[index].target_tick = foundation::SimulationTick{2U};
            commands[index].source = std::array<foundation::StableId, 3U>{30U, 10U, 20U}[index];
            commands[index].sequence = 100U + index;
            const std::array payload{api::EncodedValue::string("weather.phase"), values[index]};
            commands[index].payload = api::EncodedValue::tuple(payload);
        }
        std::array<jobs::JobHandle, 3U> submissions{};
        for (std::size_t index = 0U; index < commands.size(); ++index) {
            submissions[index] = command_jobs.submit(
                [&runtime, command = commands[index]](jobs::JobContext&) mutable {
                    (void)runtime.submit(std::move(command));
                });
        }
        for (const jobs::JobHandle& submission : submissions) submission.wait();
        assert(runtime.fixedUpdate(command_tick_one));
        assert(runtime.fixedUpdate(command_tick_two));
        const auto phase = runtime.query(foundation::stable_id("world.query"),
                                         api::EncodedValue::string("weather.phase"));
        const std::vector<std::uint8_t> phase_bytes{phase.bytes.begin(), phase.bytes.end()};
        assert(phase_bytes == std::vector<std::uint8_t>({1U}));
        return runtime.simulationSnapshot().semantic_hash;
    };
    const std::uint64_t one_worker_command_hash = runConcurrentWorldCommands(1U);
    const std::uint64_t two_worker_command_hash = runConcurrentWorldCommands(2U);
    const std::uint64_t four_worker_command_hash = runConcurrentWorldCommands(4U);
    assert(one_worker_command_hash == two_worker_command_hash);
    assert(one_worker_command_hash == four_worker_command_hash);
    return 0;
}
