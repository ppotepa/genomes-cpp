#include <genomes/game_scenes/BattlefieldScene.hpp>
#include <genomes/game_scenes/ApplicationCommand.hpp>

#include <genomes/foundation/StableHash.hpp>
#include <genomes/game_scenes/InfantryPresentation.hpp>
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

namespace genomes::game_scenes {

namespace {

#if GENOMES_HAS_INFANTRY
bool sample_battlefield_ground(void* context, foundation::Vec3 position,
                               infantry::GroundSample& output) noexcept {
    const auto* field = static_cast<const terrain::HeightField*>(context);
    if (field == nullptr) return false;
    output.height = field->sampleBilinear(position.x, position.z);
    output.normal = field->normal(position.x, position.z);
    return std::isfinite(output.height) && std::isfinite(output.normal.x) &&
           std::isfinite(output.normal.y) && std::isfinite(output.normal.z);
}
#endif

[[nodiscard]] std::string feature_summary(const world::WorldPlan& plan) {
    return "Features: " + std::to_string(plan.features.size()) + "  Roads: " +
           std::to_string(plan.count(world::WorldFeatureKind::Road)) + "  Buildings: " +
           std::to_string(plan.count(world::WorldFeatureKind::Building)) + "  Vegetation: " +
           std::to_string(plan.count(world::WorldFeatureKind::Vegetation)) + "  Parcels: " +
           std::to_string(plan.city.parcels.size()) + "  Rivers: " +
           std::to_string(plan.hydrology.rivers.size());
}

[[nodiscard]] application::WorldGenerationConfig massBattleWorldConfig(
    application::WorldGenerationConfig config) noexcept {
    if (config.seed == 0U) config.seed = 0x1F4A77U;
    config.map_size_m = 2000U;
    config.vegetation = std::min(config.vegetation, 0.18F);
    config.buildings = std::min(config.buildings, 0.08F);
    config.fenced_parcels = std::min(config.fenced_parcels, 0.04F);
    config.river_probability = std::min(config.river_probability, 0.15F);
    return config;
}

} // namespace

foundation::SceneId BattlefieldScene::id() const noexcept {
    return mode_ == BattlefieldSceneMode::InfantryMassBattle
        ? foundation::scene_id("scene.infantry-mass-battle")
        : foundation::scene_id("scene.battlefield");
}

runtime::SceneLoadingStatus BattlefieldScene::loading_status() const {
    if (simulation_failed_) {
        return {runtime::SceneLoadingPhase::Failed, 0.0,
                generation_error_.empty() ? "Scene initialization failed" : generation_error_};
    }
    if (!plan_ && !generation_error_.empty()) {
        return {runtime::SceneLoadingPhase::Failed, 0.0, generation_error_};
    }
#if GENOMES_HAS_INFANTRY
    if (mode_ == BattlefieldSceneMode::InfantryMassBattle) {
        if (mass_battle_load_stage_ == MassBattleLoadStage::Starting ||
            mass_battle_load_stage_ == MassBattleLoadStage::CreateSimulation) {
            return {runtime::SceneLoadingPhase::Starting, 0.05,
                    "Starting infantry simulation"};
        }
        if (mass_battle_load_stage_ == MassBattleLoadStage::CompileModel) {
            return {runtime::SceneLoadingPhase::InProgress, 0.25,
                    "Compiling infantry presentation prototype"};
        }
        if (mass_battle_load_stage_ == MassBattleLoadStage::Failed ||
            mass_battle_runtime_ == nullptr || infantry_model_artifact_ == nullptr) {
            return {runtime::SceneLoadingPhase::Failed, 0.15,
                    generation_error_.empty() ? "Infantry scene initialization failed"
                                              : generation_error_};
        }
        if (!plan_) {
            return {runtime::SceneLoadingPhase::InProgress, 0.75,
                    "Infantry ready; generating terrain and world objects"};
        }
    }
#endif
    if (!plan_) {
        return {runtime::SceneLoadingPhase::InProgress, 0.25,
                "Generating terrain and world objects"};
    }
    return {runtime::SceneLoadingPhase::Completed, 1.0, "Scene ready"};
}

void BattlefieldScene::on_enter(SceneContext& context) {
    if (mode_ == BattlefieldSceneMode::InfantryMassBattle) {
        config_ = massBattleWorldConfig(config_);
    }
    jobs_ = context.deterministic_capture ? nullptr : context.jobs;
    generation_error_.clear();
    simulation_failed_ = false;
    plan_.reset();
    resolved_buildings_.reset();
    scenario_.reset();
#if GENOMES_HAS_INFANTRY
    battlefield_runtime_.reset();
    mass_battle_runtime_.reset();
    if (mode_ == BattlefieldSceneMode::InfantryMassBattle) {
        mass_battle_load_stage_ = MassBattleLoadStage::Starting;
    } else {
        mass_battle_load_stage_ = MassBattleLoadStage::Inactive;
        auto viability = gameplay::BattlefieldRuntime::start(
            {.seed = config_.seed,
             .map_size_m = 25U, .fixed_step_seconds = 1.0F / 60.0F,
             .max_ticks = 240U, .tactical_ai_profile = tactical_ai_profile_},
            jobs_);
        if (viability) {
            battlefield_runtime_ = std::move(viability.value());
        } else {
            generation_error_ = std::string(viability.error().message);
            // BattlefieldRuntime is the sole production simulation owner.  A
            // failed start is a terminal scene error, never permission to revive
            // the former scene-local graph/physics pipeline.
            simulation_failed_ = true;
        }
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
    if (mode_ != BattlefieldSceneMode::InfantryMassBattle) {
        infantry::InfantryModelRequest model_request{};
        model_request.seed = config_.seed;
        model_request.loadout_id = infantry::EquipmentCatalog::loadoutId("RIFLEMAN");
        if (auto model = infantry_model_compiler_.compile(model_request); model) {
            infantry_model_artifact_ = std::move(model.value().artifact);
        } else {
            generation_error_ = std::string(model.error().message);
            simulation_failed_ = true;
        }
    }
#endif
    camera_request_ = {};
    terrain_min_height_ = 0.0F;
    terrain_max_height_ = 0.0F;
#if GENOMES_HAS_INFANTRY
    animation_system_.reset();
    animation_agents_.clear();
    animation_poses_.clear();
#endif
    if (mode_ == BattlefieldSceneMode::InfantryMassBattle) {
        const float map_size = static_cast<float>(config_.map_size_m);
        camera_request_.preset = camera::CameraPreset::Battlefield;
        camera_request_.mode = camera::CameraMode::Orbit;
        camera_request_.position = {0.0F, 140.0F, 300.0F};
        camera_request_.target = {0.0F, 10.0F, 0.0F};
        camera_request_.up = {0.0F, 1.0F, 0.0F};
        camera_request_.lens = {0.9F, 0.2F, std::max(1000.0F, map_size * 4.0F)};
    }
    if (jobs_ != nullptr) {
        scenario_ = std::make_unique<gameplay::WorldScenario>(*jobs_, building_profile_);
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
    animation_system_.reset();
    animation_agents_.clear();
    animation_poses_.clear();
#endif
    simulation_failed_ = false;
    region_streamer_.reset();
    scenario_.reset();
#if GENOMES_HAS_INFANTRY
    battlefield_runtime_.reset();
    mass_battle_runtime_.reset();
    mass_battle_load_stage_ = MassBattleLoadStage::Inactive;
#endif
    jobs_ = nullptr;
}

#if GENOMES_HAS_INFANTRY
void BattlefieldScene::advance_mass_battle_loading() {
    if (mode_ != BattlefieldSceneMode::InfantryMassBattle || simulation_failed_) return;
    switch (mass_battle_load_stage_) {
    case MassBattleLoadStage::Starting:
        mass_battle_load_stage_ = MassBattleLoadStage::CreateSimulation;
        return;
    case MassBattleLoadStage::CreateSimulation: {
        auto mass = gameplay::InfantryMassBattleRuntime::start(
            {.seed = config_.seed, .map_size_m = config_.map_size_m,
             .units_per_team = 1000U, .fixed_step_seconds = 1.0F / 60.0F}, jobs_);
        if (!mass) {
            generation_error_ = std::string(mass.error().message);
            simulation_failed_ = true;
            mass_battle_load_stage_ = MassBattleLoadStage::Failed;
            return;
        }
        mass_battle_runtime_ = std::move(mass.value());
        mass_battle_load_stage_ = MassBattleLoadStage::CompileModel;
        return;
    }
    case MassBattleLoadStage::CompileModel: {
        infantry::InfantryModelRequest model_request{};
        model_request.seed = config_.seed;
        model_request.loadout_id = infantry::EquipmentCatalog::loadoutId("RIFLEMAN");
        auto model = infantry_model_compiler_.compile(model_request);
        if (!model) {
            generation_error_ = std::string(model.error().message);
            simulation_failed_ = true;
            mass_battle_load_stage_ = MassBattleLoadStage::Failed;
            return;
        }
        infantry_model_artifact_ = std::move(model.value().artifact);
        mass_battle_load_stage_ = MassBattleLoadStage::Ready;
        return;
    }
    case MassBattleLoadStage::Inactive:
    case MassBattleLoadStage::Ready:
    case MassBattleLoadStage::Failed:
        return;
    }
}

void BattlefieldScene::initialize_infantry_animation() {
    animation_system_.reset();
    animation_agents_.clear();
    animation_poses_.clear();
    if ((!battlefield_runtime_ && !mass_battle_runtime_) || !infantry_model_artifact_) {
        return;
    }
    auto animation = infantry::AnimationSystem::create(64U);
    if (!animation) {
        generation_error_ = std::string(animation.error().message);
        return;
    }
    animation_system_ = std::move(animation.value());
    const std::size_t expected_count = battlefield_runtime_ != nullptr
        ? battlefield_runtime_->renderStates().size()
        : mass_battle_runtime_->renderStates().size();
    animation_agents_.reserve(expected_count);
    const auto add_agent = [this](simulation::EntityId entity,
                                  std::uint8_t animation_variant,
                                  float animation_phase) {
        auto locomotion = infantry::LocomotionController::create(
            infantry_model_artifact_->phenotype.body);
        if (!locomotion) {
            generation_error_ = std::string(locomotion.error().message);
            return;
        }
        InfantryAnimationAgent agent{};
        agent.entity = entity;
        agent.locomotion = std::move(locomotion.value());
        agent.locomotion_state = agent.locomotion->initialState();
        const auto preset = static_cast<infantry::BipedPreset>(
            animation_variant % 5U);
        if (!agent.locomotion->setPreset(*agent.locomotion_state, preset, true)) {
            generation_error_ = "mass infantry animation preset rejected";
            return;
        }
        agent.locomotion_state->phase = std::clamp(
            static_cast<double>(animation_phase), 0.0, 0.99);
        agent.lod.setTier(infantry::AnimationLOD::Near);
        animation_agents_.push_back(std::move(agent));
    };
    if (mass_battle_runtime_ != nullptr) {
        for (const auto& state : mass_battle_runtime_->renderStates()) {
            add_agent(state.entity, state.animation_variant, state.animation_phase);
        }
    } else {
        for (const infantry::InfantryRenderState& state : battlefield_runtime_->renderStates()) {
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
}

void BattlefieldScene::evaluate_infantry_animation(const simulation::TickContext& context) {
    if (!animation_system_ || (!battlefield_runtime_ && !mass_battle_runtime_) ||
        !infantry_model_artifact_ ||
        animation_agents_.empty()) {
        return;
    }

    const float map_size = std::max(1.0F, static_cast<float>(config_.map_size_m));
    const float near_distance = std::max(60.0F, map_size * 0.16F);
    const float mid_distance = std::max(near_distance * 2.0F, map_size * 0.42F);
    const float near2 = near_distance * near_distance;
    const float mid2 = mid_distance * mid_distance;

    std::vector<infantry::AnimationEntity> entities;
    entities.reserve(animation_agents_.size());
    const auto process = [&](InfantryAnimationAgent& agent,
                             foundation::Vec3 position,
                             float heading,
                             infantry::AgentState state,
                             std::uint8_t animation_variant,
                             float animation_phase,
                             bool mass_battle) {
        if (!agent.locomotion || !agent.locomotion_state ||
            (!mass_battle && !agent.face)) {
            return;
        }
        const float dx = position.x - camera_request_.position.x;
        const float dy = position.y - camera_request_.position.y;
        const float dz = position.z - camera_request_.position.z;
        const float distance2 = dx * dx + dy * dy + dz * dz;
        const infantry::AnimationLOD desired_lod =
            distance2 <= near2 ? infantry::AnimationLOD::Near :
            distance2 <= mid2 ? infantry::AnimationLOD::Mid :
                                infantry::AnimationLOD::Far;
        if (agent.lod.tier() != desired_lod) agent.lod.setTier(desired_lod);

        float distance = 0.0F;
        float speed = 0.0F;
        float move_angle = 0.0F;
        float turn_rate = 0.0F;
        if (agent.has_previous_motion) {
            const float dx = position.x - agent.previous_position.x;
            const float dz = position.z - agent.previous_position.z;
            distance = std::sqrt(dx * dx + dz * dz);
            speed = distance / std::max(1.0e-5F,
                                        static_cast<float>(context.fixed_dt_seconds));
            if (distance > 1.0e-5F) {
                const float travel_heading = std::atan2(dx, dz);
                move_angle = std::atan2(std::sin(travel_heading - heading),
                                        std::cos(travel_heading - heading));
            }
            const float heading_delta = std::atan2(
                std::sin(heading - agent.previous_heading),
                std::cos(heading - agent.previous_heading));
            turn_rate = heading_delta / std::max(1.0e-5F,
                                                static_cast<float>(context.fixed_dt_seconds));
        }
        if (mass_battle) {
            switch (animation_variant % 5U) {
            case 0U:
                (void)agent.locomotion->setRequested(
                    *agent.locomotion_state, {{0.0F}, {0.0F}, std::nullopt});
                break;
            case 3U:
                (void)agent.locomotion->setRequested(
                    *agent.locomotion_state, {{0.60F}, {0.0F}, std::nullopt});
                break;
            case 4U:
                (void)agent.locomotion->setRequested(
                    *agent.locomotion_state, {{0.60F}, {speed}, std::nullopt});
                break;
            default:
                (void)agent.locomotion->setRequested(
                    *agent.locomotion_state, {{0.0F}, {speed}, std::nullopt});
                break;
            }
        } else {
            switch (state) {
            case infantry::AgentState::Advance:
                if (!agent.has_previous_motion) speed = agent.locomotion->body().run_speed;
                (void)agent.locomotion->setRequested(
                    *agent.locomotion_state, {{0.0F}, {speed}, std::nullopt});
                break;
            case infantry::AgentState::Engage:
            case infantry::AgentState::Dead:
                (void)agent.locomotion->setState(*agent.locomotion_state,
                                                  infantry::AnimationState::CROUCH);
                break;
            case infantry::AgentState::Idle:
                (void)agent.locomotion->setRequested(
                    *agent.locomotion_state, {{0.0F}, {0.0F}, std::nullopt});
                break;
            }
        }
        infantry::LocomotionMotionContext motion{};
        motion.speed_mps = speed;
        motion.move_angle = move_angle;
        motion.turn_rate = turn_rate;
        motion.turning = std::abs(turn_rate) > 0.32F;
        motion.distance_m = distance;
        (void)agent.locomotion->step(*agent.locomotion_state, motion,
                                      context.fixed_dt_seconds);
        if (mass_battle) {
            agent.locomotion_state->phase = std::clamp(
                static_cast<double>(animation_phase), 0.0, 0.999999);
        }
        agent.previous_position = position;
        agent.previous_heading = heading;
        agent.has_previous_motion = true;

        if (!mass_battle && battlefield_runtime_ != nullptr) {
            if (const auto* weapon_tasks = battlefield_runtime_->weaponPoseTasks(agent.entity);
                weapon_tasks != nullptr && weapon_tasks->valid()) {
                agent.weapon_overlay = infantry_presentation::copyWeaponPoseTasks(
                    *weapon_tasks, position);
            } else {
                agent.weapon_overlay.reset();
            }
        } else {
            agent.weapon_overlay.reset();
        }

        infantry::AnimationEntity entity{};
        entity.semantic_id = foundation::stable_id("battlefield.infantry") ^ agent.entity.packed();
        entity.skeleton = &infantry_model_artifact_->skeleton;
        entity.gear = &infantry_model_artifact_->gear;
        entity.locomotion = &*agent.locomotion;
        entity.locomotion_state = &*agent.locomotion_state;
        entity.transition_runtime = &agent.transition_runtime;
        entity.face = agent.face ? &*agent.face : nullptr;
        entity.root_position = position;
        entity.look_target = foundation::Vec3{position.x + std::sin(heading) * 6.0F,
                                              position.y +
                                                  infantry_model_artifact_->phenotype.body.height * 0.62F,
                                              position.z + std::cos(heading) * 6.0F};
        entity.lod = agent.lod;
        entity.surface = &infantry_model_artifact_->appearance.body;
        entity.weapon_overlay = agent.weapon_overlay.has_value() ? &*agent.weapon_overlay : nullptr;
        if (!mass_battle) {
            // GroundSurfaceQuery retains its legacy void* callback ABI; the
            // adapter never mutates the const height field through that pointer.
            entity.ground_surface = {context.tick.value,
                                     const_cast<terrain::HeightField*>(terrain_.get()),
                                     sample_battlefield_ground};
            entity.ground_runtime = &agent.ground_runtime;
        }
        entities.push_back(std::move(entity));
    };
    if (mass_battle_runtime_ != nullptr) {
        std::unordered_map<std::uint64_t,
                           const gameplay::InfantryMassBattleRenderState*> states;
        states.reserve(mass_battle_runtime_->renderStates().size());
        for (const auto& state : mass_battle_runtime_->renderStates()) {
            states.emplace(state.entity.packed(), &state);
        }
        for (InfantryAnimationAgent& agent : animation_agents_) {
            const auto found = states.find(agent.entity.packed());
            if (found != states.end()) {
                const auto* state = found->second;
                process(agent, state->position, state->heading,
                        state->state, state->animation_variant,
                        state->animation_phase, true);
            }
        }
    } else {
        std::unordered_map<std::uint64_t, const infantry::InfantryRenderState*> states;
        states.reserve(battlefield_runtime_->renderStates().size());
        for (const auto& state : battlefield_runtime_->renderStates()) {
            states.emplace(state.entity.packed(), &state);
        }
        for (InfantryAnimationAgent& agent : animation_agents_) {
            const auto state_found = states.find(agent.entity.packed());
            if (state_found != states.end()) {
                const auto* state = state_found->second;
                process(agent, state->position, state->heading,
                        state->state, 0U, 0.0F, false);
            }
        }
    }
    if (entities.empty()) {
        return;
    }
    const auto result = animation_system_->evaluate(
        std::span<infantry::AnimationEntity>(entities), context.tick.value,
        context.fixed_dt_seconds, jobs_);
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

void BattlefieldScene::fixed_update(SceneContext&, const simulation::TickContext& context) {
    if (simulation_failed_) {
        return;
    }
#if !GENOMES_HAS_INFANTRY
    (void)context;
    return;
#else
    if (mass_battle_runtime_ != nullptr) {
        mass_battle_runtime_->fixedUpdate(context);
        evaluate_infantry_animation(context);
        return;
    }
    if (battlefield_runtime_ != nullptr) {
        if (!battlefield_runtime_->complete()) {
            // BattlefieldRuntime is the sole authoritative owner for this
            // tick; the scene only forwards the clock and extracts results.
            battlefield_runtime_->fixedUpdate(context);
        }
        if (!battlefield_runtime_->snapshot().error.empty()) {
            generation_error_ = "Battlefield runtime failed: " +
                                battlefield_runtime_->snapshot().error;
            simulation_failed_ = true;
        }
        evaluate_infantry_animation(context);
        return;
    }
    // A missing runtime can only mean that start() failed in on_enter().  Do
    // not fall back to a second ECS/physics/combat owner in the product scene.
    return;
#endif
}

void BattlefieldScene::frame_update(SceneContext& context, double) {
#if GENOMES_HAS_INFANTRY
    advance_mass_battle_loading();
#endif
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
    (void)model.set("title", mode_ == BattlefieldSceneMode::InfantryMassBattle
        ? std::string{"INFANTRY MASS BATTLE"} : std::string{"BATTLEFIELD"});
    (void)model.set("description", mode_ == BattlefieldSceneMode::InfantryMassBattle
        ? std::string{"1000 vs 1000 deterministic infantry stress scene"}
        : std::string{"Procedural world plan"});
    (void)model.set("error", std::string{});
    (void)model.set("seed", static_cast<std::int64_t>(config_.seed));
    (void)model.set("map_size", static_cast<std::int64_t>(config_.map_size_m));
    (void)model.set("features", mode_ == BattlefieldSceneMode::InfantryMassBattle
        ? std::string{"2000 infantry units in two deterministic formations"}
        : std::string{"World features pending"});
    (void)model.set("status", std::string{"Preparing world presentation..."});
    (void)model.set("diagnostics", "Renderer uploads " + std::to_string(context.render_telemetry.mesh_uploads) +
                                  " | palette updates " + std::to_string(context.render_telemetry.palette_updates) +
                                  " | draws " + std::to_string(context.render_telemetry.draw_calls));
    if (plan_) {
        (void)model.set("seed", static_cast<std::int64_t>(plan_->seed));
        (void)model.set("map_size", static_cast<std::int64_t>(plan_->map_size_m));
        (void)model.set("features", feature_summary(*plan_));
#if GENOMES_HAS_INFANTRY
        const std::size_t infantry_count = mass_battle_runtime_ != nullptr
                                                ? mass_battle_runtime_->renderStates().size()
                                                : battlefield_runtime_ != nullptr
                                                    ? battlefield_runtime_->renderStates().size()
                                                    : 0U;
#else
        constexpr std::size_t infantry_count = 0U;
#endif
        (void)model.set("units", static_cast<std::int64_t>(infantry_count));
#if GENOMES_HAS_INFANTRY
        if (mass_battle_runtime_ != nullptr) {
            const auto& mass = mass_battle_runtime_->snapshot();
            (void)model.set("viability", "Mass battle: tick " + std::to_string(mass.tick) +
                                " units " + std::to_string(mass.total_units) +
                                " avg speed " + std::to_string(mass.average_speed_mps) +
                                " m/s turns " + std::to_string(mass.direction_changes));
        } else if (battlefield_runtime_ != nullptr) {
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
        if (building_profile_ == nullptr || !building_profile_->frozen()) {
            generation_error_ = "building profile is unavailable";
            plan_.reset();
            return;
        }
        const auto resolved = gameplay::WorldScenario::compileArtifact(
            *plan_, config_, *building_profile_);
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
 #if GENOMES_HAS_INFANTRY
    if (battlefield_runtime_ != nullptr &&
        !battlefield_runtime_->bindWorldArtifactRevision(world_artifacts_->revision)) {
        generation_error_ = "battlefield consumers rejected the resolved world revision";
        plan_.reset();
        world_artifacts_.reset();
        terrain_.reset();
        terrain_mesh_.reset();
        resolved_buildings_.reset();
        return;
    }
 #endif
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
    camera_request_.position = mode_ == BattlefieldSceneMode::InfantryMassBattle
        ? foundation::Vec3{0.0F, 140.0F, 300.0F}
        : foundation::Vec3{camera_map_size * 0.78F, camera_map_size * 0.92F,
                           camera_map_size * 0.82F};
    camera_request_.target = mode_ == BattlefieldSceneMode::InfantryMassBattle
        ? foundation::Vec3{0.0F, 10.0F, 0.0F}
        : foundation::Vec3{0.0F, 0.0F, 0.0F};
    camera_request_.up = {0.0F, 1.0F, 0.0F};
    camera_request_.lens = {0.9F, 0.2F, std::max(1000.0F, camera_map_size * 4.0F)};
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
#if GENOMES_HAS_INFANTRY
    // BattlefieldRuntime owns the sole authoritative infantry/ECS instance;
    // presentation consumes its immutable render-state view.
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
#if GENOMES_HAS_INFANTRY
        if (mass_battle_runtime_ == nullptr)
#endif
        {
            render_infantry_mesh_.reset();
            return;
        }
    }

#if GENOMES_HAS_INFANTRY
    if (battlefield_runtime_ != nullptr || mass_battle_runtime_ != nullptr) {
        if (infantry_model_artifact_) {
            if (!infantry_skinned_prototype_) {
                infantry_skinned_prototype_ = infantry_presentation::makePrototype(
                    *infantry_model_artifact_);
            }
            if (infantry_skinned_prototype_) {
                const auto bind_palette = infantry_presentation::makeBindPalette(
                    infantry_model_artifact_->skeleton);
                const auto bind_local_poses = infantry_presentation::makeLocalPoses(
                    infantry_model_artifact_->skeleton, {});
                const bool mass_battle_instancing = mass_battle_runtime_ != nullptr;
                if (mass_battle_instancing && !render_infantry_mesh_) {
                    render_infantry_mesh_ = std::make_shared<render::RenderMesh>(
                        render::deformSkinnedCPU(*infantry_skinned_prototype_, bind_palette));
                    render_infantry_mesh_->mesh_id =
                        foundation::stable_id("mesh.infantry.mass-battle.rigid");
                    render_infantry_mesh_->revision = foundation::stableHashCombine(
                        infantry_skinned_prototype_->revision,
                        render_infantry_mesh_->mesh_id);
                } else if (!context.render_capabilities.gpu_skinning &&
                           !render_infantry_mesh_) {
                    render_infantry_mesh_ = std::make_shared<render::RenderMesh>(
                        render::deformSkinnedCPU(*infantry_skinned_prototype_, bind_palette));
                    render_infantry_mesh_->mesh_id = infantry_skinned_prototype_->mesh_id;
                    render_infantry_mesh_->revision = infantry_skinned_prototype_->revision;
                }
                if (mass_battle_instancing || !context.render_capabilities.gpu_skinning) {
                    context.presentation.instance_prototypes.push_back(render_infantry_mesh_);
                } else {
                    context.presentation.skinned_prototypes.push_back(
                        infantry_skinned_prototype_);
                }

                const float model_height =
                    std::max(0.01F, infantry_model_artifact_->phenotype.body.height);
                const foundation::StableId blue_material =
                    foundation::stable_id("material.infantry.blue");
                const foundation::StableId red_material =
                    foundation::stable_id("material.infantry.red");
                std::vector<infantry::InfantryRenderState> presentation_states;
                std::uint64_t presentation_tick = 0U;
                if (mass_battle_runtime_ != nullptr) {
                    presentation_tick = mass_battle_runtime_->snapshot().tick;
                    presentation_states.reserve(mass_battle_runtime_->renderStates().size());
                    for (const auto& state : mass_battle_runtime_->renderStates()) {
                        presentation_states.push_back(
                            {state.entity, state.team, state.position, state.heading,
                             state.height, state.state});
                    }
                } else {
                    presentation_tick = battlefield_runtime_->snapshot().tick;
                    presentation_states = battlefield_runtime_->renderStates();
                }
                context.presentation.instances.reserve(
                    context.presentation.instances.size() +
                        presentation_states.size());
                context.presentation.skinned_palettes.reserve(
                    context.presentation.skinned_palettes.size() +
                    presentation_states.size());
                std::unordered_map<foundation::StableId, const infantry::AnimationPose*> poses;
                poses.reserve(animation_poses_.size());
                for (const auto& pose : animation_poses_) poses.emplace(pose.semantic_id, &pose);

                for (const infantry::InfantryRenderState& state : presentation_states) {
                    const foundation::StableId object_id =
                        foundation::stable_id("entity.infantry") ^ state.entity.packed();
                    const foundation::StableId pose_id =
                        foundation::stable_id("battlefield.infantry") ^ state.entity.packed();
                    const auto pose_found = poses.find(pose_id);
                    const infantry::AnimationPose* pose =
                        pose_found != poses.end() ? pose_found->second : nullptr;
                    if (!mass_battle_instancing) {
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
                    }
                    const std::uint32_t instance_flags =
                        render::RenderInstanceFlagDynamic |
                        render::RenderInstanceFlagCastShadow |
                        render::RenderInstanceFlagReceiveShadow |
                        (state.team == infantry::Team::Red
                             ? render::RenderInstanceFlagTeamRed
                             : 0U);
                    foundation::Vec3 presentation_position = state.position;
                    if (mass_battle_instancing && terrain_ != nullptr) {
                        presentation_position.y = terrain_->sampleBilinear(
                            presentation_position.x, presentation_position.z) + 0.02F;
                    }
                    context.presentation.instances.push_back(
                        {object_id,
                         mass_battle_instancing ? render_infantry_mesh_->mesh_id
                                                : infantry_skinned_prototype_->mesh_id,
                         state.team == infantry::Team::Blue ? blue_material : red_material,
                         presentation_position,
                         {state.height / model_height, state.height / model_height,
                          state.height / model_height},
                         state.heading,
                         presentation_tick,
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

} // namespace genomes::game_scenes
