#include <genomes/game_scenes/BattlefieldScene.hpp>
#include <genomes/game_scenes/ApplicationCommand.hpp>

#include <genomes/foundation/StableHash.hpp>
#include <genomes/runtime/InfantryPresentation.hpp>
#include <genomes/world/GridLayout.hpp>
#include <genomes/world_render/WorldMeshCompiler.hpp>
#if GENOMES_HAS_INFANTRY
#include <genomes/infantry/EquipmentCatalog.hpp>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <unordered_map>

namespace genomes::runtime {

namespace {

[[nodiscard]] float sample_ground(const void* context, float x, float z) noexcept {
    if (context == nullptr) {
        return 0.0F;
    }
    const auto* height_field = static_cast<const terrain::HeightField*>(context);
    return height_field->sampleBilinear(x, z);
}

[[nodiscard]] const char* kind_name(world::WorldFeatureKind kind) noexcept {
    switch (kind) {
    case world::WorldFeatureKind::TerrainPatch:
        return "terrain";
    case world::WorldFeatureKind::Road:
        return "road";
    case world::WorldFeatureKind::Parcel:
        return "parcel";
    case world::WorldFeatureKind::Building:
        return "building";
    case world::WorldFeatureKind::Vegetation:
        return "vegetation";
    case world::WorldFeatureKind::Fence:
        return "fence";
    }
    return "unknown";
}

[[nodiscard]] foundation::StableId mesh_id(world::WorldFeatureKind kind) noexcept {
    return foundation::stable_id(std::string("mesh.world.") + kind_name(kind));
}

[[nodiscard]] foundation::StableId material_id(world::WorldFeatureKind kind) noexcept {
    return foundation::stable_id(std::string("material.world.") + kind_name(kind));
}

[[nodiscard]] std::string feature_summary(const world::WorldPlan& plan) {
    return "Features: " + std::to_string(plan.features.size()) + "  Roads: " +
           std::to_string(plan.count(world::WorldFeatureKind::Road)) + "  Buildings: " +
           std::to_string(plan.count(world::WorldFeatureKind::Building)) + "  Vegetation: " +
           std::to_string(plan.count(world::WorldFeatureKind::Vegetation)) + "  Parcels: " +
           std::to_string(plan.city.parcels.size()) + "  Rivers: " +
           std::to_string(plan.hydrology.rivers.size());
}

} // namespace

foundation::SceneId BattlefieldScene::id() const noexcept {
    return foundation::scene_id("scene.battlefield");
}

void BattlefieldScene::on_enter(SceneContext& context) {
    jobs_ = context.deterministic_capture ? nullptr : context.jobs;
    elapsed_seconds_ = 0.0;
    generation_error_.clear();
    plan_.reset();
    resolved_buildings_.reset();
    scenario_.reset();
#if GENOMES_HAS_INFANTRY
    battlefield_runtime_.reset();
    auto viability = gameplay::BattlefieldRuntime::start(
        {.seed = config_.seed,
         .map_size_m = 25U, .fixed_step_seconds = 1.0F / 60.0F,
         .max_ticks = 240U, .tactical_ai_profile = tactical_ai_profile_},
        jobs_);
    if (viability) {
        battlefield_runtime_ = std::move(viability.value());
    } else {
        generation_error_ = std::string(viability.error().message);
    }
#endif
    terrain_.reset();
    terrain_mesh_.reset();
    world_artifacts_.reset();
    render_terrain_mesh_.reset();
    render_world_mesh_.reset();
    world_mesh_artifact_.reset();
#if GENOMES_HAS_INFANTRY
    render_infantry_mesh_.reset();
    infantry_skinned_prototype_.reset();
    infantry_model_artifact_.reset();
    infantry::InfantryModelRequest model_request{};
    model_request.seed = config_.seed;
    model_request.loadout_id = infantry::EquipmentCatalog::loadoutId("RIFLEMAN");
    if (auto model = infantry_model_compiler_.compile(model_request); model) {
        infantry_model_artifact_ = std::move(model.value().artifact);
    }
#endif
    camera_request_ = {};
    terrain_min_height_ = 0.0F;
    terrain_max_height_ = 0.0F;
    entities_.clear();
    physics_ = physics::SimplePhysicsWorld{};
#if GENOMES_HAS_INFANTRY
    infantry_.reset();
    animation_system_.reset();
    animation_agents_.clear();
    animation_poses_.clear();
#endif
    navigation_.reset();
    damage_buffer_.clear();
    command_buffers_.reset(0);
    simulation_tick_ = {};
    simulation_failed_ = false;
    configure_simulation_graph();

    if (jobs_ != nullptr) {
        scenario_ = std::make_unique<gameplay::WorldScenario>(*jobs_);
        const auto requested = scenario_->requestNew(config_);
        if (!requested) {
            generation_error_ = std::string(requested.error().message);
        }
    } else {
        const auto generated = world::WorldGenerator::generate(config_);
        if (generated) {
            finalize_plan(std::move(generated.value()));
        } else {
            generation_error_ = std::string(generated.error().message);
        }
    }
    context.ui.clear();
}

void BattlefieldScene::on_exit(SceneContext&) {
    plan_.reset();
    resolved_buildings_.reset();
    terrain_.reset();
    terrain_mesh_.reset();
    world_artifacts_.reset();
    render_terrain_mesh_.reset();
    render_world_mesh_.reset();
    world_mesh_artifact_.reset();
    render_infantry_mesh_.reset();
    infantry_skinned_prototype_.reset();
    camera_request_ = {};
#if GENOMES_HAS_INFANTRY
    infantry_.reset();
    animation_system_.reset();
    animation_agents_.clear();
    animation_poses_.clear();
#endif
    navigation_.reset();
    damage_buffer_.clear();
    simulation_graph_.clear();
    command_buffers_.reset(0);
    simulation_failed_ = false;
    physics_ = physics::SimplePhysicsWorld{};
    entities_.clear();
    region_streamer_.reset();
    scenario_.reset();
#if GENOMES_HAS_INFANTRY
    battlefield_runtime_.reset();
#endif
    jobs_ = nullptr;
}

#if GENOMES_HAS_INFANTRY
void BattlefieldScene::initialize_infantry_animation() {
    animation_system_.reset();
    animation_agents_.clear();
    animation_poses_.clear();
    if (!infantry_ || !infantry_model_artifact_) {
        return;
    }
    auto animation = infantry::AnimationSystem::create(64U);
    if (!animation) {
        generation_error_ = std::string(animation.error().message);
        return;
    }
    animation_system_ = std::move(animation.value());
    animation_agents_.reserve(infantry_->renderStates().size());
    for (const infantry::InfantryRenderState& state : infantry_->renderStates()) {
        auto locomotion = infantry::LocomotionController::create(
            infantry_model_artifact_->phenotype.body);
        auto face = infantry::FaceAnimator::create(
            proc::Seed(foundation::stableHashCombine(
                static_cast<std::uint64_t>(config_.seed), state.entity.packed())),
            infantry_model_artifact_->phenotype.face);
        if (!locomotion || !face) {
            generation_error_ = !locomotion ? std::string(locomotion.error().message)
                                            : std::string(face.error().message);
            continue;
        }
        InfantryAnimationAgent agent{};
        agent.entity = state.entity;
        agent.locomotion = std::move(locomotion.value());
        agent.locomotion_state = agent.locomotion->initialState();
        agent.face = std::move(face.value());
        agent.lod.setTier(infantry::AnimationLOD::Near);
        animation_agents_.push_back(std::move(agent));
    }
}

void BattlefieldScene::evaluate_infantry_animation(float fixed_dt_seconds) {
    if (!animation_system_ || !infantry_ || !infantry_model_artifact_ ||
        animation_agents_.empty()) {
        return;
    }
    std::unordered_map<std::uint64_t, const infantry::InfantryRenderState*> states;
    states.reserve(infantry_->renderStates().size());
    for (const auto& state : infantry_->renderStates()) {
        states.emplace(state.entity.packed(), &state);
    }

    const float map_size = std::max(1.0F, static_cast<float>(config_.map_size_m));
    const float near_distance = std::max(60.0F, map_size * 0.16F);
    const float mid_distance = std::max(near_distance * 2.0F, map_size * 0.42F);
    const float near2 = near_distance * near_distance;
    const float mid2 = mid_distance * mid_distance;

    std::vector<infantry::AnimationEntity> entities;
    entities.reserve(animation_agents_.size());
    for (InfantryAnimationAgent& agent : animation_agents_) {
        const auto state_found = states.find(agent.entity.packed());
        if (state_found == states.end() || !agent.locomotion ||
            !agent.locomotion_state || !agent.face) {
            continue;
        }
        const auto* state = state_found->second;
        const float dx = state->position.x - camera_request_.position.x;
        const float dy = state->position.y - camera_request_.position.y;
        const float dz = state->position.z - camera_request_.position.z;
        const float distance2 = dx * dx + dy * dy + dz * dz;
        const infantry::AnimationLOD desired_lod =
            distance2 <= near2 ? infantry::AnimationLOD::Near :
            distance2 <= mid2 ? infantry::AnimationLOD::Mid :
                                infantry::AnimationLOD::Far;
        if (agent.lod.tier() != desired_lod) agent.lod.setTier(desired_lod);

        infantry::BipedPreset preset = infantry::BipedPreset::Idle;
        switch (state->state) {
        case infantry::AgentState::Advance:
            preset = infantry::BipedPreset::Run;
            break;
        case infantry::AgentState::Engage:
            preset = infantry::BipedPreset::Crouch;
            break;
        case infantry::AgentState::Dead:
            preset = infantry::BipedPreset::Crouch;
            break;
        case infantry::AgentState::Idle:
            preset = infantry::BipedPreset::Idle;
            break;
        }
        (void)agent.locomotion->setPreset(*agent.locomotion_state, preset);
        (void)agent.locomotion->step(*agent.locomotion_state, fixed_dt_seconds);
        entities.push_back({
            foundation::stable_id("battlefield.infantry") ^ agent.entity.packed(),
            &infantry_model_artifact_->skeleton,
            &*agent.locomotion,
            &*agent.locomotion_state,
            &*agent.face,
            state->position,
            foundation::Vec3{state->position.x +
                                 std::sin(state->heading) * 6.0F,
                             state->position.y +
                                 infantry_model_artifact_->phenotype.body.height * 0.62F,
                             state->position.z +
                                 std::cos(state->heading) * 6.0F},
            agent.lod,
            &infantry_model_artifact_->appearance.body});
    }
    if (entities.empty()) {
        return;
    }
    const auto result = animation_system_->evaluate(
        std::span<infantry::AnimationEntity>(entities), simulation_tick_.value,
        fixed_dt_seconds, jobs_);
    if (!result) {
        generation_error_ = std::string(result.error().message);
        return;
    }
    animation_poses_ = animation_system_->currentSnapshot().poses;
}
#endif

void BattlefieldScene::handle_input(SceneContext& context, const input::InputFrame& input) {
    if (input.cancel_pressed || input.confirm_pressed) {
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::ReturnToMainMenu);
    }
}

