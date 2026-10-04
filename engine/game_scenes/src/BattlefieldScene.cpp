#include <genomes/game_scenes/BattlefieldScene.hpp>
#include <genomes/game_scenes/ApplicationCommand.hpp>
#include <genomes/gameplay/ProductionGenerators.hpp>

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
#include <utility>
#include <unordered_map>

namespace genomes::game_scenes {

BattlefieldScene::~BattlefieldScene() {
#if GENOMES_HAS_INFANTRY
    // SceneDirector destroys the active scene directly at application
    // shutdown. Keep callbacks that capture this scene alive until their
    // scheduler state has completed before releasing the animation system.
    animation_job_.wait();
#endif
}

namespace {

constexpr std::uint64_t MassBattleRtsCameraRevision = 0x52545343414D3031ULL;
#if GENOMES_HAS_INFANTRY
constexpr float MassBattleModelYawOffset = 0.0F;
// Simulation yaw is +Z toward +X; the Diligent world matrix rotates +Z
// toward -X for positive yaw. Convert only at the presentation boundary.
[[nodiscard]] constexpr float infantryPresentationYaw(float heading) noexcept {
    return -heading + MassBattleModelYawOffset;
}
#endif

constexpr std::array<std::size_t, 8U> MassBattlePosePhaseCounts{
     1U, 12U, 12U, 1U, 8U, 1U, 8U, 1U};
static_assert([]() constexpr {
    std::size_t total = 0U;
    for (const std::size_t phase_count : MassBattlePosePhaseCounts) {
        total += phase_count;
    }
    return total;
}() == 44U);

[[nodiscard]] constexpr const char* massBattleProfileName(
    MassBattlePresentationProfile profile) noexcept {
    switch (profile) {
    case MassBattlePresentationProfile::Quality: return "Quality";
    case MassBattlePresentationProfile::Balanced: return "Balanced";
    case MassBattlePresentationProfile::Stress: return "Stress";
    }
    return "Balanced";
}

[[nodiscard]] constexpr const char* massBattleAnimationMode(
    MassBattlePresentationProfile profile) noexcept {
    return profile == MassBattlePresentationProfile::Quality ? "live skinning" : "atlas";
}

[[nodiscard]] constexpr const char* terrainPresetName(world::TerrainPreset preset) noexcept {
    switch (preset) {
    case world::TerrainPreset::Plains: return "Plains";
    case world::TerrainPreset::RollingHills: return "Rolling hills";
    case world::TerrainPreset::Highlands: return "Highlands";
    case world::TerrainPreset::RiverValley: return "River valley";
    }
    return "Rolling hills";
}

[[nodiscard]] constexpr const char* hydrologyModeName(
    hydrology::HydrologyMode mode) noexcept {
    switch (mode) {
    case hydrology::HydrologyMode::Off: return "Off";
    case hydrology::HydrologyMode::SeededOptional: return "Seeded optional";
    case hydrology::HydrologyMode::Forced: return "Forced";
    }
    return "Off";
}

[[nodiscard]] constexpr world::TerrainPreset nextTerrainPreset(
    world::TerrainPreset preset) noexcept {
    switch (preset) {
    case world::TerrainPreset::Plains: return world::TerrainPreset::RollingHills;
    case world::TerrainPreset::RollingHills: return world::TerrainPreset::Highlands;
    case world::TerrainPreset::Highlands: return world::TerrainPreset::RiverValley;
    case world::TerrainPreset::RiverValley: return world::TerrainPreset::Plains;
    }
    return world::TerrainPreset::RollingHills;
}

[[nodiscard]] constexpr hydrology::HydrologyMode nextHydrologyMode(
    hydrology::HydrologyMode mode) noexcept {
    switch (mode) {
    case hydrology::HydrologyMode::Off: return hydrology::HydrologyMode::SeededOptional;
    case hydrology::HydrologyMode::SeededOptional: return hydrology::HydrologyMode::Forced;
    case hydrology::HydrologyMode::Forced: return hydrology::HydrologyMode::Off;
    }
    return hydrology::HydrologyMode::Off;
}

[[nodiscard]] constexpr std::uint8_t nextTerrainSampleSpacing(
    std::uint8_t sample_spacing_m) noexcept {
    // 4 m is the default battlefield detail and 8 m is the lower-cost option.
    // Finer levels remain available in the profile API, but are deliberately
    // not offered as a casual live-scene toggle for a 2 km battle map.
    return sample_spacing_m <= 4U ? 8U : 4U;
}

[[nodiscard]] foundation::Color lerpColor(foundation::Color from, foundation::Color to,
                                          float amount) noexcept {
    const float t = std::clamp(amount, 0.0F, 1.0F);
    return {std::lerp(from.r, to.r, t), std::lerp(from.g, to.g, t),
            std::lerp(from.b, to.b, t), std::lerp(from.a, to.a, t)};
}

[[nodiscard]] float terrainPatchNoise(float x, float z, float cell_size_m,
                                      foundation::StableId seed) noexcept {
    const float grid_x = x / cell_size_m;
    const float grid_z = z / cell_size_m;
    const int x0 = static_cast<int>(std::floor(grid_x));
    const int z0 = static_cast<int>(std::floor(grid_z));
    const auto lattice = [seed](int cell_x, int cell_z) {
        std::uint64_t hash = foundation::stableHashCombine(
            seed, static_cast<std::uint64_t>(static_cast<std::int64_t>(cell_x)));
        hash = foundation::stableHashCombine(
            hash, static_cast<std::uint64_t>(static_cast<std::int64_t>(cell_z)));
        return static_cast<float>(hash & 0xFFFFU) / 65535.0F;
    };
    const auto smooth = [](float value) {
        const float t = std::clamp(value, 0.0F, 1.0F);
        return t * t * (3.0F - 2.0F * t);
    };
    const float tx = smooth(grid_x - static_cast<float>(x0));
    const float tz = smooth(grid_z - static_cast<float>(z0));
    const float lower = std::lerp(lattice(x0, z0), lattice(x0 + 1, z0), tx);
    const float upper = std::lerp(lattice(x0, z0 + 1), lattice(x0 + 1, z0 + 1), tx);
    return std::lerp(lower, upper, tz);
}

[[nodiscard]] foundation::Color terrainSurfaceColor(
    const terrain::TerrainMeshVertex& vertex, const world::WorldPlan& plan,
    float minimum_height, float maximum_height) noexcept {
    const float relief = std::max(maximum_height - minimum_height, 1.0F);
    const float elevation = std::clamp((vertex.position.y - minimum_height) / relief, 0.0F, 1.0F);
    const float slope = std::clamp((1.0F - vertex.normal.y) * 5.0F, 0.0F, 1.0F);
    const float wetness = plan.hydrology.sampleWater(vertex.position.x, vertex.position.z).wetness;
    // This is a compact, deterministic surface mask until authored terrain
    // materials are introduced.  It makes large landforms legible without
    // leaking renderer textures into world generation.
    const float broad_patch = terrainPatchNoise(vertex.position.x, vertex.position.z, 180.0F,
                                                 foundation::stable_id("terrain.surface.broad"));
    const float fine_patch = terrainPatchNoise(vertex.position.x, vertex.position.z, 52.0F,
                                                foundation::stable_id("terrain.surface.fine"));
    const float dryness = std::clamp(elevation * 0.62F + broad_patch * 0.22F +
                                         fine_patch * 0.08F - wetness * 0.58F,
                                     0.0F, 1.0F);
    foundation::Color surface = lerpColor({0.075F, 0.27F, 0.075F, 1.0F},
                                          {0.48F, 0.38F, 0.105F, 1.0F}, dryness);
    surface = lerpColor(surface, {0.34F, 0.31F, 0.24F, 1.0F}, slope * slope * 0.48F);
    surface = lerpColor(surface, {0.055F, 0.20F, 0.085F, 1.0F}, wetness * 0.72F);
    return surface;
}

[[nodiscard]] std::string durationText(foundation::Nanoseconds duration) {
    const double milliseconds = std::chrono::duration<double, std::milli>(duration).count();
    const double rounded = std::round(milliseconds * 100.0) / 100.0;
    return std::to_string(rounded) + " ms";
}

#if GENOMES_HAS_INFANTRY
[[nodiscard]] constexpr infantry::AnimationState massBattleAnimationState(
    std::uint8_t variant) noexcept {
    switch (massBattleAnimationArchetype(variant)) {
    case MassBattleAnimationArchetype::Idle: return infantry::AnimationState::IDLE;
    case MassBattleAnimationArchetype::Walk: return infantry::AnimationState::WALK;
    case MassBattleAnimationArchetype::Run: return infantry::AnimationState::RUN;
    case MassBattleAnimationArchetype::Crouch: return infantry::AnimationState::CROUCH;
    case MassBattleAnimationArchetype::CrouchWalk:
        return infantry::AnimationState::CROUCH_WALK;
    case MassBattleAnimationArchetype::Prone: return infantry::AnimationState::PRONE;
    case MassBattleAnimationArchetype::ProneMove: return infantry::AnimationState::PRONE_MOVE;
    case MassBattleAnimationArchetype::WeaponReady: return infantry::AnimationState::IDLE;
    }
    return infantry::AnimationState::IDLE;
}

[[nodiscard]] bool massBattleUnitShouldRender(
    const render::PresentationSnapshot& presentation,
    MassBattlePresentationProfile profile,
    foundation::Vec3 position,
    float height,
    foundation::Vec3 camera_target) noexcept {
    if (profile == MassBattlePresentationProfile::Stress ||
        !presentation.has_resolved_camera) {
        return true;
    }
    const foundation::Vec3 center = position + foundation::Vec3{0.0F, height * 0.5F, 0.0F};
    const float radius = height * 1.5F;
    bool intersects_view = true;
    for (const auto& plane : presentation.resolved_camera.frustum.planes) {
        if (plane.signedDistance(center) < -radius) {
            intersects_view = false;
            break;
        }
    }
    const float shadow_dx = position.x - camera_target.x;
    const float shadow_dz = position.z - camera_target.z;
    const bool shadow_relevant = shadow_dx * shadow_dx + shadow_dz * shadow_dz <=
                                 180.0F * 180.0F;
    return intersects_view || shadow_relevant;
}

[[nodiscard]] constexpr std::size_t massBattleArchetypeIndex(
    std::uint8_t variant) noexcept {
    return static_cast<std::size_t>(massBattleAnimationArchetype(variant));
}

[[nodiscard]] std::optional<infantry::AnimationState> infantryActionAnimationState(
    foundation::StableId action) noexcept {
    if (action == foundation::stable_id("infantry.action.idle"))
        return infantry::AnimationState::IDLE;
    if (action == foundation::stable_id("infantry.action.walk"))
        return infantry::AnimationState::WALK;
    if (action == foundation::stable_id("infantry.action.run"))
        return infantry::AnimationState::RUN;
    if (action == foundation::stable_id("infantry.action.crouch") ||
        action == foundation::stable_id("infantry.action.weapon-ready"))
        return infantry::AnimationState::CROUCH;
    if (action == foundation::stable_id("infantry.action.crouch-walk"))
        return infantry::AnimationState::CROUCH_WALK;
    if (action == foundation::stable_id("infantry.action.prone"))
        return infantry::AnimationState::PRONE;
    if (action == foundation::stable_id("infantry.action.prone-move"))
        return infantry::AnimationState::PRONE_MOVE;
    return std::nullopt;
}
#endif

[[nodiscard]] constexpr std::size_t massBattlePoseOffset(std::size_t variant) noexcept {
    std::size_t offset=0U;
    for (std::size_t index=0U;index<variant;++index) offset+=MassBattlePosePhaseCounts[index];
    return offset;
}

[[nodiscard]] constexpr std::size_t massBattlePoseBucket(std::size_t variant,float phase,
                                                          std::size_t visible_phases) noexcept {
    const std::size_t native_phases=MassBattlePosePhaseCounts[variant];
    visible_phases=std::clamp(visible_phases,std::size_t{1U},native_phases);
    const float wrapped=std::clamp(phase,0.0F,0.99999994F);
    const std::size_t coarse=std::min(
        static_cast<std::size_t>(wrapped*static_cast<float>(visible_phases)),visible_phases-1U);
    const std::size_t native=std::min(
        ((coarse*2U+1U)*native_phases)/(visible_phases*2U),native_phases-1U);
    return massBattlePoseOffset(variant)+native;
}

static_assert(massBattlePoseOffset(0U)==0U&&massBattlePoseOffset(1U)==1U&&
              massBattlePoseOffset(2U)==13U&&massBattlePoseOffset(3U)==25U&&
              massBattlePoseOffset(4U)==26U);
static_assert(massBattlePoseBucket(1U,0.0F,12U)==1U&&
              massBattlePoseBucket(1U,0.99F,12U)==12U&&
              massBattlePoseBucket(2U,0.25F,6U)>=13U&&
              massBattlePoseBucket(4U,0.75F,2U)<34U&&
              massBattlePoseOffset(5U)==34U&&massBattlePoseOffset(6U)==35U);

#if GENOMES_HAS_INFANTRY
bool sample_battlefield_ground(void* context, foundation::Vec3 position,
                               infantry::GroundSample& output) noexcept {
    const auto* artifacts = static_cast<const gameplay::WorldScenarioArtifact*>(context);
    if (artifacts == nullptr || artifacts->terrain == nullptr) return false;
    const auto landscape = artifacts->sampleLandscape(position.x, position.z);
    if (!landscape.traversable()) return false;
    output.height = landscape.ground_y;
    output.normal = artifacts->terrain->normal(position.x, position.z);
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
    const bool initial_mass_battle_config = config.map_size_m != 2000U;
    config.map_size_m = 2000U;
    config.vegetation = std::min(config.vegetation, 0.18F);
    config.buildings = 0.0F;
    config.fenced_parcels = 0.0F;
    if (initial_mass_battle_config) {
        config.hydrology_mode = hydrology::HydrologyMode::Forced;
        config.river_probability = 1.0F;
    }
    config.terrain.elevation_range_m = std::max(config.terrain.elevation_range_m, 250.0F);
    if (initial_mass_battle_config) config.terrain.sample_spacing_m = 4U;
    config.terrain.landform_scale_m = std::min(config.terrain.landform_scale_m, 420.0F);
    config.terrain.roughness = std::max(config.terrain.roughness, 0.70F);
    config.hydrology.main_river_min = std::max(config.hydrology.main_river_min, std::uint8_t{1U});
    config.hydrology.river_width_min_m =
        std::max(config.hydrology.river_width_min_m, 16.0F);
    config.hydrology.river_width_max_m =
        std::max(config.hydrology.river_width_max_m, 28.0F);
    config.hydrology.valley_width_min_m =
        std::max(config.hydrology.valley_width_min_m, 48.0F);
    config.hydrology.valley_width_max_m =
        std::max(config.hydrology.valley_width_max_m, 110.0F);
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
    jobs_ = context.scheduler;
    cancellation_ = context.engine_services != nullptr
                        ? context.engine_services->cancellation : jobs::CancelToken{};
    generation_error_.clear();
    simulation_failed_ = false;
    plan_.reset();
    resolved_buildings_.reset();
    scenario_.reset();
#if GENOMES_HAS_INFANTRY
    battlefield_runtime_.reset();
    mass_battle_session_.reset();
    simulation_facade_ = nullptr;
    procedural_runtime_.reset();
    procedural_registry_ = {};
    if (context.scheduler != nullptr) {
        auto registry = gameplay::makeProductionGeneratorRegistry();
        if (registry) {
            procedural_registry_ = std::move(registry.value());
            procedural_runtime_ = std::make_unique<proc::ProceduralRuntime>(
                procedural_registry_, *context.scheduler);
        }
    }
    if (mode_ == BattlefieldSceneMode::InfantryMassBattle) {
        mass_battle_load_stage_ = MassBattleLoadStage::Starting;
    } else {
        mass_battle_load_stage_ = MassBattleLoadStage::Inactive;
        auto viability = gameplay::BattlefieldRuntime::start(
            {.seed = config_.seed,
             .map_size_m = 25U, .fixed_step_seconds = 1.0F / 60.0F,
             .max_ticks = 240U, .tactical_ai_profile = tactical_ai_profile_},
            context.scheduler,
            context.deterministic_capture ? gameplay::BattlefieldExecutionMode::Inline
                                           : gameplay::BattlefieldExecutionMode::Parallel,
            procedural_runtime_.get());
        if (viability) {
            battlefield_runtime_ = std::move(viability.value());
            battlefield_runtime_->setSceneEpoch(scene_epoch_);
            simulation_facade_ = battlefield_runtime_.get();
            if (context.engine_services != nullptr) {
                context.engine_services->simulation = simulation_facade_;
            }
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
    mass_battle_pose_meshes_.fill(nullptr);
    mass_battle_atlas_job_ = {};
    pending_mass_battle_pose_meshes_.reset();
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
        foundation::Result<infantry::InfantryModelCompileResult, foundation::Error> model =
            infantry_model_compiler_.compile(model_request);
        if (procedural_runtime_ != nullptr &&
            procedural_runtime_->registry().find(proc::generatorId("infantry.model")) != nullptr) {
            proc::GenerationRequest<infantry::InfantryModelRequest,
                                    infantry::InfantryModelCompileResult>
                generation;
            generation.generator = proc::generatorId("infantry.model");
            generation.input = std::make_shared<const infantry::InfantryModelRequest>(model_request);
            generation.seed_path = proc::SeedPath(model_request.seed);
            generation.options.input_hash = infantry::InfantryModelCompiler::canonicalRequestKey(
                model_request);
            generation.options.retained_bytes = sizeof(infantry::InfantryModelCompileResult);
            const auto generated = procedural_runtime_->generateInline(generation);
            if (generated) {
                model = foundation::Result<infantry::InfantryModelCompileResult,
                                            foundation::Error>::success(*generated.value());
            } else {
                model = foundation::Result<infantry::InfantryModelCompileResult,
                                            foundation::Error>::failure(generated.error());
            }
        }
        if (model) {
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
    animation_job_ = {};
    pose_stall_frames_ = 0U;
    last_pose_tick_ = 0U;
    last_pose_revision_ = 0U;
    pending_animation_context_.reset();
    pending_animation_error_.reset();
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
    if (context.scheduler != nullptr) {
#if GENOMES_HAS_INFANTRY
        auto generation_registry = procedural_registry_;
#else
        proc::GeneratorRegistry generation_registry{};
#endif
        scenario_ = std::make_unique<gameplay::WorldScenario>(
            *context.scheduler, building_profile_, std::shared_ptr<proc::ArtifactCache>{},
            std::move(generation_registry));
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
        const auto generated = world::WorldGenerator::generate(config_);
        if (generated) {
            finalize_plan(std::move(generated.value()));
        } else {
            generation_error_ = std::string(generated.error().message);
        }
    }
    context.ui.clear();
}

void BattlefieldScene::on_exit(SceneContext& context) {
#if GENOMES_HAS_INFANTRY
    if (animation_job_.valid()) {
        animation_job_.wait();
        animation_job_ = {};
    }
    if (mass_battle_atlas_job_.valid()) {
        mass_battle_atlas_job_.wait();
        mass_battle_atlas_job_ = {};
    }
    if (mass_battle_presentation_group_) {
        mass_battle_presentation_group_->wait();
        mass_battle_presentation_group_.reset();
    }
    pending_mass_battle_presentation_.reset();
    ready_mass_battle_presentation_.reset();
    pending_mass_battle_pose_meshes_.reset();
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
    render_world_mesh_.reset();
    world_mesh_artifact_.reset();
    render_infantry_mesh_.reset();
    mass_battle_pose_meshes_.fill(nullptr);
    mass_battle_atlas_job_ = {};
    pending_mass_battle_pose_meshes_.reset();
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
    procedural_runtime_.reset();
    procedural_registry_ = {};
    mass_battle_load_stage_ = MassBattleLoadStage::Inactive;
#endif
    jobs_ = nullptr;
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
        auto mass = gameplay::BattlefieldSession::startMassBattle(
            {.seed = config_.seed, .map_size_m = config_.map_size_m,
             .units_per_team = MassBattleUnitsPerTeam, .fixed_step_seconds = 1.0F / 60.0F}, jobs_);
        if (!mass) {
            generation_error_ = std::string(mass.error().message);
            simulation_failed_ = true;
            mass_battle_load_stage_ = MassBattleLoadStage::Failed;
            return;
        }
        mass_battle_session_ = std::move(mass.value());
        mass_battle_session_->setSceneEpoch(scene_epoch_);
        simulation_facade_ = mass_battle_session_.get();
        if (world_artifacts_ != nullptr &&
            !mass_battle_session_->bindWorldArtifact(world_artifacts_)) {
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
        foundation::Result<infantry::InfantryModelCompileResult, foundation::Error> model =
            foundation::Result<infantry::InfantryModelCompileResult, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState, "infantry procedural runtime unavailable"});
        if (procedural_runtime_) {
            proc::GenerationRequest<infantry::InfantryModelRequest,
                                    infantry::InfantryModelCompileResult> request;
            request.generator = proc::generatorId("infantry.model");
            request.input = std::make_shared<const infantry::InfantryModelRequest>(model_request);
            request.seed_path = proc::SeedPath(model_request.seed);
            request.options.input_hash = infantry::InfantryModelCompiler::canonicalRequestKey(
                model_request);
            request.options.retained_bytes = sizeof(infantry::InfantryModelCompileResult);
            const auto generated = procedural_runtime_->generateInline(request);
            if (generated) {
                model = foundation::Result<infantry::InfantryModelCompileResult,
                                            foundation::Error>::success(*generated.value());
            } else {
                model = foundation::Result<infantry::InfantryModelCompileResult,
                                            foundation::Error>::failure(generated.error());
            }
        } else {
            model = infantry_model_compiler_.compile(model_request);
        }
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

void BattlefieldScene::initialize_infantry_animation() {
    if (animation_system_ && infantry_model_artifact_) {
        const std::size_t expected_count = battlefield_runtime_ != nullptr
            ? battlefield_runtime_->presentationSnapshot().states.size()
            : mass_battle_session_ != nullptr ? mass_battle_session_->presentationSnapshot().states.size() : 0U;
        if (animation_agents_.size() == expected_count) return;
    }
    animation_job_.wait();
    animation_system_.reset();
    animation_agents_.clear();
    animation_poses_.clear();
    pose_stall_frames_ = 0U;
    last_pose_tick_ = 0U;
    last_pose_revision_ = 0U;
    animation_job_ = {};
    pending_animation_context_.reset();
    pending_animation_error_.reset();
    if ((!battlefield_runtime_ && !mass_battle_session_) || !infantry_model_artifact_) {
        return;
    }
    auto animation = infantry::PresentationAnimation::create(64U);
    if (!animation) {
        generation_error_ = std::string(animation.error().message);
        return;
    }
    animation_system_ = std::move(animation.value());
    const std::size_t expected_count = battlefield_runtime_ != nullptr
        ? battlefield_runtime_->presentationSnapshot().states.size()
        : mass_battle_session_->presentationSnapshot().states.size();
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
        if (!agent.locomotion->setState(*agent.locomotion_state,
                                        massBattleAnimationState(animation_variant), true)) {
            generation_error_ = "mass infantry animation preset rejected";
            return;
        }
        agent.locomotion_state->phase = std::clamp(
            static_cast<double>(animation_phase), 0.0, 0.99);
        agent.lod.setTier(infantry::AnimationLOD::Near);
        animation_agents_.push_back(std::move(agent));
    };
    if (mass_battle_session_ != nullptr) {
        for (const auto& state : mass_battle_session_->presentationSnapshot().states) {
            add_agent(state.entity, state.animation_variant, state.animation_phase);
        }
    } else {
        for (const infantry::InfantryRenderState& state : battlefield_runtime_->presentationSnapshot().states) {
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
    if (!animation_system_ || (!battlefield_runtime_ && !mass_battle_session_) ||
        !infantry_model_artifact_ ||
        animation_agents_.empty()) {
        return;
    }
    if (animation_job_.valid()) {
        // Keep only the newest simulation input while one pose evaluation is
        // active. Agent state is deliberately not mutated until that job has
        // completed, so the worker owns a stable input view.
        pending_animation_context_ = context;
        return;
    }

    const float map_size = std::max(1.0F, static_cast<float>(config_.map_size_m));
    const float near_distance = std::max(60.0F, map_size * 0.16F);
    const float mid_distance = std::max(near_distance * 2.0F, map_size * 0.42F);
    const float near2 = near_distance * near_distance;
    const float mid2 = mid_distance * mid_distance;

    std::vector<bool> live_selected(animation_agents_.size(), true);
    if (mass_battle_session_ != nullptr && mass_battle_pose_atlas_ready_ &&
        mass_battle_profile_ != MassBattlePresentationProfile::Quality) {
        struct Candidate final {
            std::size_t agent_index{0U};
            float distance_squared{0.0F};
            bool transition_pending{false};
            bool previously_live{false};
            std::uint64_t entity_key{0U};
        };
        std::unordered_map<std::uint64_t,
                           const gameplay::BattlefieldUnitPresentation*> states;
        states.reserve(mass_battle_session_->presentationSnapshot().states.size());
        for (const auto& state : mass_battle_session_->presentationSnapshot().states) {
            states.emplace(state.entity.packed(), &state);
        }
        const auto was_live = [this](simulation::EntityId entity) {
            return std::find(live_animation_entities_.begin(),
                             live_animation_entities_.end(), entity) !=
                   live_animation_entities_.end();
        };
        std::vector<Candidate> candidates;
        candidates.reserve(animation_agents_.size());
        for (std::size_t index = 0U; index < animation_agents_.size(); ++index) {
            const auto& agent = animation_agents_[index];
            const auto found = states.find(agent.entity.packed());
            if (found == states.end() || !agent.locomotion_state) continue;
            const auto& state = *found->second;
            const float dx = state.position.x - camera_request_.position.x;
            const float dy = state.position.y - camera_request_.position.y;
            const float dz = state.position.z - camera_request_.position.z;
            const bool transition_pending = agent.locomotion_state->transition_active ||
                agent.locomotion_state->requested_state !=
                    massBattleAnimationState(state.animation_variant);
            candidates.push_back({index, dx * dx + dy * dy + dz * dz,
                                  transition_pending, was_live(agent.entity),
                                  agent.entity.packed()});
        }
        std::stable_sort(candidates.begin(), candidates.end(),
            [](const Candidate& lhs, const Candidate& rhs) {
                if (lhs.transition_pending != rhs.transition_pending)
                    return lhs.transition_pending;
                const float lhs_score = lhs.distance_squared *
                    (lhs.previously_live ? 0.90F : 1.0F);
                const float rhs_score = rhs.distance_squared *
                    (rhs.previously_live ? 0.90F : 1.0F);
                if (lhs_score != rhs_score) return lhs_score < rhs_score;
                return lhs.entity_key < rhs.entity_key;
            });
        std::fill(live_selected.begin(), live_selected.end(), false);
        live_animation_entities_.clear();
        const std::size_t selected_count =
            std::min(liveAnimationBudget(), candidates.size());
        live_animation_entities_.reserve(selected_count);
        for (std::size_t index = 0U; index < selected_count; ++index) {
            const Candidate& candidate = candidates[index];
            live_selected[candidate.agent_index] = true;
            live_animation_entities_.push_back(
                animation_agents_[candidate.agent_index].entity);
        }
    } else if (mass_battle_session_ != nullptr) {
        live_animation_entities_.clear();
        live_animation_entities_.reserve(animation_agents_.size());
        for (const auto& agent : animation_agents_)
            live_animation_entities_.push_back(agent.entity);
    }

    std::vector<infantry::AnimationEntity> entities;
    entities.reserve(animation_agents_.size());
    const auto process = [&](InfantryAnimationAgent& agent,
                             foundation::Vec3 position,
                             float heading,
                             infantry::AgentState state,
                             std::uint8_t animation_variant,
                             float animation_phase,
                             foundation::StableId action,
                             bool mass_battle) {
        if (!agent.locomotion || !agent.locomotion_state ||
            (!mass_battle && !agent.face)) {
            return;
        }
        const float dx = position.x - camera_request_.position.x;
        const float dy = position.y - camera_request_.position.y;
        const float dz = position.z - camera_request_.position.z;
        const float distance2 = dx * dx + dy * dy + dz * dz;
        infantry::AnimationLOD desired_lod =
            mass_battle && (mass_battle_profile_ == MassBattlePresentationProfile::Quality ||
                            liveAnimationBudget() >= animation_agents_.size() ||
                            !mass_battle_pose_atlas_ready_)
                ? infantry::AnimationLOD::Near
                : distance2 <= near2 ? infantry::AnimationLOD::Near
                : distance2 <= mid2 ? infantry::AnimationLOD::Mid
                                    : infantry::AnimationLOD::Far;
        if (mass_battle &&
            (agent.locomotion_state->transition_active ||
             agent.locomotion_state->requested_state !=
                 massBattleAnimationState(animation_variant))) {
            desired_lod = infantry::AnimationLOD::Near;
        }
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
            // The simulation changes posture at each local goal epoch.
            const auto requested = massBattleAnimationState(animation_variant);
            if (agent.locomotion_state->requested_state != requested) {
                (void)agent.locomotion->setState(
                    *agent.locomotion_state, requested, false);
            }
        } else {
            const auto requested_action = infantryActionAnimationState(action);
            if (requested_action.has_value()) {
                if (*requested_action == infantry::AnimationState::IDLE) {
                    (void)agent.locomotion->setRequested(
                        *agent.locomotion_state, {{0.0F}, {0.0F}, std::nullopt});
                } else {
                    (void)agent.locomotion->setState(
                        *agent.locomotion_state, *requested_action);
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
        } else if (mass_battle) {
            const bool wants_weapon_ready =
                massBattleAnimationArchetype(animation_variant) ==
                MassBattleAnimationArchetype::WeaponReady;
            const float readiness_delta =
                static_cast<float>(context.fixed_dt_seconds) * 2.5F;
            agent.weapon_readiness = std::clamp(
                agent.weapon_readiness +
                    (wants_weapon_ready ? readiness_delta : -readiness_delta),
                0.0F, 1.0F);
            if (agent.weapon_readiness > 1.0e-3F) {
            const float height = infantry_model_artifact_->phenotype.body.height;
            infantry::AnimationWeaponOverlay overlay{};
            overlay.weapon_id = agent.entity.packed();
            overlay.readiness = agent.weapon_readiness;
            overlay.primary = {infantry::AnimationHandOwner::Primary,
                               {0.18F * height, 0.70F * height, 0.38F * height},
                               agent.weapon_readiness, 0.75F, true};
            overlay.support = {infantry::AnimationHandOwner::Support,
                               {-0.12F * height, 0.69F * height, 0.49F * height},
                               agent.weapon_readiness, 0.7F, true};
            agent.weapon_overlay = overlay;
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
                                     const_cast<gameplay::WorldScenarioArtifact*>(
                                         world_artifacts_.get()),
                                     sample_battlefield_ground};
            entity.ground_runtime = &agent.ground_runtime;
        }
        entities.push_back(std::move(entity));
    };
    if (mass_battle_session_ != nullptr) {
        std::unordered_map<std::uint64_t,
                           const gameplay::BattlefieldUnitPresentation*> states;
        states.reserve(mass_battle_session_->presentationSnapshot().states.size());
        for (const auto& state : mass_battle_session_->presentationSnapshot().states) {
            states.emplace(state.entity.packed(), &state);
        }
        for (std::size_t index = 0U; index < animation_agents_.size(); ++index) {
            if (!live_selected[index]) continue;
            InfantryAnimationAgent& agent = animation_agents_[index];
            const auto found = states.find(agent.entity.packed());
            if (found != states.end()) {
                const auto* state = found->second;
                process(agent, state->position, state->heading,
                        state->state, state->animation_variant,
                        state->animation_phase, 0U, true);
            }
        }
    } else {
        std::unordered_map<std::uint64_t, const infantry::InfantryRenderState*> states;
        states.reserve(battlefield_runtime_->presentationSnapshot().states.size());
        for (const auto& state : battlefield_runtime_->presentationSnapshot().states) {
            states.emplace(state.entity.packed(), &state);
        }
        for (InfantryAnimationAgent& agent : animation_agents_) {
            const auto state_found = states.find(agent.entity.packed());
            if (state_found != states.end()) {
                const auto* state = state_found->second;
                process(agent, state->position, state->heading,
                        state->state, 0U, 0.0F, state->action, false);
            }
        }
    }
    if (entities.empty()) {
        // A zero budget returns the whole crowd to atlas poses. Do not keep
        // rendering the previous live selection indefinitely.
        if (mass_battle_session_ != nullptr) {
            animation_poses_.clear();
            mass_battle_evaluated_poses_ = 0U;
            mass_battle_lod_counts_.fill(0U);
        }
        return;
    }
    if (jobs_ != nullptr) {
        infantry::AnimationWorkSet work{std::move(entities), context.tick.value,
                                        static_cast<float>(context.fixed_dt_seconds)};
        animation_job_ = animation_system_->evaluateAsync(
            std::move(work), *jobs_, cancellation_,
            infantry::PresentationBudget{liveAnimationBudget()});
        return;
    }

    const auto result = animation_system_->evaluate(
        std::span<infantry::AnimationEntity>(entities), context.tick.value,
        context.fixed_dt_seconds, nullptr);
    if (!result) {
        generation_error_ = std::string(result.error().message);
        return;
    }
    publish_infantry_animation_result();
}

void BattlefieldScene::publish_infantry_animation_result() {
    if (!animation_system_) return;
    animation_poses_ = animation_system_->currentSnapshot().poses;
    if (infantry_model_artifact_) {
        if (auto write = pose_exchange_.acquireWrite(); write) {
            auto& snapshot = write.value().snapshot();
            snapshot.metadata.tick = animation_system_->currentSnapshot().simulation_tick;
            snapshot.metadata.scene_epoch = scene_epoch_;
            snapshot.metadata.revision = animation_system_->currentSnapshot().pose_revision;
            snapshot.palettes.reserve(animation_poses_.size());
            for (const auto& pose : animation_poses_) {
                render::SkinnedBonePalette palette{};
                if (mass_battle_session_ != nullptr) {
                    const foundation::StableId packed_entity = pose.semantic_id ^
                        foundation::stable_id("battlefield.infantry");
                    palette.instance_id = foundation::stable_id("entity.infantry") ^ packed_entity;
                } else {
                    palette.instance_id = pose.semantic_id;
                }
                palette.skeleton_id = infantry_model_artifact_->skeleton.cacheKey();
                palette.pose_revision = pose.revision;
                const auto pose_span = std::span<const infantry::RigTransform>(pose.bones);
                palette.matrices = infantry_presentation::makePalette(
                    infantry_model_artifact_->skeleton, pose_span);
                if (mass_battle_session_ == nullptr) {
                    palette.local_poses = infantry_presentation::makeLocalPoses(
                        infantry_model_artifact_->skeleton, pose_span);
                }
                palette.morph_weights = {pose.face.eyelids_close, pose.face.eyelids_arc,
                                         pose.face.neck_flex, pose.face.hands_relax};
                snapshot.palettes.push_back(std::move(palette));
            }
            (void)pose_exchange_.publish(std::move(write.value()));
        }
    }
    if (mass_battle_session_ != nullptr) {
        mass_battle_lod_counts_.fill(0U);
        for (const auto& agent : animation_agents_) {
            const auto index = static_cast<std::size_t>(agent.lod.tier());
            if (index < mass_battle_lod_counts_.size()) ++mass_battle_lod_counts_[index];
        }
        mass_battle_evaluated_poses_ = animation_system_->lastStats().evaluated_count;
    }
}

void BattlefieldScene::request_mass_battle_pose_atlas() {
    if (mass_battle_session_ == nullptr || mass_battle_pose_atlas_ready_ ||
        mass_battle_pose_atlas_failed_ ||
        mass_battle_atlas_job_.valid() || !infantry_skinned_prototype_ ||
        !infantry_model_artifact_ || animation_poses_.empty()) {
        return;
    }
    std::unordered_map<foundation::StableId, const infantry::AnimationPose*> poses;
    poses.reserve(animation_poses_.size());
    for (const auto& pose : animation_poses_) {
        poses.emplace(pose.semantic_id, &pose);
    }
    // AnimationPose contains the complete rig arrays; keeping all atlas
    // samples on the frame stack overflows the relatively small application
    // thread stack. The atlas job already owns heap state, so keep samples
    // there as well.
    auto samples = std::make_shared<
        std::array<std::optional<infantry::AnimationPose>, MassBattlePoseAtlasSize>>();
    std::array<float, MassBattlePoseAtlasSize> sample_errors{};
    sample_errors.fill(std::numeric_limits<float>::max());
    for (const auto& state : mass_battle_session_->presentationSnapshot().states) {
        const std::size_t variant = state.animation_variant % MassBattlePoseVariantCount;
        const std::size_t phases = MassBattlePosePhaseCounts[variant];
        const std::size_t bucket = massBattlePoseBucket(variant, state.animation_phase, phases);
        const float target = (static_cast<float>(bucket - massBattlePoseOffset(variant)) + 0.5F) /
                             static_cast<float>(phases);
        const float direct = std::abs(state.animation_phase - target);
        const float error = std::min(direct, 1.0F - direct);
        const auto found = poses.find(foundation::stable_id("battlefield.infantry") ^
                                      state.entity.packed());
        if (found != poses.end() &&
            found->second->active_state == massBattleAnimationState(state.animation_variant) &&
            found->second->requested_state == found->second->active_state &&
            found->second->transition_stage == infantry::AnimationTransitionStage::None &&
            (variant != 7U || found->second->weapon_readiness > 0.9F) &&
            (variant != 0U || found->second->weapon_readiness < 0.1F) &&
            error < sample_errors[bucket]) {
            (*samples)[bucket] = *found->second;
            sample_errors[bucket] = error;
        }
    }
    for (std::size_t variant = 0U; variant < MassBattlePoseVariantCount; ++variant) {
        const std::size_t begin = massBattlePoseOffset(variant);
        const std::size_t end = begin + MassBattlePosePhaseCounts[variant];
        // Small populations may not cover every animation phase. Reuse the
        // nearest phase of the same stance; never borrow another stance.
        for (std::size_t slot = begin; slot < end; ++slot) {
            if ((*samples)[slot]) continue;
            std::size_t nearest = end;
            for (std::size_t candidate = begin; candidate < end; ++candidate) {
                if ((*samples)[candidate] &&
                    (nearest == end || std::abs(static_cast<int>(candidate) - static_cast<int>(slot)) <
                                           std::abs(static_cast<int>(nearest) - static_cast<int>(slot)))) {
                    nearest = candidate;
                }
            }
            if (nearest == end) return;
            (*samples)[slot] = (*samples)[nearest];
        }
    }

    auto baked = std::make_shared<
        std::array<std::shared_ptr<render::RenderMesh>, MassBattlePoseAtlasSize>>();
    const auto prototype = infantry_skinned_prototype_;
    const auto artifact = infantry_model_artifact_;
    auto bake = [baked, prototype, artifact, samples]() mutable {
        for (std::size_t slot = 0U; slot < MassBattlePoseAtlasSize; ++slot) {
            const auto palette = infantry_presentation::makePalette(
                artifact->skeleton,
                std::span<const infantry::RigTransform>((*samples)[slot]->bones));
            auto mesh = std::make_shared<render::RenderMesh>(
                render::deformSkinnedCPU(*prototype, palette));
            mesh->mesh_id = foundation::stableHashCombine(
                foundation::stable_id("mesh.infantry.mass-battle.pose-atlas"), slot + 1U);
            mesh->revision = foundation::stableHashCombine(prototype->revision, mesh->mesh_id);
            (*baked)[slot] = std::move(mesh);
        }
    };
    pending_mass_battle_pose_meshes_ = baked;
    if (jobs_ != nullptr) {
        jobs::JobOptions options;
        options.lane = jobs::ExecutionLane::Render;
        options.work_class = jobs::WorkClass::Render;
        mass_battle_atlas_job_ = jobs_->submit(
            [bake](jobs::JobContext&) mutable { bake(); }, options);
    } else {
        try {
            bake();
            mass_battle_pose_meshes_ = *baked;
            pending_mass_battle_pose_meshes_.reset();
            mass_battle_pose_atlas_ready_ = true;
        } catch (...) {
            mass_battle_pose_meshes_.fill(nullptr);
            pending_mass_battle_pose_meshes_.reset();
            mass_battle_pose_atlas_failed_ = true;
        }
    }
}

void BattlefieldScene::schedule_mass_battle_presentation() {
    if (mass_battle_session_ == nullptr || !infantry_model_artifact_ ||
        mass_battle_presentation_group_ || ready_mass_battle_presentation_) {
        return;
    }
    const auto states = std::make_shared<
        const std::vector<gameplay::BattlefieldUnitPresentation>>(
        mass_battle_session_->presentationSnapshot().states);
    if (states->empty()) return;

    const auto batch = std::make_shared<MassBattlePresentationBatch>();
    batch->states = *states;
    batch->tick = mass_battle_session_->presentationSnapshot().metadata.tick;
    const auto ranges = std::make_shared<
        std::vector<std::vector<render::RenderInstance>>>();
    constexpr std::size_t range_size = 128U;
    ranges->resize(states->size() / range_size +
                   (states->size() % range_size != 0U ? 1U : 0U));
    const auto terrain = terrain_;
    const float model_height = std::max(0.01F, infantry_model_artifact_->phenotype.body.height);
    const foundation::StableId mesh_id = infantry_skinned_prototype_->mesh_id;
    const foundation::StableId blue_material = foundation::stable_id("material.infantry.blue");
    const foundation::StableId red_material = foundation::stable_id("material.infantry.red");
    jobs::JobGraphBuilder builder;
    jobs::JobOptions presentation_options;
    presentation_options.work_class = jobs::WorkClass::Presentation;
    presentation_options.lane = jobs::ExecutionLane::Worker;
    std::vector<jobs::JobGraphNode> range_nodes;
    range_nodes.reserve(ranges->size());
    for (std::size_t range = 0U; range < ranges->size(); ++range) {
        const std::size_t begin = range * range_size;
        const std::size_t end = begin + std::min(states->size() - begin, range_size);
        range_nodes.push_back(builder.add(
            [states, ranges, begin, end, range, terrain, model_height, mesh_id,
             blue_material_value = blue_material, red_material_value = red_material,
             tick = batch->tick](jobs::JobContext&) {
                auto& output = (*ranges)[range];
                output.reserve(end - begin);
                for (std::size_t index = begin; index < end; ++index) {
                    const auto& state = (*states)[index];
                    foundation::Vec3 position = state.position;
                    if (terrain != nullptr) {
                        position.y = terrain->sampleBilinear(position.x, position.z) + 0.02F;
                    }
                    const foundation::StableId object_id =
                        foundation::stable_id("entity.infantry") ^ state.entity.packed();
                    const std::uint32_t flags =
                        render::RenderInstanceFlagDynamic |
                        render::RenderInstanceFlagCastShadow |
                        render::RenderInstanceFlagReceiveShadow |
                        (state.team == infantry::Team::Red
                             ? render::RenderInstanceFlagTeamRed
                             : 0U);
                    output.push_back(
                        {object_id,
                         mesh_id,
                         state.team == infantry::Team::Blue ? blue_material_value
                                                            : red_material_value,
                         position,
                         {state.height / model_height, state.height / model_height,
                          state.height / model_height},
                         infantryPresentationYaw(state.heading),
                         tick,
                         flags,
                         state.team == infantry::Team::Red
                             ? foundation::Color{1.0F, 0.78F, 0.72F, 1.0F}
                             : foundation::Color{0.78F, 0.87F, 1.0F, 1.0F}});
                }
            }, presentation_options));
    }
    const auto merge = builder.add(
        [batch, ranges](jobs::JobContext&) {
            std::size_t total = 0U;
            for (const auto& range : *ranges) total += range.size();
            batch->instances.reserve(total);
            for (auto& range : *ranges) {
                batch->instances.insert(batch->instances.end(), range.begin(), range.end());
            }
        }, presentation_options);
    for (const auto node : range_nodes) builder.precedes(node, merge);

    pending_mass_battle_presentation_ = batch;
    auto graph = std::move(builder).build();
    if (jobs_ == nullptr) {
        auto serial_group = graph.run(jobs::processScheduler());
        serial_group.wait();
        if (!serial_group.failed()) {
            ready_mass_battle_presentation_ = pending_mass_battle_presentation_;
        }
        pending_mass_battle_presentation_.reset();
        return;
    }
    mass_battle_presentation_group_ =
        std::make_unique<jobs::JobGroup>(graph.run(*jobs_));
}

void BattlefieldScene::consume_mass_battle_presentation() {
    if (!mass_battle_presentation_group_ ||
        !mass_battle_presentation_group_->isComplete()) {
        return;
    }
    if (!mass_battle_presentation_group_->failed()) {
        ready_mass_battle_presentation_ = pending_mass_battle_presentation_;
    }
    pending_mass_battle_presentation_.reset();
    mass_battle_presentation_group_.reset();
}
#endif

#if GENOMES_HAS_INFANTRY
void BattlefieldScene::set_mass_battle_profile(
    MassBattlePresentationProfile profile) noexcept {
    mass_battle_profile_ = profile;
    if (mode_ != BattlefieldSceneMode::InfantryMassBattle) return;
    // Profile changes are presentation-only.  The runtime, entity positions
    // and per-unit animation phase are deliberately left untouched.
    if (profile == MassBattlePresentationProfile::Quality ||
        !mass_battle_pose_atlas_ready_) {
        for (auto& agent : animation_agents_) agent.lod.setTier(infantry::AnimationLOD::Near);
    }
}
#endif

std::size_t BattlefieldScene::massBattleUnitCount() const noexcept {
    if (mode_ != BattlefieldSceneMode::InfantryMassBattle) return 0U;
#if GENOMES_HAS_INFANTRY
    if (mass_battle_session_ != nullptr)
        return mass_battle_session_->presentationSnapshot().states.size();
#endif
    // The configured population is also available while the scene is loading.
    return 2U * MassBattleUnitsPerTeam;
}

std::size_t BattlefieldScene::liveAnimationBudget() const noexcept {
    return std::min(live_animation_budget_, massBattleUnitCount());
}

ui::UiActionResult BattlefieldScene::handle_ui_action(
    SceneContext& context, ui::UiActionId action, const ui::UiActionArguments&) {
    if (mode_ != BattlefieldSceneMode::InfantryMassBattle) {
        return ui::UiActionResult::Unknown;
    }
    if (action == foundation::stable_id("mass-battle.restart")) {
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::OpenMassBattle, config_);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.generate-new")) {
        auto next_config = config_;
        next_config.seed = foundation::stableHashCombine(
            config_.seed, foundation::stableHashString("mass-battle.generate-new"));
        if (next_config.seed == 0U) next_config.seed = 1U;
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::OpenMassBattle,
            std::move(next_config));
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.next-terrain")) {
        auto next_config = config_;
        next_config.terrain.preset = nextTerrainPreset(next_config.terrain.preset);
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::OpenMassBattle,
            std::move(next_config));
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.next-terrain-detail")) {
        auto next_config = config_;
        next_config.terrain.sample_spacing_m =
            nextTerrainSampleSpacing(next_config.terrain.sample_spacing_m);
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::OpenMassBattle,
            std::move(next_config));
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.next-hydrology")) {
        auto next_config = config_;
        next_config.hydrology_mode = nextHydrologyMode(next_config.hydrology_mode);
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::OpenMassBattle,
            std::move(next_config));
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.frame-battle")) {
        mass_battle_frame_terrain_ = false;
        if (++mass_battle_camera_revision_ == 0U)
            mass_battle_camera_revision_ = MassBattleRtsCameraRevision;
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.frame-terrain")) {
        mass_battle_frame_terrain_ = true;
        if (++mass_battle_camera_revision_ == 0U)
            mass_battle_camera_revision_ = MassBattleRtsCameraRevision;
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.diagnostics-toggle")) {
        mass_battle_diagnostics_open_ = !mass_battle_diagnostics_open_;
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.diagnostics-overview")) {
        mass_battle_diagnostics_tab_ = MassBattleDiagnosticsTab::Overview;
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.diagnostics-terrain")) {
        mass_battle_diagnostics_tab_ = MassBattleDiagnosticsTab::Terrain;
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.diagnostics-hydrology")) {
        mass_battle_diagnostics_tab_ = MassBattleDiagnosticsTab::Hydrology;
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.diagnostics-performance")) {
        mass_battle_diagnostics_tab_ = MassBattleDiagnosticsTab::Performance;
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.diagnostics-units")) {
        mass_battle_diagnostics_tab_ = MassBattleDiagnosticsTab::Units;
        return ui::UiActionResult::Handled;
    }
#if GENOMES_HAS_INFANTRY
    constexpr std::size_t budget_step = 32U;
    if (action == foundation::stable_id("mass-battle.animation-decrease")) {
        const auto budget = liveAnimationBudget();
        live_animation_budget_ = budget - std::min(budget, budget_step);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.animation-increase")) {
        const auto budget = liveAnimationBudget();
        live_animation_budget_ = budget + std::min(budget_step, massBattleUnitCount() - budget);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("mass-battle.animation-maximum")) {
        live_animation_budget_ = massBattleUnitCount();
        return ui::UiActionResult::Handled;
    }
#endif
    if (action == foundation::stable_id("mass-battle.profile-quality")) {
#if GENOMES_HAS_INFANTRY
        set_mass_battle_profile(MassBattlePresentationProfile::Quality);
        return ui::UiActionResult::Handled;
#else
        return ui::UiActionResult::Unknown;
#endif
    }
    if (action == foundation::stable_id("mass-battle.profile-balanced")) {
#if GENOMES_HAS_INFANTRY
        set_mass_battle_profile(MassBattlePresentationProfile::Balanced);
        return ui::UiActionResult::Handled;
#else
        return ui::UiActionResult::Unknown;
#endif
    }
    if (action == foundation::stable_id("mass-battle.profile-stress")) {
#if GENOMES_HAS_INFANTRY
        set_mass_battle_profile(MassBattlePresentationProfile::Stress);
        return ui::UiActionResult::Handled;
#else
        return ui::UiActionResult::Unknown;
#endif
    }
    return ui::UiActionResult::Unknown;
}

void BattlefieldScene::handle_input(SceneContext& context, const input::InputFrame& input) {
    if (input.cancel_pressed || input.confirm_pressed) {
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::ReturnToMainMenu);
    }
}

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
    evaluate_infantry_animation(context);
#endif
}

