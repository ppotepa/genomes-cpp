#include <genomes/game_scenes/BattlefieldScene.hpp>
#include <genomes/game_scenes/ApplicationCommand.hpp>
#include "BattlefieldSceneDetail.hpp"

#include <genomes/foundation/StableHash.hpp>
#include <genomes/game_scenes/InfantryPresentation.hpp>
#include <genomes/world/GridLayout.hpp>
#include <genomes/world_render/WorldMeshCompiler.hpp>
#if GENOMES_HAS_INFANTRY
#include <genomes/infantry/EquipmentCatalog.hpp>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>

namespace genomes::game_scenes {

using namespace battlefield_detail;

BattlefieldScene::~BattlefieldScene() {
#if GENOMES_HAS_INFANTRY
    // Worker results are immutable/shared and carry no scene pointer.  The
    // scene lifetime is protected by epoch/cancellation, so destruction must
    // not turn into a worker barrier.
    animation_job_.cancel();
#endif
}

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
            mass_battle_session_ == nullptr || infantry_model_artifact_ == nullptr) {
            return {runtime::SceneLoadingPhase::Failed, 0.15,
                    generation_error_.empty() ? "Infantry scene initialization failed"
                                              : generation_error_};
        }
        if (mass_battle_profile_ != MassBattlePresentationProfile::Quality &&
            !mass_battle_pose_atlas_ready_ && !mass_battle_pose_atlas_failed_) {
            return {runtime::SceneLoadingPhase::InProgress, 0.65,
                    "Baking animation atlas"};
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
    scene_epoch_ = context.scene_epoch;
#if GENOMES_HAS_INFANTRY
    pose_exchange_.rejectBeforeSceneEpoch(scene_epoch_);
#endif
    if (mode_ == BattlefieldSceneMode::InfantryMassBattle) {
        config_ = massBattleWorldConfig(config_);
    }
    // The application-owned scheduler remains the single execution owner in
    // deterministic capture as well; capture waits for its groups instead of
    // creating a scene-local serial pool.
    engine_services_ = context.engine_services;
    if (engine_services_ != nullptr && engine_services_->execution != nullptr) {
        mass_battle_presentation_scheduler_.bind(*engine_services_->execution);
    }
    cancellation_ = context.engine_services != nullptr &&
                    context.engine_services->lifetime != nullptr
                        ? context.engine_services->lifetime->cancellation()
                        : jobs::CancelToken{};
    generation_error_.clear();
    simulation_failed_ = false;
    plan_.reset();
    resolved_buildings_.reset();
    scenario_.reset();
#if GENOMES_HAS_INFANTRY
    battlefield_runtime_.reset();
    mass_battle_session_.reset();
    simulation_facade_ = nullptr;
    generation_ = context.engine_services != nullptr
        ? context.engine_services->generation : nullptr;
    if (mode_ == BattlefieldSceneMode::InfantryMassBattle) {
        mass_battle_load_stage_ = MassBattleLoadStage::Starting;
    } else {
        mass_battle_load_stage_ = MassBattleLoadStage::Inactive;
    }
#endif
    terrain_.reset();
    terrain_mesh_.reset();
    world_artifacts_.reset();
    render_terrain_mesh_.reset();
    render_water_mesh_.reset();
    render_world_mesh_.reset();
    world_mesh_artifact_.reset();
#if GENOMES_HAS_INFANTRY
    render_infantry_mesh_.reset();
    mass_battle_pose_meshes_.fill(nullptr);
    mass_battle_presentation_scheduler_.cancel();
    mass_battle_pose_atlas_ready_ = false;
    mass_battle_pose_atlas_failed_ = false;
    mass_battle_visible_units_ = 0U;
    mass_battle_active_pose_slots_ = 0U;
    mass_battle_archetype_counts_.fill(0U);
    mass_battle_lod_counts_.fill(0U);
    mass_battle_evaluated_poses_ = 0U;
    hydrology_water_cells_ = 0U;
    hydrology_flood_cells_ = 0U;
    pose_stall_frames_ = 0U;
    last_pose_tick_ = 0U;
    last_pose_revision_ = 0U;
    infantry_skinned_prototype_.reset();
    infantry_model_artifact_.reset();
    if (mode_ != BattlefieldSceneMode::InfantryMassBattle) {
        infantry::InfantryModelRequest model_request{};
        model_request.seed = config_.seed;
        model_request.loadout_id = infantry::EquipmentCatalog::loadoutId("RIFLEMAN");
        if (active_generation() != nullptr &&
            active_generation()->hasGenerator(proc::generatorId("infantry.model"))) {
            proc::GenerationRequest<infantry::InfantryModelRequest,
                                    infantry::InfantryModelCompileResult> request;
            request.generator = proc::generatorId("infantry.model");
            request.input = std::make_shared<const infantry::InfantryModelRequest>(model_request);
            request.seed_path = proc::SeedPath(model_request.seed);
            request.options.input_hash = infantry::InfantryModelCompiler::canonicalRequestKey(
                model_request);
            request.options.retained_bytes = sizeof(infantry::InfantryModelCompileResult);
            request.options.cancellation = cancellation_;
            tactical_model_ticket_ = active_generation()->request(std::move(request));
        } else {
            generation_error_ = "shared infantry procedural runtime unavailable";
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
    animation_job_ = {};
    pose_stall_frames_ = 0U;
    last_pose_tick_ = 0U;
    last_pose_revision_ = 0U;
    pending_animation_context_.reset();
    pending_animation_error_.reset();
    if (mass_battle_model_ticket_.valid()) {
        mass_battle_model_ticket_.cancel();
        mass_battle_model_ticket_ = {};
    }
    if (tactical_model_ticket_.valid()) {
        tactical_model_ticket_.cancel();
        tactical_model_ticket_ = {};
    }
#endif
    if (mode_ == BattlefieldSceneMode::InfantryMassBattle) {
        const float map_size = static_cast<float>(config_.map_size_m);
        camera_request_.preset = camera::CameraPreset::Battlefield;
        camera_request_.mode = camera::CameraMode::RTS;
        camera_request_.position = {0.0F, 105.0F, 260.0F};
        camera_request_.target = {0.0F, 2.0F, 0.0F};
        camera_request_.up = {0.0F, 1.0F, 0.0F};
        camera_request_.lens = {0.9F, 0.2F, std::max(1000.0F, map_size * 4.0F)};
        camera_request_.rts = rts_controls_;
        camera_request_.lens.projection_offset_x = -0.12F;
        camera_request_.rts.target_min = {-map_size * 0.5F + 16.0F,
                                          -map_size * 0.5F + 16.0F};
        camera_request_.rts.target_max = {map_size * 0.5F - 16.0F,
                                          map_size * 0.5F - 16.0F};
    }
    if (active_generation() != nullptr) {
        scenario_ = std::make_unique<gameplay::WorldScenario>(
            *active_generation(), building_profile_);
        if (context.deterministic_capture) {
            const auto started = scenario_->startNew(config_);
            if (!started) {
                generation_error_ = std::string(started.error().message);
            } else if (const auto* generated = scenario_->activePlan(); generated != nullptr) {
                finalize_plan(*generated);
            }
        } else {
            const auto requested = scenario_->requestNew(config_);
            if (!requested) {
                generation_error_ = std::string(requested.error().message);
            }
        }
    } else {
        generation_error_ = "world generation capability is unavailable";
    }
    context.ui.clear();
}

void BattlefieldScene::on_exit(SceneContext& context) {
#if GENOMES_HAS_INFANTRY
    if (animation_job_.valid()) {
        animation_job_.cancel();
        animation_job_ = {};
    }
    mass_battle_presentation_scheduler_.cancel();
    mass_battle_presentation_scheduler_.cancel();
    mass_battle_presentation_scheduler_.reset();
    mass_battle_presentation_scheduler_.reset();
    pending_animation_context_.reset();
    pending_animation_error_.reset();
#endif
    if (context.engine_services != nullptr) {
        context.engine_services->simulation = nullptr;
    }
    plan_.reset();
    resolved_buildings_.reset();
    terrain_.reset();
    terrain_mesh_.reset();
    world_artifacts_.reset();
    render_terrain_mesh_.reset();
    render_water_mesh_.reset();
    render_world_mesh_.reset();
    world_mesh_artifact_.reset();
    render_infantry_mesh_.reset();
    mass_battle_pose_meshes_.fill(nullptr);
    mass_battle_presentation_scheduler_.cancel();
    mass_battle_pose_atlas_ready_ = false;
    mass_battle_pose_atlas_failed_ = false;
    mass_battle_visible_units_ = 0U;
    mass_battle_active_pose_slots_ = 0U;
    mass_battle_archetype_counts_.fill(0U);
    mass_battle_lod_counts_.fill(0U);
    mass_battle_evaluated_poses_ = 0U;
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
    mass_battle_session_.reset();
    simulation_facade_ = nullptr;
    generation_ = nullptr;
    mass_battle_load_stage_ = MassBattleLoadStage::Inactive;
#endif
    engine_services_ = nullptr;
    cancellation_ = {};
}
#if GENOMES_HAS_INFANTRY
void BattlefieldScene::advance_mass_battle_loading() {
    if (mode_ != BattlefieldSceneMode::InfantryMassBattle || simulation_failed_) return;
    switch (mass_battle_load_stage_) {
    case MassBattleLoadStage::Starting:
        mass_battle_load_stage_ = MassBattleLoadStage::CreateSimulation;
        return;
    case MassBattleLoadStage::CreateSimulation: {
        // A mass-battle runtime is never allowed to initialize against the
        // implicit flat origin. Wait for the single immutable world commit.
        if (world_artifacts_ == nullptr || !world_artifacts_->valid()) {
            return;
        }
        auto mass = gameplay::BattlefieldSession::startMassBattle(
            {.seed = config_.seed, .map_size_m = config_.map_size_m,
             .units_per_team = MassBattleUnitsPerTeam, .fixed_step_seconds = 1.0F / 60.0F},
            *engine_services_->execution,
            active_generation() != nullptr ? *active_generation() : proc::GenerationClient{});
        if (!mass) {
            generation_error_ = std::string(mass.error().message);
            simulation_failed_ = true;
            mass_battle_load_stage_ = MassBattleLoadStage::Failed;
            return;
        }
        mass_battle_session_ = std::move(mass.value());
        mass_battle_session_->setSceneEpoch(scene_epoch_);
        simulation_facade_ = mass_battle_session_.get();
        if (!mass_battle_session_->bindWorldArtifact(world_artifacts_)) {
            generation_error_ = "mass battlefield rejected the resolved world terrain";
            simulation_failed_ = true;
            mass_battle_load_stage_ = MassBattleLoadStage::Failed;
            return;
        }
        // The scene consumes the same immutable simulation facade for both
        // battlefield modes; the concrete runtime remains an implementation
        // detail of the gameplay module.
        mass_battle_load_stage_ = MassBattleLoadStage::CompileModel;
        return;
    }
    case MassBattleLoadStage::CompileModel: {
        infantry::InfantryModelRequest model_request{};
        model_request.seed = config_.seed;
        model_request.loadout_id = infantry::EquipmentCatalog::loadoutId("RIFLEMAN");
        if (!mass_battle_model_ticket_.valid()) {
            if (active_generation() == nullptr) {
                generation_error_ = "infantry procedural runtime unavailable";
                simulation_failed_ = true;
                mass_battle_load_stage_ = MassBattleLoadStage::Failed;
                return;
            }
            proc::GenerationRequest<infantry::InfantryModelRequest,
                                    infantry::InfantryModelCompileResult> request;
            request.generator = proc::generatorId("infantry.model");
            request.input = std::make_shared<const infantry::InfantryModelRequest>(model_request);
            request.seed_path = proc::SeedPath(model_request.seed);
            request.options.input_hash = infantry::InfantryModelCompiler::canonicalRequestKey(
                model_request);
            request.options.retained_bytes = sizeof(infantry::InfantryModelCompileResult);
            request.options.cancellation = cancellation_;
            mass_battle_model_ticket_ = active_generation()->request(std::move(request));
            return;
        }
        if (!mass_battle_model_ticket_.complete()) return;
        foundation::Result<infantry::InfantryModelCompileResult, foundation::Error> model =
            foundation::Result<infantry::InfantryModelCompileResult, foundation::Error>::failure(
                mass_battle_model_ticket_.error());
        if (const auto generated = mass_battle_model_ticket_.artifact(); generated) {
            model = foundation::Result<infantry::InfantryModelCompileResult,
                                       foundation::Error>::success(*generated);
        }
        mass_battle_model_ticket_ = {};
        if (!model) {
            generation_error_ = std::string(model.error().message);
            simulation_failed_ = true;
            mass_battle_load_stage_ = MassBattleLoadStage::Failed;
            return;
        }
        infantry_model_artifact_ = std::move(model.value().artifact);
        // Mass Battle is staged independently from world-plan generation.  A
        // plan may finish before the runtime exists, so the earlier
        // finalize_plan() initialization is intentionally retried here once
        // both the runtime and the model are available.
        initialize_infantry_animation();
        mass_battle_load_stage_ = MassBattleLoadStage::Ready;
        return;
    }
    case MassBattleLoadStage::Inactive:
    case MassBattleLoadStage::Ready:
    case MassBattleLoadStage::Failed:
        return;
    }
}
#endif

void BattlefieldScene::fixed_update(SceneContext& scene_context,
                                    const simulation::TickContext& context) {
    if (simulation_failed_) {
        return;
    }
#if !GENOMES_HAS_INFANTRY
    (void)context;
    return;
#else
    if (world_artifacts_ == nullptr) {
        return;
    }
    if (mode_ != BattlefieldSceneMode::InfantryMassBattle &&
        battlefield_runtime_ == nullptr) {
        if (scene_context.engine_services == nullptr ||
            scene_context.engine_services->execution == nullptr) {
            generation_error_ = "battlefield requires composition-root services";
            simulation_failed_ = true;
            return;
        }
        auto viability = gameplay::BattlefieldRuntime::start(
            {.seed = config_.seed,
             .map_size_m = 25U, .fixed_step_seconds = 1.0F / 60.0F,
             .max_ticks = 240U, .tactical_ai_profile = tactical_ai_profile_},
            *scene_context.engine_services->execution,
            scene_context.deterministic_capture ? gameplay::BattlefieldExecutionMode::Inline
                                                 : gameplay::BattlefieldExecutionMode::Parallel,
            active_generation() != nullptr ? *active_generation() : proc::GenerationClient{});
        if (!viability) {
            generation_error_ = std::string(viability.error().message);
            simulation_failed_ = true;
            return;
        }
        battlefield_runtime_ = std::move(viability.value());
        battlefield_runtime_->setSceneEpoch(scene_epoch_);
        simulation_facade_ = battlefield_runtime_.get();
        scene_context.engine_services->simulation = simulation_facade_;
    }
    api::SimulationFacade* simulation = simulation_facade_;
    if (scene_context.engine_services != nullptr &&
        scene_context.engine_services->simulation != nullptr) {
        simulation = scene_context.engine_services->simulation;
    }
    if (simulation == nullptr) {
        // The staged mass-battle session has not reached CreateSimulation yet.
        return;
    }
    if (!simulation->advance(context)) {
        generation_error_ = mode_ == BattlefieldSceneMode::InfantryMassBattle
            ? "Mass Battle simulation update failed"
            : "Battlefield runtime failed during API advance";
        simulation_failed_ = true;
        return;
    }
    const bool regenerate =
        (battlefield_runtime_ != nullptr && battlefield_runtime_->consumeWorldRegenerateRequest()) ||
        (mass_battle_session_ != nullptr && mass_battle_session_->consumeWorldRegenerateRequest());
    const bool restart =
        (battlefield_runtime_ != nullptr && battlefield_runtime_->consumeRestartRequest()) ||
        (mass_battle_session_ != nullptr && mass_battle_session_->consumeRestartRequest());
    if (regenerate) {
        auto next_config = config_;
        next_config.seed = foundation::stableHashCombine(
            config_.seed, foundation::stableHashString("mass-battle.generate-new"));
        if (next_config.seed == 0U) next_config.seed = 1U;
        application::enqueueApplicationCommand(
            scene_context, application::ApplicationCommandKind::OpenMassBattle,
            std::move(next_config));
        return;
    }
    if (restart) {
        application::enqueueApplicationCommand(
            scene_context, application::ApplicationCommandKind::OpenMassBattle, config_);
        return;
    }
    evaluate_infantry_animation(context);
#endif
}

void BattlefieldScene::finalize_plan(world::WorldPlan plan) {
    plan_ = std::move(plan);
    resolved_buildings_.reset();
    terrain_.reset();
    terrain_mesh_.reset();
    world_artifacts_.reset();
    render_terrain_mesh_.reset();
    render_water_mesh_.reset();
    render_world_mesh_.reset();
    world_mesh_artifact_.reset();
    mass_battle_visible_units_ = 0U;
    mass_battle_active_pose_slots_ = 0U;
    mass_battle_archetype_counts_.fill(0U);
    mass_battle_lod_counts_.fill(0U);
    mass_battle_evaluated_poses_ = 0U;
    camera_request_ = {};
    const auto shared_artifact_handle =
        scenario_ != nullptr ? scenario_->activeArtifactHandle()
                              : std::shared_ptr<const gameplay::WorldScenarioArtifact>{};
    const gameplay::WorldScenarioArtifact* shared_artifact = shared_artifact_handle.get();
    if (shared_artifact != nullptr && shared_artifact->plan.content_hash == plan_->content_hash &&
        shared_artifact->revision == world::artifactRevision(*plan_) &&
        shared_artifact->terrain != nullptr && shared_artifact->terrain_mesh != nullptr &&
        shared_artifact->water_mesh != nullptr &&
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
        generation_error_ =
            "battlefield presentation requires an atomically published world artifact";
        plan_.reset();
        return;
    }
 #if GENOMES_HAS_INFANTRY
    if (battlefield_runtime_ != nullptr &&
        !battlefield_runtime_->bindWorldArtifact(world_artifacts_)) {
        generation_error_ = "battlefield consumers rejected the resolved world revision";
        plan_.reset();
        world_artifacts_.reset();
        terrain_.reset();
        terrain_mesh_.reset();
        resolved_buildings_.reset();
        return;
    }
    if (mass_battle_session_ != nullptr &&
        !mass_battle_session_->bindWorldArtifact(world_artifacts_)) {
        generation_error_ = "mass battlefield rejected the resolved world terrain";
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
    camera_request_.mode = mode_ == BattlefieldSceneMode::InfantryMassBattle
        ? camera::CameraMode::RTS : camera::CameraMode::Orbit;
    camera_request_.position = mode_ == BattlefieldSceneMode::InfantryMassBattle
        ? foundation::Vec3{0.0F, 105.0F, 260.0F}
        : foundation::Vec3{camera_map_size * 0.78F, camera_map_size * 0.92F,
                           camera_map_size * 0.82F};
    camera_request_.target = mode_ == BattlefieldSceneMode::InfantryMassBattle
        ? foundation::Vec3{0.0F, 2.0F, 0.0F}
        : foundation::Vec3{0.0F, 0.0F, 0.0F};
    camera_request_.up = {0.0F, 1.0F, 0.0F};
    camera_request_.lens = {0.9F, 0.2F, std::max(1000.0F, camera_map_size * 4.0F)};
    if (mode_ == BattlefieldSceneMode::InfantryMassBattle) {
        camera_request_.rts = rts_controls_;
        camera_request_.lens.projection_offset_x = -0.12F;
        camera_request_.rts.target_min = {-camera_map_size * 0.5F + 16.0F,
                                          -camera_map_size * 0.5F + 16.0F};
        camera_request_.rts.target_max = {camera_map_size * 0.5F - 16.0F,
                                          camera_map_size * 0.5F - 16.0F};
    }
    terrain_min_height_ = std::numeric_limits<float>::max();
    terrain_max_height_ = std::numeric_limits<float>::lowest();
    for (std::uint32_t z = 0; z < terrain_->height(); ++z) {
        for (std::uint32_t x = 0; x < terrain_->width(); ++x) {
            const float height = terrain_->at(x, z);
            terrain_min_height_ = std::min(terrain_min_height_, height);
            terrain_max_height_ = std::max(terrain_max_height_, height);
        }
    }
    auto render_mesh = std::make_shared<render::RenderMesh>();
    render_mesh->mesh_id = foundation::stable_id("mesh.world.terrain");
    render_mesh->revision = world::artifactRevision(*plan_);
    render_mesh->vertices.reserve(terrain_mesh_->vertices.size());
    render_mesh->indices = terrain_mesh_->indices;
    for (const terrain::TerrainMeshVertex& vertex : terrain_mesh_->vertices) {
        render_mesh->vertices.push_back({vertex.position, vertex.normal, vertex.uv,
                                         terrainSurfaceColor(vertex, *plan_,
                                                             terrain_min_height_,
                                                             terrain_max_height_)});
    }
    render_terrain_mesh_ = std::move(render_mesh);
    const auto world_mesh_result = world_render::WorldMeshCompiler::compile(
        *plan_, *terrain_, *resolved_buildings_, world::artifactRevision(*plan_),
        world_artifacts_ != nullptr ? world_artifacts_->water_mesh.get() : nullptr);
    if (!world_mesh_result) {
        generation_error_ = std::string(world_mesh_result.error().message);
        plan_.reset();
        terrain_.reset();
        terrain_mesh_.reset();
        render_terrain_mesh_.reset();
        render_water_mesh_.reset();
        return;
    }
    if (world_mesh_result.value().source_revision != world::artifactRevision(*plan_) ||
        world_mesh_result.value().mesh == nullptr ||
        world_mesh_result.value().water_mesh == nullptr ||
        (plan_->hydrology.enabled && !plan_->hydrology.rivers.empty() &&
         (world_mesh_result.value().water_mesh->vertices.empty() ||
          world_mesh_result.value().water_mesh->indices.empty()))) {
        generation_error_ = "world presentation artifacts have inconsistent revision";
        plan_.reset();
        terrain_.reset();
        terrain_mesh_.reset();
        render_terrain_mesh_.reset();
        render_water_mesh_.reset();
        return;
    }
    world_mesh_artifact_ = std::move(world_mesh_result.value());
    render_world_mesh_ = world_mesh_artifact_->mesh;
    render_water_mesh_ = world_mesh_artifact_->water_mesh;
#if GENOMES_HAS_INFANTRY
    // BattlefieldRuntime owns the sole authoritative infantry/ECS instance;
    // presentation consumes its immutable published presentation snapshot.
    initialize_infantry_animation();
#endif
    hydrology_water_cells_ = static_cast<std::size_t>(std::count_if(
        plan_->hydrology.water_mask.begin(), plan_->hydrology.water_mask.end(),
        [](std::uint8_t value) { return value != 0U; }));
    hydrology_flood_cells_ = static_cast<std::size_t>(std::count_if(
        plan_->hydrology.flood_mask.begin(), plan_->hydrology.flood_mask.end(),
        [](std::uint8_t value) { return value != 0U; }));
}

} // namespace genomes::game_scenes