void BattlefieldScene::fixed_update(SceneContext&, double dt) {
    if (simulation_failed_) {
        return;
    }
    elapsed_seconds_ += dt;
    simulation_tick_.increment();
#if !GENOMES_HAS_INFANTRY
    (void)dt;
    return;
#else
    const simulation::TickContext tick_context{
        simulation_tick_, dt, simulation::SessionSimulationTickRateHz};
    if (battlefield_runtime_ != nullptr) {
        if (!battlefield_runtime_->complete()) {
            // The runtime is the sole authoritative owner for this tick. The
            // scene graph remains a compatibility fallback until its state is
            // transferred into BattlefieldRuntime; running both would advance
            // two ECS/physics/combat pipelines for one session tick.
            battlefield_runtime_->fixedUpdate(tick_context);
        }
        if (!battlefield_runtime_->snapshot().error.empty()) {
            generation_error_ = "Battlefield runtime failed: " +
                                battlefield_runtime_->snapshot().error;
            simulation_failed_ = true;
        }
        evaluate_infantry_animation(static_cast<float>(dt));
        return;
    }
    if (!infantry_) {
        return;
    }

    if (simulation_graph_.compiled()) {
        const auto result = simulation_graph_.run(simulation_tick_, dt, jobs_, &command_buffers_);
        if (!result) {
            generation_error_ = "Simulation graph failed: " +
                                 std::string(result.error().message);
            command_buffers_.reset(0);
            simulation_failed_ = true;
        } else {
            const auto committed = simulation::CommandCommitter{}.commit(
                entities_.ecs(), command_buffers_.buffers());
            if (!committed) {
                generation_error_ = "Simulation command commit failed: " +
                                     std::string(committed.error().message);
            }
        }
    } else {
        // Keep a safe fallback for a partially constructed scene. Once the
        // graph is compiled all authoritative updates go through its phases.
        infantry_->fixedUpdate(dt, simulation_tick_);
        infantry_->stepPhysics(dt);
        // The compatibility graph has no authoritative weapon-to-damage
        // path. Keep its event buffer bounded until BattlefieldRuntime is
        // available; the legacy adapter is test-only and must not run here.
        damage_buffer_.clear();
    }
    evaluate_infantry_animation(static_cast<float>(dt));
#endif
}

