#include <genomes/foundation/BuildInfo.hpp>
#if GENOMES_HAS_INFANTRY
#include <genomes/gameplay/BattlefieldScenario.hpp>
#endif
#include <genomes/gameplay/WorldScenario.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/simulation/SimulationCommand.hpp>
#include <genomes/simulation/WorldEcs.hpp>

#include <algorithm>
#include <array>
#include <iostream>
#include <span>
#include <string_view>
#include <thread>
#include <vector>

namespace {

struct DemoPosition final {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
};

struct DemoVelocity final {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
};

} // namespace

int main(int argc, char** argv) {
    const genomes::foundation::BuildInfo info = genomes::foundation::buildInfo();
    std::cout << info.projectName << " native " << info.nativeBootstrapVersion << '\n';

    // The headless tool also exercises the low-level authoritative storage
    // without opening a window. Domain modules can adopt the same contract
    // incrementally while the legacy EntityStore remains available.
    genomes::simulation::WorldEcs ecs;
    const auto position_type = genomes::simulation::makeComponentType<DemoPosition>(
        "component.demo.position");
    const auto velocity_type = genomes::simulation::makeComponentType<DemoVelocity>(
        "component.demo.velocity");
    if (!ecs.registerComponent(position_type) || !ecs.registerComponent(velocity_type)) {
        return 1;
    }
    std::vector<genomes::simulation::ComponentTypeId> components{
        position_type.id, velocity_type.id};
    std::sort(components.begin(), components.end());
    DemoPosition initial_position{1.0F, 0.0F, 2.0F};
    DemoVelocity initial_velocity{0.5F, 0.0F, -0.25F};
    const std::array<genomes::simulation::ComponentInit, 2> initial{{
        {position_type.id, &initial_position},
        {velocity_type.id, &initial_velocity},
    }};
    const auto entity = ecs.create({components}, initial);
    if (!entity || ecs.component<DemoPosition>(entity.value(), position_type.id) == nullptr) {
        return 1;
    }
    const auto query = ecs.compileQuery({{position_type.id}, {}, {}});
    if (!query || !ecs.forEachChunk(query.value(), [](genomes::simulation::QueryChunkView view) {
            return view.size() > 0;
        })) {
        return 1;
    }
    const auto movable = ecs.create({{position_type.id}},
                                    std::span<const genomes::simulation::ComponentInit>{
                                        initial.data(), 1});
    if (!movable || !ecs.addComponent(movable.value(), velocity_type.id, &initial_velocity) ||
        ecs.component<DemoVelocity>(movable.value(), velocity_type.id) == nullptr ||
        !ecs.removeComponent(movable.value(), position_type.id) ||
        ecs.component<DemoPosition>(movable.value(), position_type.id) != nullptr ||
        !ecs.destroy(movable.value()) || ecs.contains(movable.value())) {
        return 1;
    }
    genomes::simulation::CommandBuffer commands;
    auto writer = commands.writer(1, genomes::foundation::stable_id("demo.system"), 1,
                                  {1});
    const auto token = writer.create({{position_type.id}});
    writer.add(token, velocity_type.id, initial_velocity);
    writer.destroy(token);
    writer.add(token, velocity_type.id, initial_velocity); // destroy dominates this mutation
    genomes::simulation::CommandBuffer* command_buffers[] = {&commands};
    const auto committed = genomes::simulation::CommandCommitter{}.commit(
        ecs, std::span<genomes::simulation::CommandBuffer* const>{command_buffers});
    if (!committed || committed.value().created != 1 || committed.value().destroyed != 1 ||
        committed.value().rejected != 1 || ecs.entityCount() != 1) {
        return 1;
    }
    std::cout << "ecs entities: " << ecs.entityCount() << '\n';

    // The optional world path is intentionally part of the tiny headless
    // executable: it gives CI and developers one deterministic, renderer-free
    // way to observe the same terrain/settlement artifact consumed by the
    // battlefield scene.
    if (argc > 1 && argv != nullptr && argv[1] != nullptr &&
        std::string_view{argv[1]} == "--world") {
        genomes::jobs::JobSystem world_jobs(2U);
        genomes::gameplay::WorldScenario world(world_jobs);
        genomes::world::WorldGenerationRequest request{};
        request.seed = 0x5EED2026ULL;
        if (!world.startNew(request)) {
            std::cerr << "world generation failed: " << world.status().last_error.message << '\n';
            return 1;
        }
        const auto* artifact = world.activeArtifact();
        const auto semantic = world.semanticSnapshot();
        if (artifact == nullptr || !artifact->valid()) {
            std::cerr << "world artifact is invalid\n";
            return 1;
        }
        for (std::size_t attempt = 0U; attempt < 10000U &&
             world.status().streaming_pending != 0U; ++attempt) {
            const auto streamed = world.poll();
            if (!streamed) {
                std::cerr << "world streaming failed: " << streamed.error().message << '\n';
                return 1;
            }
            std::this_thread::yield();
        }
        std::cout << "world hash=" << semantic.content_hash << " seed=" << request.seed
                  << " map=" << semantic.map_size_m
                  << " terrain_samples=" << semantic.terrain_sample_count
                  << " rivers=" << semantic.river_count
                  << " roads=" << semantic.road_count
                  << " parcels=" << semantic.parcel_count
                  << " buildings=" << semantic.building_count
                  << " sites=" << semantic.building_site_count
                  << " vegetation=" << semantic.vegetation_count
                  << " streamed_resident=" << world.status().streaming_resident
                  << " streamed_pending=" << world.status().streaming_pending
                  << " save_bytes=" << artifact->save_package.size()
                  << " terrain_mesh_vertices=" << artifact->terrain_mesh->vertices.size()
                  << " terrain_mesh_triangles=" << artifact->terrain_mesh->triangle_count()
                  << '\n';
    }
#if GENOMES_HAS_INFANTRY
    if (argc <= 1 || argv == nullptr || argv[1] == nullptr ||
        std::string_view{argv[1]} == "--battlefield") {
        auto battlefield = genomes::gameplay::startBattlefieldScenario(
            {.seed = 0xC0FFEEU,
             .map_size_m = 25U,
             .fixed_step_seconds = 1.0F / 60.0F,
             .max_ticks = 180U});
        if (!battlefield) {
            std::cerr << "battlefield scenario start failed: " << battlefield.error().message
                      << '\n';
            return 1;
        }
        auto scenario = std::move(battlefield.value());
        while (!scenario->complete()) {
            scenario->fixedUpdate();
        }
        const auto& battle = scenario->snapshot();
        std::cout << "battlefield 25x25: ticks=" << battle.tick
                  << " spawned=" << battle.spawned << " perceived=" << battle.perceived
                  << " intents=" << battle.intents << " fired=" << battle.fired
                  << " projectiles=" << battle.active_projectiles
                  << " impacts=" << battle.impacts << " damage=" << battle.accepted_damage
                  << " deaths=" << battle.deaths << " destruction=" << battle.destruction_damage
                  << " holes=" << battle.destruction_holes
                  << '\n';
        if (!battle.error.empty() || battle.deaths == 0U || battle.impacts == 0U) {
            std::cerr << "battlefield scenario did not reach impact/death"
                      << (battle.error.empty() ? "" : ": " + battle.error) << '\n';
            return 1;
        }
    }
#endif
    return 0;
}