void BattlefieldScene::frame_update(SceneContext& context, double dt) {
#if GENOMES_HAS_INFANTRY
    if (animation_job_.valid() && animation_job_.isComplete()) {
        const auto error = pending_animation_error_;
        const auto pending = pending_animation_context_;
        const bool job_failed = animation_job_.failed();
        animation_job_ = {};
        pending_animation_error_.reset();
        pending_animation_context_.reset();
        if (error && *error) {
            generation_error_ = std::string((*error)->message);
        } else if (job_failed) {
            generation_error_ = "infantry pose evaluation job failed";
        } else {
            publish_infantry_animation_result();
            if (context.engine_services != nullptr &&
                context.engine_services->telemetry != nullptr && animation_system_.has_value()) {
                context.engine_services->telemetry->animation_duration =
                    animation_system_->lastStats().evaluation_duration;
            }
        }
        if (pending && !simulation_failed_) {
            evaluate_infantry_animation(*pending);
        }
    }
    if (mass_battle_atlas_job_.valid() && mass_battle_atlas_job_.isComplete()) {
        const bool failed = mass_battle_atlas_job_.failed();
        const auto baked = pending_mass_battle_pose_meshes_;
        mass_battle_atlas_job_ = {};
        pending_mass_battle_pose_meshes_.reset();
        if (!failed && baked) {
            mass_battle_pose_meshes_ = *baked;
            mass_battle_pose_atlas_ready_ = true;
            mass_battle_pose_atlas_failed_ = false;
        } else {
            mass_battle_pose_meshes_.fill(nullptr);
            mass_battle_pose_atlas_ready_ = false;
            mass_battle_pose_atlas_failed_ = true;
        }
    }
    advance_mass_battle_loading();
    if (context.engine_services != nullptr && simulation_facade_ != nullptr) {
        context.engine_services->simulation = simulation_facade_;
    }
    if (context.engine_services != nullptr && context.engine_services->simulation != nullptr) {
        context.requestPresentationSnapshot(
            context.engine_services->simulation->snapshotView());
    }
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
        ? std::string{"WASD/arrows move | RMB rotate | MMB pan | wheel zoom | R frame battle"}
        : std::string{"Procedural world plan"});
    (void)model.set("error", std::string{});
    (void)model.set("seed", static_cast<std::int64_t>(config_.seed));
    (void)model.set("map_size", static_cast<std::int64_t>(config_.map_size_m));
    (void)model.set("features", mode_ == BattlefieldSceneMode::InfantryMassBattle
        ? std::string{"2000 infantry units in two deterministic formations"}
        : std::string{"World features pending"});
    (void)model.set("status", std::string{"Preparing world presentation..."});
    const std::string profile = mode_ == BattlefieldSceneMode::InfantryMassBattle
        ? massBattleProfileName(mass_battle_profile_) : "n/a";
    (void)model.set("profile", profile);
    (void)model.set("live_animation_budget", static_cast<std::int64_t>(liveAnimationBudget()));
    (void)model.set("live_animation_limit", static_cast<std::int64_t>(massBattleUnitCount()));
    (void)model.set("live_animation_note",
        mass_battle_profile_ == MassBattlePresentationProfile::Quality
            ? "Quality animates all units; budget applies to Balanced and Stress."
            : "Higher budgets animate more units smoothly and increase frame cost.");
    (void)model.set("animation_mode", mode_ == BattlefieldSceneMode::InfantryMassBattle
        ? massBattleAnimationMode(mass_battle_profile_) : "n/a");
    (void)model.set("orientation", mode_ == BattlefieldSceneMode::InfantryMassBattle
        ? "model +Z | blue +X | red -X" : "");
    const double fps = std::isfinite(dt) && dt > 0.0 ? 1.0 / dt : 0.0;
    (void)model.set("diagnostics_panel_open", mass_battle_diagnostics_open_);
    (void)model.set("diagnostics_panel_closed", !mass_battle_diagnostics_open_);
    (void)model.set("diag_tab_overview",
                    mass_battle_diagnostics_tab_ == MassBattleDiagnosticsTab::Overview);
    (void)model.set("diag_tab_terrain",
                    mass_battle_diagnostics_tab_ == MassBattleDiagnosticsTab::Terrain);
    (void)model.set("diag_tab_hydrology",
                    mass_battle_diagnostics_tab_ == MassBattleDiagnosticsTab::Hydrology);
    (void)model.set("diag_tab_performance",
                    mass_battle_diagnostics_tab_ == MassBattleDiagnosticsTab::Performance);
    (void)model.set("diag_tab_units",
                    mass_battle_diagnostics_tab_ == MassBattleDiagnosticsTab::Units);
    (void)model.set("terrain_preset", std::string{terrainPresetName(config_.terrain.preset)});
    (void)model.set("terrain_detail", std::to_string(config_.terrain.sample_spacing_m) + " m");
    (void)model.set("hydrology_mode", std::string{hydrologyModeName(config_.hydrology_mode)});
    (void)model.set("camera_frame",
                    std::string{mass_battle_frame_terrain_ ? "Terrain" : "Battle"});
    (void)model.set("diag_scene_epoch", std::to_string(scene_epoch_));
    (void)model.set("diag_fps", static_cast<std::int64_t>(fps + 0.5));
    (void)model.set("diag_draw_calls",
                    static_cast<std::int64_t>(context.render_telemetry.draw_calls));
    (void)model.set("diag_mesh_uploads",
                    static_cast<std::int64_t>(context.render_telemetry.mesh_uploads));
    (void)model.set("diag_palette_updates",
                    static_cast<std::int64_t>(context.render_telemetry.palette_updates));
    (void)model.set("diag_jobs_queued",
                    static_cast<std::int64_t>(context.scheduler_telemetry.queued));
    (void)model.set("diag_jobs_running",
                    static_cast<std::int64_t>(context.scheduler_telemetry.running));
    (void)model.set("diag_jobs_completed",
                    static_cast<std::int64_t>(context.scheduler_telemetry.completed));
    (void)model.set("diag_jobs_canceled",
                    static_cast<std::int64_t>(context.scheduler_telemetry.canceled));
    (void)model.set("diag_sim_ms", durationText(context.simulation_duration));
    (void)model.set("diag_presentation_ms", durationText(context.presentation_duration));
    (void)model.set("diag_animation_ms", std::string{"0.000"});
    (void)model.set("diag_extraction_ms", std::string{"0.000"});
    (void)model.set("diag_gpu_ms", durationText(context.gpu_duration));
    (void)model.set("diag_stale_snapshots",
                    static_cast<std::int64_t>(context.rejected_stale_snapshots));
    (void)model.set("diag_semantic_hash", std::string{"pending"});
    (void)model.set("diag_world_hash", std::string{"pending"});
    (void)model.set("diag_world_revision", std::string{"pending"});
    (void)model.set("diag_terrain_samples", std::string{"pending"});
    (void)model.set("diag_terrain_mesh", std::string{"pending"});
    (void)model.set("diag_elevation", std::string{"pending"});
    (void)model.set("diag_cell_size", std::string{"pending"});
    (void)model.set("diag_rivers", static_cast<std::int64_t>(0));
    (void)model.set("diag_crossings", static_cast<std::int64_t>(0));
    (void)model.set("diag_water_cells", static_cast<std::int64_t>(0));
    (void)model.set("diag_flood_cells", static_cast<std::int64_t>(0));
    (void)model.set("diag_tick", static_cast<std::int64_t>(0));
    (void)model.set("diag_units", static_cast<std::int64_t>(massBattleUnitCount()));
    (void)model.set("diag_blue_units", static_cast<std::int64_t>(0));
    (void)model.set("diag_red_units", static_cast<std::int64_t>(0));
    (void)model.set("diag_average_speed", std::string{"0.0"});
    (void)model.set("diag_direction_changes", static_cast<std::int64_t>(0));
    (void)model.set("diag_published_units",
                    static_cast<std::int64_t>(mass_battle_visible_units_));
    (void)model.set("diag_atlas_slots",
                    static_cast<std::int64_t>(mass_battle_active_pose_slots_));
    (void)model.set("diag_animation_summary",
        "idle " + std::to_string(mass_battle_archetype_counts_[0U]) +
        " | walk " + std::to_string(mass_battle_archetype_counts_[1U]) +
        " | run " + std::to_string(mass_battle_archetype_counts_[2U]) +
        " | crouch " + std::to_string(mass_battle_archetype_counts_[3U]) +
        " | prone " + std::to_string(mass_battle_archetype_counts_[5U]));
    std::string diagnostics = "profile " + profile + " | FPS " +
        std::to_string(static_cast<int>(fps + 0.5)) +
        " | draw calls " + std::to_string(context.render_telemetry.draw_calls) +
        " | mesh uploads " + std::to_string(context.render_telemetry.mesh_uploads) +
        " | palette updates " + std::to_string(context.render_telemetry.palette_updates) +
        " | CPU sim running " + std::to_string(
            context.scheduler_telemetry.running_by_class[
                static_cast<std::size_t>(jobs::WorkClass::Simulation)]) +
        " | CPU presentation running " + std::to_string(
            context.scheduler_telemetry.running_by_class[
                static_cast<std::size_t>(jobs::WorkClass::Presentation)]) +
        " | GPU draw calls " + std::to_string(context.render_telemetry.draw_calls);
    if (context.engine_services != nullptr && context.engine_services->telemetry != nullptr) {
        const auto& telemetry = *context.engine_services->telemetry;
        (void)model.set("diag_sim_ms",
                        std::to_string(telemetry.simulation_duration.count() / 1'000'000.0));
        (void)model.set("diag_presentation_ms",
                        std::to_string(telemetry.presentation_duration.count() / 1'000'000.0));
        (void)model.set("diag_animation_ms",
                        std::to_string(telemetry.animation_duration.count() / 1'000'000.0));
        (void)model.set("diag_extraction_ms",
                        std::to_string(telemetry.extraction_duration.count() / 1'000'000.0));
        (void)model.set("diag_gpu_ms",
                        std::to_string(telemetry.gpu_duration.count() / 1'000'000.0));
        (void)model.set("diag_stale_snapshots",
                        static_cast<std::int64_t>(telemetry.rejected_stale_snapshots));
        (void)model.set("diag_semantic_hash", std::to_string(telemetry.semantic_hash));
        diagnostics += " | sim ms " + std::to_string(
                           telemetry.simulation_duration.count() / 1'000'000.0) +
                       " | presentation ms " + std::to_string(
                           telemetry.presentation_duration.count() / 1'000'000.0) +
                       " | GPU ms " + std::to_string(
                           telemetry.gpu_duration.count() / 1'000'000.0) +
                       " | stale snapshots " + std::to_string(
                           telemetry.rejected_stale_snapshots);
    }
    if (mode_ == BattlefieldSceneMode::InfantryMassBattle) {
        diagnostics += " | published " + std::to_string(mass_battle_visible_units_) +
                       " | atlas slots " + std::to_string(mass_battle_active_pose_slots_) +
                       " | idle " + std::to_string(mass_battle_archetype_counts_[0U]) +
                       " walk " + std::to_string(mass_battle_archetype_counts_[1U]) +
                       " run " + std::to_string(mass_battle_archetype_counts_[2U]) +
                       " crouch " + std::to_string(mass_battle_archetype_counts_[3U]) +
                       " crouch-walk " + std::to_string(mass_battle_archetype_counts_[4U]) +
                       " prone " + std::to_string(mass_battle_archetype_counts_[5U]) +
                       " prone-move " + std::to_string(mass_battle_archetype_counts_[6U]) +
                       " weapon-ready " + std::to_string(mass_battle_archetype_counts_[7U]);
        if (mass_battle_profile_ == MassBattlePresentationProfile::Quality) {
            diagnostics += " | poses evaluated " +
                           std::to_string(mass_battle_evaluated_poses_) +
                           " | LOD near " + std::to_string(mass_battle_lod_counts_[0U]) +
                           " mid " + std::to_string(mass_battle_lod_counts_[1U]) +
                           " far " + std::to_string(mass_battle_lod_counts_[2U]);
        } else {
            diagnostics += std::string{" | atlas | phases "} +
                           (mass_battle_profile_ == MassBattlePresentationProfile::Stress
                                ? "full" : "adaptive") +
                           " | shadows " +
                           (mass_battle_profile_ == MassBattlePresentationProfile::Stress
                                ? "full" : "180m");
        }
    }
    (void)model.set("diagnostics",std::move(diagnostics));
    if (plan_) {
        (void)model.set("seed", static_cast<std::int64_t>(plan_->seed));
        (void)model.set("map_size", static_cast<std::int64_t>(plan_->map_size_m));
        (void)model.set("features", feature_summary(*plan_));
#if GENOMES_HAS_INFANTRY
        const std::size_t infantry_count = mass_battle_session_ != nullptr
                                                ? mass_battle_session_->presentationSnapshot().states.size()
                                                : battlefield_runtime_ != nullptr
                                                    ? battlefield_runtime_->presentationSnapshot().states.size()
                                                    : 0U;
#else
        constexpr std::size_t infantry_count = 0U;
#endif
        (void)model.set("units", static_cast<std::int64_t>(infantry_count));
#if GENOMES_HAS_INFANTRY
        if (const auto mass = mass_battle_session_ != nullptr
                                  ? mass_battle_session_->massBattleSnapshot()
                                  : std::optional<gameplay::InfantryMassBattleSnapshot>{}; mass) {
            (void)model.set("diag_tick", static_cast<std::int64_t>(mass->tick));
            (void)model.set("diag_units", static_cast<std::int64_t>(mass->total_units));
            (void)model.set("diag_blue_units", static_cast<std::int64_t>(mass->blue_units));
            (void)model.set("diag_red_units", static_cast<std::int64_t>(mass->red_units));
            (void)model.set("diag_average_speed", std::to_string(mass->average_speed_mps));
            (void)model.set("diag_direction_changes",
                            static_cast<std::int64_t>(mass->direction_changes));
            (void)model.set("viability", "Mass battle: tick " + std::to_string(mass->tick) +
                                " units " + std::to_string(mass->total_units) +
                                " avg speed " + std::to_string(mass->average_speed_mps) +
                                " m/s turns " + std::to_string(mass->direction_changes));
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
        (void)model.set("diag_world_hash", std::to_string(plan_->content_hash));
        (void)model.set("diag_world_revision",
                        std::to_string(world::artifactRevision(*plan_)));
        (void)model.set("diag_terrain_samples",
                        std::to_string(terrain_->width()) + " x " +
                        std::to_string(terrain_->height()));
        (void)model.set("diag_terrain_mesh",
                        std::to_string(terrain_mesh_->vertices.size()) + " vertices | " +
                        std::to_string(terrain_mesh_->triangle_count()) + " triangles");
        (void)model.set("diag_elevation", std::to_string(terrain_min_height_) + " .. " +
                                               std::to_string(terrain_max_height_) + " m");
        (void)model.set("diag_cell_size", std::to_string(terrain_->cellSize()) + " m");
        (void)model.set("diag_rivers",
                        static_cast<std::int64_t>(plan_->hydrology.rivers.size()));
        (void)model.set("diag_crossings",
                        static_cast<std::int64_t>(plan_->hydrology.crossings.size()));
        (void)model.set("diag_water_cells",
                        static_cast<std::int64_t>(hydrology_water_cells_));
        (void)model.set("diag_flood_cells",
                        static_cast<std::int64_t>(hydrology_flood_cells_));
        (void)model.set("status",
            mode_ == BattlefieldSceneMode::InfantryMassBattle &&
                    mass_battle_profile_ != MassBattlePresentationProfile::Quality &&
                    !mass_battle_pose_atlas_ready_
                ? std::string{"Baking animation atlas..."}
                : std::string{"World plan ready for terrain, navigation and rendering."});
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
#if GENOMES_HAS_INFANTRY
    if (mode_ == BattlefieldSceneMode::InfantryMassBattle &&
        mass_battle_session_ != nullptr && infantry_model_artifact_ != nullptr &&
        mass_battle_profile_ != MassBattlePresentationProfile::Quality &&
        !mass_battle_pose_atlas_ready_) {
        (void)model.set("status", mass_battle_pose_atlas_failed_
            ? std::string{"Animation atlas unavailable; using fallback rendering."}
            : std::string{"Baking animation atlas..."});
    }
#endif
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

void BattlefieldScene::build_presentation(SceneContext& context) {
#if GENOMES_HAS_INFANTRY
    if (mass_battle_session_ != nullptr && mass_battle_frame_terrain_ && terrain_ != nullptr) {
        const foundation::Vec3 center{0.0F, terrain_->sampleBilinear(0.0, 0.0) + 2.0F, 0.0F};
        constexpr float initial_pitch = 0.558505361F;
        const float half_extent = static_cast<float>(config_.map_size_m) * 0.5F;
        const float distance = std::clamp(
            half_extent / std::tan(camera_request_.lens.vertical_fov * 0.5F),
            rts_controls_.min_distance, rts_controls_.max_distance);
        camera_request_.target = center;
        camera_request_.position = {center.x, center.y + std::sin(initial_pitch) * distance,
                                    center.z + std::cos(initial_pitch) * distance};
    } else if (mass_battle_session_ != nullptr &&
               !mass_battle_session_->presentationSnapshot().states.empty()) {
        foundation::Vec3 minimum{std::numeric_limits<float>::max(),0.0F,
                                 std::numeric_limits<float>::max()};
        foundation::Vec3 maximum{std::numeric_limits<float>::lowest(),0.0F,
                                 std::numeric_limits<float>::lowest()};
        for (const auto& state : mass_battle_session_->presentationSnapshot().states) {
            minimum.x=std::min(minimum.x,state.position.x);
            minimum.z=std::min(minimum.z,state.position.z);
            maximum.x=std::max(maximum.x,state.position.x);
            maximum.z=std::max(maximum.z,state.position.z);
        }
        foundation::Vec3 center{(minimum.x+maximum.x)*0.5F,0.0F,
                                (minimum.z+maximum.z)*0.5F};
        center.y = terrain_ != nullptr ? terrain_->sampleBilinear(center.x, center.z) + 2.0F
                                       : 2.0F;
        constexpr float initial_pitch=0.558505361F;
        const float half_extent=std::max(maximum.x-minimum.x,maximum.z-minimum.z)*0.5F+10.0F;
        const float distance=std::clamp(
            half_extent / std::tan(camera_request_.lens.vertical_fov * 0.5F),
            rts_controls_.min_distance, rts_controls_.max_distance);
        camera_request_.target = center;
        camera_request_.position = {center.x,center.y+std::sin(initial_pitch)*distance,
                                    center.z+std::cos(initial_pitch)*distance};
    }
    if (mass_battle_session_ != nullptr)
        context.presentation.camera.revision = mass_battle_camera_revision_;
#endif
    context.publishCameraRequest(camera_request_);
    context.presentation.terrain_mesh = render_terrain_mesh_;
    context.presentation.world_mesh = render_world_mesh_;
    if (!plan_) {
#if GENOMES_HAS_INFANTRY
        if (mass_battle_session_ == nullptr)
#endif
        {
            render_infantry_mesh_.reset();
            return;
        }
    }

#if GENOMES_HAS_INFANTRY
    if (battlefield_runtime_ != nullptr || mass_battle_session_ != nullptr) {
        if (infantry_model_artifact_) {
            if (!infantry_skinned_prototype_) {
                infantry_skinned_prototype_ = infantry_presentation::makePrototype(
                    *infantry_model_artifact_,
                    infantry_presentation::PrototypePreparation::OptimizeDrawOrder,
                    procedural_runtime_.get(),
                    infantry_presentation::WeaponPoseAttachment::RightHand);
            }
            if (infantry_skinned_prototype_) {
                const auto bind_palette = infantry_presentation::makeBindPalette(
                    infantry_model_artifact_->skeleton);
                const auto bind_local_poses = infantry_presentation::makeLocalPoses(
                    infantry_model_artifact_->skeleton, {});
                const bool mass_battle_atlas_requested =
                    mass_battle_session_ != nullptr &&
                    mass_battle_profile_ != MassBattlePresentationProfile::Quality;
                const bool mass_battle_atlas = mass_battle_atlas_requested &&
                                               mass_battle_pose_atlas_ready_;
                std::unordered_map<foundation::StableId, const infantry::AnimationPose*> poses;
                poses.reserve(animation_poses_.size());
                for (const auto& pose : animation_poses_)
                    poses.emplace(pose.semantic_id, &pose);
                std::optional<render::PoseSnapshotExchange::ReadLease> pose_read;
                std::unordered_map<foundation::StableId,
                                   const render::SkinnedBonePalette*> published_palettes;
                bool received_new_pose = false;
                if (mass_battle_session_ != nullptr) {
                    auto latest_pose = pose_exchange_.acquireLatestRead();
                    if (latest_pose) {
                        pose_read.emplace(std::move(latest_pose.value()));
                        const auto& snapshot = pose_read->snapshot();
                        received_new_pose = snapshot.metadata.tick > last_pose_tick_ ||
                                             snapshot.metadata.revision > last_pose_revision_;
                        if (received_new_pose) {
                            last_pose_tick_ = snapshot.metadata.tick;
                            last_pose_revision_ = snapshot.metadata.revision;
                        }
                        published_palettes.reserve(snapshot.palettes.size());
                        for (const auto& palette : snapshot.palettes) {
                            published_palettes.emplace(palette.instance_id, &palette);
                        }
                    }
                    if (received_new_pose) {
                        pose_stall_frames_ = 0U;
                    } else if (pose_stall_frames_ < 8U) {
                        ++pose_stall_frames_;
                    }
                }
                if (mass_battle_atlas_requested && !mass_battle_pose_atlas_ready_ &&
                    !mass_battle_pose_atlas_failed_ && !poses.empty()) {
                    request_mass_battle_pose_atlas();
                }
                if (mass_battle_profile_ == MassBattlePresentationProfile::Balanced &&
                    mass_battle_atlas_requested && !mass_battle_pose_atlas_ready_ &&
                    context.render_capabilities.gpu_skinning && !render_infantry_mesh_) {
                    render_infantry_mesh_ = std::make_shared<render::RenderMesh>(
                        render::deformSkinnedCPU(*infantry_skinned_prototype_, bind_palette));
                    render_infantry_mesh_->mesh_id =
                        foundation::stable_id("mesh.infantry.mass-battle.bind-fallback");
                    render_infantry_mesh_->revision = foundation::stableHashCombine(
                        infantry_skinned_prototype_->revision, render_infantry_mesh_->mesh_id);
                } else if (!mass_battle_atlas && !context.render_capabilities.gpu_skinning &&
                           !render_infantry_mesh_) {
                    render_infantry_mesh_ = std::make_shared<render::RenderMesh>(
                        render::deformSkinnedCPU(*infantry_skinned_prototype_, bind_palette));
                    render_infantry_mesh_->mesh_id = infantry_skinned_prototype_->mesh_id;
                    render_infantry_mesh_->revision = infantry_skinned_prototype_->revision;
                }
                // Keep the last complete pose for a short presentation stall,
                // then fall back to the immutable atlas (handled above) or a
                // bind palette. This never blocks the frame thread and avoids
                // replaying unboundedly stale animation data.
                if (mass_battle_session_ != nullptr && pose_stall_frames_ >= 8U &&
                    !mass_battle_atlas) {
                    poses.clear();
                    published_palettes.clear();
                }
                if (mass_battle_atlas) {
                    for (const auto& mesh : mass_battle_pose_meshes_) {
                        if (mesh) context.presentation.instance_prototypes.push_back(mesh);
                    }
                    if (context.render_capabilities.gpu_skinning && !poses.empty()) {
                        context.presentation.skinned_prototypes.push_back(
                            infantry_skinned_prototype_);
                    }
                } else if (mass_battle_profile_ == MassBattlePresentationProfile::Balanced &&
                           mass_battle_atlas_requested &&
                           !mass_battle_pose_atlas_ready_ &&
                           context.render_capabilities.gpu_skinning) {
                    if (render_infantry_mesh_) {
                        context.presentation.instance_prototypes.push_back(render_infantry_mesh_);
                    }
                    if (!poses.empty()) {
                        context.presentation.skinned_prototypes.push_back(
                            infantry_skinned_prototype_);
                    }
                } else if (!context.render_capabilities.gpu_skinning) {
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
                if (mass_battle_session_ != nullptr) {
                    mass_battle_archetype_counts_.fill(0U);
                    for (const auto& state : mass_battle_session_->presentationSnapshot().states) {
                        const auto index = massBattleArchetypeIndex(state.animation_variant);
                        if (index < mass_battle_archetype_counts_.size()) {
                            ++mass_battle_archetype_counts_[index];
                        }
                    }
                }
                if (mass_battle_atlas) {
                    const auto& states=mass_battle_session_->presentationSnapshot().states;
                    context.presentation.instances.reserve(
                        context.presentation.instances.size()+states.size());
                    const foundation::Vec3 camera_position=context.presentation.camera.enabled
                        ? context.presentation.camera.position : camera_request_.position;
                    const foundation::Vec3 camera_target=context.presentation.camera.enabled
                        ? context.presentation.camera.target : camera_request_.target;
                    const float focal_pixels=static_cast<float>(std::max(1,context.framebuffer_height))*
                        0.5F/std::tan(camera_request_.lens.vertical_fov*0.5F);
                    std::array<bool,MassBattlePoseAtlasSize> used_pose_slots{};
                    std::size_t visible_units=0U;
                    for (const auto& state:states) {
                        foundation::Vec3 presentation_position=state.position;
                        if (terrain_!=nullptr) presentation_position.y=terrain_->sampleBilinear(
                            presentation_position.x,presentation_position.z)+0.02F;
                        const foundation::Vec3 center=presentation_position+
                            foundation::Vec3{0.0F,state.height*0.5F,0.0F};
                        const float shadow_dx=presentation_position.x-camera_target.x;
                        const float shadow_dz=presentation_position.z-camera_target.z;
                        const bool casts_shadow=mass_battle_profile_ ==
                            MassBattlePresentationProfile::Stress ||
                            shadow_dx*shadow_dx+shadow_dz*shadow_dz<=180.0F*180.0F;
                        // Preserve off-screen casters inside the existing
                        // shadow region. Stress mode intentionally submits the
                        // full population for renderer load measurements.
                        if (!massBattleUnitShouldRender(
                                context.presentation, mass_battle_profile_,
                                presentation_position, state.height, camera_target)) {
                            continue;
                        }
                        const float distance=std::max(0.01F,math::length(camera_position-center));
                        const float pixel_height=state.height*focal_pixels/distance;
                        const std::size_t variant=state.animation_variant%MassBattlePoseVariantCount;
                        const std::size_t native_phases=MassBattlePosePhaseCounts[variant];
                        const std::size_t visible_phases =
                            mass_battle_profile_ == MassBattlePresentationProfile::Stress
                                ? native_phases
                                : pixel_height >= 8.0F ? native_phases
                                : pixel_height >= 3.0F ? std::min(native_phases, std::size_t{6U})
                                : pixel_height >= 1.5F ? std::min(native_phases, std::size_t{2U})
                                                        : 1U;
                        const std::size_t atlas_slot=massBattlePoseBucket(
                            variant,state.animation_phase,visible_phases);
                        const auto& mesh=mass_battle_pose_meshes_[atlas_slot];
                        const foundation::StableId object_id=
                            foundation::stable_id("entity.infantry")^state.entity.packed();
                        const auto pose_found = poses.find(
                            foundation::stable_id("battlefield.infantry")^
                            state.entity.packed());
                        const bool use_live_pose = context.render_capabilities.gpu_skinning &&
                                                   pose_found != poses.end();
                        if (!use_live_pose && !mesh) continue;
                        if (!use_live_pose) used_pose_slots[atlas_slot]=true;
                        ++visible_units;
                        std::uint32_t flags=render::RenderInstanceFlagDynamic|
                            render::RenderInstanceFlagReceiveShadow|
                            (state.team==infantry::Team::Red?render::RenderInstanceFlagTeamRed:0U);
                        if (casts_shadow) flags|=render::RenderInstanceFlagCastShadow;
                        if (use_live_pose) {
                            const auto published = published_palettes.find(object_id);
                            if (published != published_palettes.end()) {
                                context.presentation.skinned_palettes.push_back(
                                    *published->second);
                            } else {
                                render::SkinnedBonePalette palette{};
                                palette.instance_id = object_id;
                                palette.skeleton_id = infantry_model_artifact_->skeleton.cacheKey();
                                palette.pose_revision = pose_found->second->revision;
                                palette.matrices = infantry_presentation::makePalette(
                                    infantry_model_artifact_->skeleton,
                                    std::span<const infantry::RigTransform>(
                                        pose_found->second->bones));
                                context.presentation.skinned_palettes.push_back(
                                    std::move(palette));
                            }
                        }
                        context.presentation.instances.push_back(
                            {object_id,use_live_pose ? infantry_skinned_prototype_->mesh_id
                                                     : mesh->mesh_id,
                             state.team==infantry::Team::Blue?blue_material:red_material,
                             presentation_position,
                             {state.height/model_height,state.height/model_height,state.height/model_height},
                             infantryPresentationYaw(state.heading),
                             mass_battle_session_->presentationSnapshot().metadata.tick,flags,
                             state.team==infantry::Team::Red
                                 ? foundation::Color{1.0F,0.78F,0.72F,1.0F}
                                 : foundation::Color{0.78F,0.87F,1.0F,1.0F}});
                    }
                    mass_battle_visible_units_=visible_units;
                    mass_battle_active_pose_slots_=static_cast<std::size_t>(std::count(
                        used_pose_slots.begin(),used_pose_slots.end(),true));
                    return;
                }

                const auto publish_live = [&](simulation::EntityId entity,
                                              infantry::Team team,
                                              foundation::Vec3 position,
                                              float heading,
                                              float height,
                                              std::uint64_t presentation_tick,
                                              const infantry::AnimationPose* pose,
                                              const render::SkinnedBonePalette* published_palette = nullptr,
                                              bool emit_instance = true) {
                    const foundation::StableId object_id =
                        foundation::stable_id("entity.infantry") ^ entity.packed();
                    render::SkinnedBonePalette palette{};
                    palette.instance_id = object_id;
                    palette.skeleton_id = infantry_model_artifact_->skeleton.cacheKey();
                    palette.pose_revision = pose != nullptr ? pose->revision : 0U;
                    if (published_palette != nullptr) {
                        palette = *published_palette;
                        palette.instance_id = object_id;
                    } else if (pose != nullptr) {
                        const auto pose_span=std::span<const infantry::RigTransform>(pose->bones);
                        palette.matrices=infantry_presentation::makePalette(
                            infantry_model_artifact_->skeleton,pose_span);
                        // Mass Battle is consumed by the GPU skinning path;
                        // retaining a second local-space copy for every one
                        // of 2000 units only duplicates the same skeleton
                        // contract and creates avoidable allocator churn.
                        if (mass_battle_session_ == nullptr) {
                            palette.local_poses=infantry_presentation::makeLocalPoses(
                                infantry_model_artifact_->skeleton,pose_span);
                        }
                        palette.morph_weights[0]=pose->face.eyelids_close;
                        palette.morph_weights[1]=pose->face.eyelids_arc;
                        palette.morph_weights[2]=pose->face.neck_flex;
                        palette.morph_weights[3]=pose->face.hands_relax;
                    } else {
                        palette.matrices=bind_palette;
                        palette.local_poses=bind_local_poses;
                    }
                    context.presentation.skinned_palettes.push_back(std::move(palette));
                    if (!emit_instance) return;
                    const std::uint32_t instance_flags =
                        render::RenderInstanceFlagDynamic |
                        render::RenderInstanceFlagCastShadow |
                        render::RenderInstanceFlagReceiveShadow |
                        (team == infantry::Team::Red
                             ? render::RenderInstanceFlagTeamRed
                             : 0U);
                    context.presentation.instances.push_back(
                        {object_id,
                         infantry_skinned_prototype_->mesh_id,
                         team == infantry::Team::Blue ? blue_material : red_material,
                         position,
                         {height / model_height, height / model_height, height / model_height},
                         infantryPresentationYaw(heading),
                         presentation_tick,
                         instance_flags,
                         team == infantry::Team::Red
                             ? foundation::Color{1.0F, 0.78F, 0.72F, 1.0F}
                             : foundation::Color{0.78F, 0.87F, 1.0F, 1.0F}});
                };
                if (mass_battle_session_ != nullptr) {
                    const bool balanced_atlas_warmup =
                        mass_battle_profile_ == MassBattlePresentationProfile::Balanced &&
                        mass_battle_atlas_requested && !mass_battle_pose_atlas_ready_;
                    consume_mass_battle_presentation();
                    if (balanced_atlas_warmup) {
                        // This asynchronous batch is the legacy all-skinned
                        // fallback. Drop it during atlas warmup so it cannot
                        // reintroduce 2000 GPU palettes after the static path
                        // has taken over.
                        ready_mass_battle_presentation_.reset();
                    }
                    if (!ready_mass_battle_presentation_) {
                        if (!balanced_atlas_warmup) schedule_mass_battle_presentation();
                    }
                    if (ready_mass_battle_presentation_) {
                        const auto ready = ready_mass_battle_presentation_;
                        context.presentation.instances.reserve(
                            context.presentation.instances.size() + ready->instances.size());
                        context.presentation.skinned_palettes.reserve(
                            context.presentation.skinned_palettes.size() + ready->states.size());
                        for (std::size_t index = 0U; index < ready->states.size(); ++index) {
                            const auto& state = ready->states[index];
                            const auto pose_found = poses.find(
                                foundation::stable_id("battlefield.infantry") ^
                                state.entity.packed());
                            const auto palette_found = published_palettes.find(
                                foundation::stable_id("entity.infantry") ^ state.entity.packed());
                            publish_live(state.entity, state.team, state.position, state.heading,
                                         state.height, ready->tick,
                                         pose_found != poses.end() ? pose_found->second : nullptr,
                                         palette_found != published_palettes.end()
                                             ? palette_found->second
                                             : nullptr,
                                         false);
                            context.presentation.instances.push_back(ready->instances[index]);
                        }
                        mass_battle_visible_units_ = ready->instances.size();
                        mass_battle_active_pose_slots_ = 0U;
                        ready_mass_battle_presentation_.reset();
                        return;
                    }
                    const auto& states = mass_battle_session_->presentationSnapshot().states;
                    context.presentation.instances.reserve(
                        context.presentation.instances.size() + states.size());
                    context.presentation.skinned_palettes.reserve(
                        context.presentation.skinned_palettes.size() +
                        (balanced_atlas_warmup ? poses.size() : states.size()));
                    const foundation::Vec3 camera_target =
                        context.presentation.camera.enabled
                            ? context.presentation.camera.target
                            : camera_request_.target;
                    std::size_t visible_units = 0U;
                    for (const auto& state : states) {
                        foundation::Vec3 position = state.position;
                        if (terrain_ != nullptr) {
                            position.y = terrain_->sampleBilinear(position.x, position.z) + 0.02F;
                        }
                        if (balanced_atlas_warmup &&
                            !massBattleUnitShouldRender(context.presentation,
                                                        mass_battle_profile_, position,
                                                        state.height, camera_target)) {
                            continue;
                        }
                        const auto pose_found = poses.find(
                            foundation::stable_id("battlefield.infantry") ^ state.entity.packed());
                        if (balanced_atlas_warmup &&
                            pose_found == poses.end() && render_infantry_mesh_) {
                            const foundation::StableId object_id =
                                foundation::stable_id("entity.infantry") ^ state.entity.packed();
                            const float shadow_dx = position.x - camera_target.x;
                            const float shadow_dz = position.z - camera_target.z;
                            const bool casts_shadow = shadow_dx * shadow_dx +
                                shadow_dz * shadow_dz <= 180.0F * 180.0F;
                            std::uint32_t flags = render::RenderInstanceFlagDynamic |
                                render::RenderInstanceFlagReceiveShadow |
                                (state.team == infantry::Team::Red
                                     ? render::RenderInstanceFlagTeamRed : 0U);
                            if (casts_shadow) flags |= render::RenderInstanceFlagCastShadow;
                            context.presentation.instances.push_back(
                                {object_id, render_infantry_mesh_->mesh_id,
                                 state.team == infantry::Team::Blue ? blue_material : red_material,
                                 position,
                                 {state.height / model_height, state.height / model_height,
                                  state.height / model_height},
                                 infantryPresentationYaw(state.heading),
                                 mass_battle_session_->presentationSnapshot().metadata.tick, flags,
                                 state.team == infantry::Team::Red
                                     ? foundation::Color{1.0F, 0.78F, 0.72F, 1.0F}
                                     : foundation::Color{0.78F, 0.87F, 1.0F, 1.0F}});
                        } else {
                            const auto palette_found = published_palettes.find(
                                foundation::stable_id("entity.infantry") ^ state.entity.packed());
                            publish_live(state.entity, state.team, position, state.heading,
                                         state.height, mass_battle_session_->presentationSnapshot().metadata.tick,
                                         pose_found != poses.end() ? pose_found->second : nullptr,
                                         palette_found != published_palettes.end()
                                             ? palette_found->second : nullptr);
                        }
                        ++visible_units;
                    }
                    mass_battle_visible_units_ = visible_units;
                    mass_battle_active_pose_slots_ = 0U;
                    return;
                }

                const std::uint64_t presentation_tick = battlefield_runtime_->snapshot().tick;
                const auto& presentation_states = battlefield_runtime_->presentationSnapshot().states;
                context.presentation.instances.reserve(context.presentation.instances.size() +
                                                       presentation_states.size());
                context.presentation.skinned_palettes.reserve(
                    context.presentation.skinned_palettes.size() + presentation_states.size());
                for (const infantry::InfantryRenderState& state : presentation_states) {
                    const foundation::StableId pose_id =
                        foundation::stable_id("battlefield.infantry") ^ state.entity.packed();
                    const auto pose_found = poses.find(pose_id);
                    publish_live(state.entity, state.team, state.position, state.heading,
                                 state.height, presentation_tick,
                                 pose_found != poses.end() ? pose_found->second : nullptr);
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