void BattlefieldScene::configure_simulation_graph() {
    simulation_graph_.clear();

#if !GENOMES_HAS_INFANTRY
    return;
#else

    const auto add_system = [this](simulation::SystemDescriptor descriptor) {
        const auto result = simulation_graph_.add(std::move(descriptor));
        if (!result) {
            generation_error_ = "Simulation graph setup failed: " +
                                 std::string(result.error().message);
            return false;
        }
        return true;
    };

    constexpr simulation::CadencePolicy every_tick{simulation::CadenceKind::EveryTick};

    simulation::SystemDescriptor infantry_update{};
    infantry_update.id = foundation::stable_id("system.infantry.update");
    infantry_update.phase = simulation::SystemPhase::MoveIntent;
    infantry_update.access.reads = {
        foundation::stable_id("component.entity.health"),
        foundation::stable_id("resource.navigation"),
    };
    infantry_update.access.writes = {
        foundation::stable_id("component.entity.position"),
        foundation::stable_id("component.entity.velocity"),
        foundation::stable_id("resource.physics.commands"),
        foundation::stable_id("resource.infantry.state"),
    };
    infantry_update.cadence = every_tick;
    infantry_update.callback = [this](simulation::SystemContext& context) {
        if (infantry_) {
            infantry_->fixedUpdate(context.fixed_dt, context.tick);
        }
    };
    if (!add_system(std::move(infantry_update))) {
        return;
    }

    simulation::SystemDescriptor physics_step{};
    physics_step.id = foundation::stable_id("system.physics.step");
    physics_step.phase = simulation::SystemPhase::PhysicsStep;
    physics_step.access.reads = {foundation::stable_id("resource.physics.commands")};
    physics_step.access.writes = {foundation::stable_id("resource.physics.world")};
    physics_step.cadence = every_tick;
    physics_step.callback = [this](simulation::SystemContext& context) {
        if (infantry_) {
            infantry_->stepPhysics(context.fixed_dt);
        } else {
            physics_.step(static_cast<float>(context.fixed_dt));
        }
    };
    if (!add_system(std::move(physics_step))) {
        return;
    }

    simulation::SystemDescriptor emit_events{};
    emit_events.id = foundation::stable_id("system.combat.emit-events");
    emit_events.phase = simulation::SystemPhase::CombatBallistics;
    emit_events.access.reads = {
        foundation::stable_id("component.entity.position"),
        foundation::stable_id("resource.infantry.state"),
    };
    emit_events.access.writes = {foundation::stable_id("resource.combat.events")};
    emit_events.cadence = every_tick;
    emit_events.callback = [this](simulation::SystemContext& context) {
        if (infantry_) {
            infantry_->emitCombatEvents(context.tick, damage_buffer_);
        }
    };
    if (!add_system(std::move(emit_events))) {
        return;
    }

    simulation::SystemDescriptor apply_damage{};
    apply_damage.id = foundation::stable_id("system.combat.apply-damage");
    apply_damage.phase = simulation::SystemPhase::DamageDestruction;
    apply_damage.access.reads = {foundation::stable_id("resource.combat.events")};
    apply_damage.access.writes = {
        foundation::stable_id("component.entity.health"),
        foundation::stable_id("component.entity.flags"),
    };
    apply_damage.cadence = every_tick;
    apply_damage.callback = [this](simulation::SystemContext&) {
        damage_buffer_.clear();
    };
    if (!add_system(std::move(apply_damage))) {
        return;
    }

    const auto compiled = simulation_graph_.compile();
    if (!compiled) {
        generation_error_ = "Simulation graph compile failed: " +
                            std::string(compiled.error().message);
    }
#endif
}

void BattlefieldScene::frame_update(SceneContext& context, double) {
    if (scenario_) {
        const auto generated = scenario_->poll();
        if (!generated) {
            generation_error_ = std::string(generated.error().message);
        } else if (!plan_ && generated.value()) {
            if (const gameplay::WorldScenarioArtifact* artifact = scenario_->activeArtifact();
                artifact != nullptr) {
                finalize_plan(artifact->plan);
            }
        }
    } else if (!plan_ && region_streamer_) {
        region_streamer_->poll();
        auto ready_regions = region_streamer_->take_ready();
        if (!ready_regions.empty()) {
            finalize_plan(std::move(ready_regions.front().plan));
        }
    }

    context.ui.clear();
    auto& model = context.ui.model();
    (void)model.set("title", std::string{"BATTLEFIELD"});
    (void)model.set("description", std::string{"Procedural world plan"});
    (void)model.set("error", std::string{});
    (void)model.set("diagnostics", "Renderer uploads " + std::to_string(context.render_telemetry.mesh_uploads) +
                                  " | palette updates " + std::to_string(context.render_telemetry.palette_updates) +
                                  " | draws " + std::to_string(context.render_telemetry.draw_calls));
    if (plan_) {
        (void)model.set("seed", static_cast<std::int64_t>(plan_->seed));
        (void)model.set("map_size", static_cast<std::int64_t>(plan_->map_size_m));
        (void)model.set("features", feature_summary(*plan_));
#if GENOMES_HAS_INFANTRY
        const std::size_t infantry_count = infantry_ ? infantry_->activeCount() : 0U;
#else
        constexpr std::size_t infantry_count = 0U;
#endif
        (void)model.set("units", static_cast<std::int64_t>(infantry_count));
#if GENOMES_HAS_INFANTRY
        if (battlefield_runtime_ != nullptr) {
            const auto& viability = battlefield_runtime_->snapshot();
            (void)model.set("viability", "Combat slice: tick " + std::to_string(viability.tick) +
                                " fire " + std::to_string(viability.fired) +
                                " impact " + std::to_string(viability.impacts) +
                                " deaths " + std::to_string(viability.deaths));
        }
#endif
        (void)model.set("terrain", "Terrain: " + std::to_string(terrain_->width()) + " x " +
                            std::to_string(terrain_->height()) + " samples");
        (void)model.set("mesh", "Mesh: " + std::to_string(terrain_mesh_->vertices.size()) +
                            " vertices / " + std::to_string(terrain_mesh_->triangle_count()) + " triangles");
        (void)model.set("elevation", "Elevation: " + std::to_string(terrain_min_height_) + " .. " +
                            std::to_string(terrain_max_height_) + " m");
        (void)model.set("hash", static_cast<std::int64_t>(plan_->content_hash));
        (void)model.set("status", std::string{"World plan ready for terrain, navigation and rendering."});
    } else if (scenario_ && scenario_->status().generation_pending) {
        (void)model.set("status", std::string{"Generating world on worker threads..."});
    } else if (scenario_ && scenario_->status().streaming_pending > 0U) {
        (void)model.set("status", std::string{"Streaming adjacent world region..."});
    } else if (region_streamer_ && region_streamer_->pending_count() > 0) {
        (void)model.set("status", std::string{"Generating world on worker threads..."});
    } else if (region_streamer_ && region_streamer_->failed()) {
        (void)model.set("error", "World generation failed: " +
                            std::string(region_streamer_->error().message));
    } else {
        (void)model.set("error", "World generation failed: " + generation_error_);
    }
}

void BattlefieldScene::finalize_plan(world::WorldPlan plan) {
    plan_ = std::move(plan);
    resolved_buildings_.reset();
    terrain_.reset();
    terrain_mesh_.reset();
    world_artifacts_.reset();
    render_terrain_mesh_.reset();
    render_world_mesh_.reset();
    world_mesh_artifact_.reset();
    render_infantry_mesh_.reset();
    camera_request_ = {};
    const auto shared_artifact_handle =
        scenario_ != nullptr ? scenario_->activeArtifactHandle()
                              : std::shared_ptr<const gameplay::WorldScenarioArtifact>{};
    const gameplay::WorldScenarioArtifact* shared_artifact = shared_artifact_handle.get();
    if (shared_artifact != nullptr && shared_artifact->plan.content_hash == plan_->content_hash &&
        shared_artifact->revision == world::artifactRevision(*plan_) &&
        shared_artifact->terrain != nullptr && shared_artifact->terrain_mesh != nullptr &&
        shared_artifact->resolved_buildings != nullptr &&
        shared_artifact->resolved_buildings->size() == plan_->building_sites.size()) {
        world_artifacts_ = shared_artifact_handle;
        terrain_ = shared_artifact->terrain;
        terrain_mesh_ = shared_artifact->terrain_mesh;
        resolved_buildings_ = shared_artifact->resolved_buildings;
    } else if (scenario_ != nullptr) {
        generation_error_ = "world scenario did not publish a matching resolved artifact";
        plan_.reset();
        return;
    } else {
        const auto resolved = gameplay::WorldScenario::compileArtifact(*plan_, config_);
        if (!resolved) {
            generation_error_ = std::string(resolved.error().message);
            plan_.reset();
            return;
        }
        world_artifacts_ = std::make_shared<const gameplay::WorldScenarioArtifact>(
            std::move(resolved.value()));
        terrain_ = world_artifacts_->terrain;
        terrain_mesh_ = world_artifacts_->terrain_mesh;
        resolved_buildings_ = world_artifacts_->resolved_buildings;
    }
    const auto grid_layout = world::GridLayout::forMap(config_.map_size_m);
    if (!grid_layout.valid()) {
        generation_error_ = "world request has no valid grid layout";
        plan_.reset();
        world_artifacts_.reset();
        terrain_.reset();
        terrain_mesh_.reset();
        resolved_buildings_.reset();
        return;
    }
    const float camera_map_size = static_cast<float>(config_.map_size_m);
    camera_request_.preset = camera::CameraPreset::Battlefield;
    camera_request_.mode = camera::CameraMode::Orbit;
    camera_request_.position = {camera_map_size * 0.78F, camera_map_size * 0.92F,
                                camera_map_size * 0.82F};
    camera_request_.target = {0.0F, 0.0F, 0.0F};
    camera_request_.up = {0.0F, 1.0F, 0.0F};
    camera_request_.lens = {0.9F, 0.2F, std::max(1000.0F, camera_map_size * 4.0F)};
    physics_.setGroundHeightQuery({&*terrain_, &sample_ground});
    physics_.bindWorldRevision(world::artifactRevision(*plan_));
    auto render_mesh = std::make_shared<render::RenderMesh>();
    render_mesh->mesh_id = foundation::stable_id("mesh.world.terrain");
    render_mesh->revision = world::artifactRevision(*plan_);
    render_mesh->vertices.reserve(terrain_mesh_->vertices.size());
    render_mesh->indices = terrain_mesh_->indices;
    for (const terrain::TerrainMeshVertex& vertex : terrain_mesh_->vertices) {
        render_mesh->vertices.push_back({vertex.position, vertex.normal, vertex.uv,
                                         {0.19F, 0.42F, 0.22F, 1.0F}});
    }
    render_terrain_mesh_ = std::move(render_mesh);
    const auto world_mesh_result = world_render::WorldMeshCompiler::compile(
        *plan_, *terrain_, *resolved_buildings_, world::artifactRevision(*plan_));
    if (!world_mesh_result) {
        generation_error_ = std::string(world_mesh_result.error().message);
        plan_.reset();
        terrain_.reset();
        terrain_mesh_.reset();
        render_terrain_mesh_.reset();
        return;
    }
    world_mesh_artifact_ = std::move(world_mesh_result.value());
    render_world_mesh_ = world_mesh_artifact_->mesh;
    entities_.clear();
    const std::uint32_t nav_cells = grid_layout.cell_count;
    navigation_ = std::make_unique<navigation::GridNavigationWorld>(
        navigation::NavGridSpec{nav_cells, nav_cells, grid_layout.spacing_m,
                                grid_layout.origin});
    if (navigation_) {
        navigation_->bindWorldRevision(world::artifactRevision(*plan_));
    }
    if (navigation_ && navigation_->valid()) {
        for (std::uint32_t z = 0; z < nav_cells; ++z) {
            for (std::uint32_t x = 0; x < nav_cells; ++x) {
                const foundation::Vec3 cell = {
                    grid_layout.origin.x +
                        (static_cast<float>(x) + 0.5F) * grid_layout.spacing_m,
                    0.0F,
                    grid_layout.origin.z +
                        (static_cast<float>(z) + 0.5F) * grid_layout.spacing_m};
                if (plan_->hydrology.isWater(cell.x, cell.z)) {
                    (void)navigation_->setBlocked(x, z, true);
                }
            }
        }
        for (const buildings::BuildingGenerationResult& building : *resolved_buildings_) {
            const world::BuildingSiteResolution& site = building.resolution;
            const float radius_x = site.resolved_footprint.x * 0.55F;
            const float radius_z = site.resolved_footprint.z * 0.55F;
            for (std::uint32_t z = 0; z < nav_cells; ++z) {
                for (std::uint32_t x = 0; x < nav_cells; ++x) {
                    const foundation::Vec3 cell = {
                        grid_layout.origin.x +
                            (static_cast<float>(x) + 0.5F) * grid_layout.spacing_m,
                        0.0F,
                        grid_layout.origin.z +
                            (static_cast<float>(z) + 0.5F) * grid_layout.spacing_m};
                    if (std::abs(cell.x - site.world_position.x) <= radius_x &&
                        std::abs(cell.z - site.world_position.z) <= radius_z) {
                        (void)navigation_->setBlocked(x, z, true);
                    }
                }
            }
        }
    }
#if GENOMES_HAS_INFANTRY
    infantry_ = std::make_unique<infantry::InfantrySimulation>(
        entities_, navigation_.get(), &physics_, jobs_, true);
    constexpr std::uint32_t units_per_team = 25;
    const float map_size = static_cast<float>(config_.map_size_m);
    const float half_map = map_size * 0.5F;
    const auto spawn_infantry = [&](infantry::Team team, foundation::Vec3 position,
                                    const infantry::InfantryGenome& genome,
                                    std::uint32_t squad_id) {
        const auto result = infantry_->spawn({team, position, genome,
                                              {{team, squad_id}},
                                              weapons::weapon_id("infantry_default")});
        if (!result) {
            generation_error_ = std::string(result.error().message);
        }
    };
    for (std::uint32_t index = 0; index < units_per_team; ++index) {
        const float lateral = 70.0F + static_cast<float>(index % 5) * 12.0F;
        const float depth = 150.0F + static_cast<float>(index / 5) * 22.0F;
        const infantry::InfantryGenome genome{
            1.65F + static_cast<float>(index % 4) * 0.045F,
            2.7F + static_cast<float>(index % 3) * 0.15F,
            90.0F,
            38.0F,
            100.0F,
            static_cast<float>(index % 4)};
        const foundation::Vec3 blue_position{-half_map + lateral, 0.0F, -half_map + depth};
        const foundation::Vec3 red_position{half_map - lateral, 0.0F, half_map - depth};
        const float blue_ground = terrain_->sampleBilinear(blue_position.x, blue_position.z);
        const float red_ground = terrain_->sampleBilinear(red_position.x, red_position.z);
        spawn_infantry(infantry::Team::Blue,
                       {blue_position.x, blue_ground + 0.55F, blue_position.z}, genome,
                       index / 5U);
        spawn_infantry(infantry::Team::Red,
                       {red_position.x, red_ground + 0.55F, red_position.z}, genome,
                       100U + index / 5U);
    }
    initialize_infantry_animation();
#endif
    terrain_min_height_ = std::numeric_limits<float>::max();
    terrain_max_height_ = std::numeric_limits<float>::lowest();
    for (std::uint32_t z = 0; z < terrain_->height(); ++z) {
        for (std::uint32_t x = 0; x < terrain_->width(); ++x) {
            const float height = terrain_->at(x, z);
            terrain_min_height_ = std::min(terrain_min_height_, height);
            terrain_max_height_ = std::max(terrain_max_height_, height);
        }
    }
}

void BattlefieldScene::build_presentation(SceneContext& context) {
    context.publishCameraRequest(camera_request_);
    context.presentation.terrain_mesh = render_terrain_mesh_;
    context.presentation.world_mesh = render_world_mesh_;
    if (!plan_) {
        render_infantry_mesh_.reset();
        return;
    }

    context.presentation.instances.reserve(plan_->features.size());
    for (const world::WorldFeature& feature : plan_->features) {
        context.presentation.instances.push_back({feature.id, mesh_id(feature.kind),
                                                   material_id(feature.kind), feature.position,
                                                   feature.scale, feature.rotation_y});
    }
#if GENOMES_HAS_INFANTRY
    if (infantry_) {
        if (infantry_model_artifact_) {
            if (!infantry_skinned_prototype_) {
                infantry_skinned_prototype_ = infantry_presentation::makePrototype(
                    *infantry_model_artifact_);
            }
            if (infantry_skinned_prototype_) {
                context.presentation.skinned_prototypes.push_back(infantry_skinned_prototype_);
                const auto bind_palette = infantry_presentation::makeBindPalette(
                    infantry_model_artifact_->skeleton);
                const auto bind_local_poses = infantry_presentation::makeLocalPoses(
                    infantry_model_artifact_->skeleton, {});
                if (!context.render_capabilities.gpu_skinning && !render_infantry_mesh_) {
                    render_infantry_mesh_ = std::make_shared<render::RenderMesh>(
                        render::deformSkinnedCPU(*infantry_skinned_prototype_, bind_palette));
                    render_infantry_mesh_->mesh_id = infantry_skinned_prototype_->mesh_id;
                    render_infantry_mesh_->revision = infantry_skinned_prototype_->revision;
                }
                if (!context.render_capabilities.gpu_skinning && render_infantry_mesh_) {
                    context.presentation.instance_prototypes.push_back(render_infantry_mesh_);
                }

                const float model_height =
                    std::max(0.01F, infantry_model_artifact_->phenotype.body.height);
                const foundation::StableId blue_material =
                    foundation::stable_id("material.infantry.blue");
                const foundation::StableId red_material =
                    foundation::stable_id("material.infantry.red");
                context.presentation.instances.reserve(
                    context.presentation.instances.size() + infantry_->renderStates().size());
                context.presentation.skinned_palettes.reserve(
                    context.presentation.skinned_palettes.size() +
                    infantry_->renderStates().size());
                std::unordered_map<foundation::StableId, const infantry::AnimationPose*> poses;
                poses.reserve(animation_poses_.size());
                for (const auto& pose : animation_poses_) poses.emplace(pose.semantic_id, &pose);

                for (const infantry::InfantryRenderState& state : infantry_->renderStates()) {
                    const foundation::StableId object_id =
                        foundation::stable_id("entity.infantry") ^ state.entity.packed();
                    const foundation::StableId pose_id =
                        foundation::stable_id("battlefield.infantry") ^ state.entity.packed();
                    const auto pose_found = poses.find(pose_id);
                    const infantry::AnimationPose* pose =
                        pose_found != poses.end() ? pose_found->second : nullptr;
                    render::SkinnedBonePalette palette{};
                    palette.instance_id = object_id;
                    palette.skeleton_id = infantry_model_artifact_->skeleton.cacheKey();
                    palette.pose_revision = pose != nullptr ? pose->revision : 0U;
                    if (pose != nullptr) {
                        const auto pose_span =
                            std::span<const infantry::RigTransform>(pose->bones);
                        palette.matrices = infantry_presentation::makePalette(
                            infantry_model_artifact_->skeleton, pose_span);
                        palette.local_poses = infantry_presentation::makeLocalPoses(
                            infantry_model_artifact_->skeleton, pose_span);
                        palette.morph_weights[0] = pose->face.eyelids_close;
                        palette.morph_weights[1] = pose->face.eyelids_arc;
                        palette.morph_weights[2] = pose->face.neck_flex;
                        palette.morph_weights[3] = pose->face.hands_relax;
                    } else {
                        palette.matrices = bind_palette;
                        palette.local_poses = bind_local_poses;
                    }
                    context.presentation.skinned_palettes.push_back(std::move(palette));
                    const std::uint32_t instance_flags =
                        render::RenderInstanceFlagDynamic |
                        render::RenderInstanceFlagCastShadow |
                        render::RenderInstanceFlagReceiveShadow |
                        (state.team == infantry::Team::Red
                             ? render::RenderInstanceFlagTeamRed
                             : 0U);
                    context.presentation.instances.push_back(
                        {object_id,
                         infantry_skinned_prototype_->mesh_id,
                         state.team == infantry::Team::Blue ? blue_material : red_material,
                         state.position,
                         {state.height / model_height, state.height / model_height,
                          state.height / model_height},
                         state.heading,
                         simulation_tick_.value,
                         instance_flags,
                         state.team == infantry::Team::Red
                             ? foundation::Color{1.0F, 0.78F, 0.72F, 1.0F}
                             : foundation::Color{0.78F, 0.87F, 1.0F, 1.0F}});
                }
                return;
            }
        }
        // There is deliberately no second, simplified infantry renderer here.
        // A failed compiler result leaves the scene without infantry until the
        // immutable model artifact can be rebuilt.
    }
#endif
}

} // namespace genomes::runtime
