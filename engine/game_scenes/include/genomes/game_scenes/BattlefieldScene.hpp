#pragma once

#include <genomes/combat/TacticalAI.hpp>
#include <genomes/buildings/BuildingModel.hpp>
#include <genomes/buildings/BuildingProfile.hpp>
#include <genomes/camera/Camera.hpp>
#if GENOMES_HAS_INFANTRY
#include <genomes/gameplay/BattlefieldRuntime.hpp>
#include <genomes/gameplay/InfantryMassBattleRuntime.hpp>
#endif
#include <genomes/gameplay/WorldScenario.hpp>
#if GENOMES_HAS_INFANTRY
#include <genomes/infantry/InfantryModelCompiler.hpp>
#include <genomes/infantry/PresentationAnimation.hpp>
#include <genomes/infantry/InfantryProcedural.hpp>
#include <genomes/infantry/FaceAnimation.hpp>
#include <genomes/infantry/LocomotionController.hpp>
#endif
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/jobs/JobGraph.hpp>
#include <genomes/proc/ProceduralRuntime.hpp>
#include <genomes/render/RenderTypes.hpp>
#include <genomes/render/PoseSnapshot.hpp>
#include <genomes/render/SkinnedDeformer.hpp>
#include <genomes/runtime/Scene.hpp>
#include <genomes/game_scenes/WorldConfig.hpp>
#include <genomes/terrain/HeightField.hpp>
#include <genomes/terrain/TerrainMesh.hpp>
#include <genomes/world/WorldPlan.hpp>
#include <genomes/world/WorldRegionStreamer.hpp>
#include <genomes/world_render/WorldMeshCompiler.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace genomes::game_scenes {

using runtime::Scene;
using runtime::SceneContext;

enum class BattlefieldSceneMode : std::uint8_t {
    Tactical,
    InfantryMassBattle,
};

enum class MassBattlePresentationProfile : std::uint8_t {
    Quality,
    Balanced,
    Stress,
};

enum class MassBattleAnimationArchetype : std::uint8_t {
    Idle,
    Walk,
    Run,
    Crouch,
    CrouchWalk,
    Prone,
    ProneMove,
    WeaponReady,
};

[[nodiscard]] constexpr MassBattleAnimationArchetype
massBattleAnimationArchetype(std::uint8_t variant) noexcept {
    switch (variant % 8U) {
    case 0U: return MassBattleAnimationArchetype::Idle;
    case 1U: return MassBattleAnimationArchetype::Walk;
    case 2U: return MassBattleAnimationArchetype::Run;
    case 3U: return MassBattleAnimationArchetype::Crouch;
    case 4U: return MassBattleAnimationArchetype::CrouchWalk;
    case 5U: return MassBattleAnimationArchetype::Prone;
    case 6U: return MassBattleAnimationArchetype::ProneMove;
    default: return MassBattleAnimationArchetype::WeaponReady;
    }
}

class BattlefieldScene final : public Scene {
public:
    BattlefieldScene(application::WorldGenerationConfig config,
                     std::shared_ptr<const buildings::FrozenBuildingProfile> building_profile,
                     combat::TacticalAIProfile tactical_ai_profile = {},
                     BattlefieldSceneMode mode = BattlefieldSceneMode::Tactical,
                     std::size_t live_animation_budget = 96U,
                     camera::RtsCameraSettings rts_controls = {})
        : config_{std::move(config)}, building_profile_{std::move(building_profile)},
          live_animation_budget_{live_animation_budget},
          rts_controls_{rts_controls}, tactical_ai_profile_{tactical_ai_profile},
          mode_{mode} {}

    ~BattlefieldScene() override;

    [[nodiscard]] foundation::SceneId id() const noexcept override;
    [[nodiscard]] runtime::SceneLoadingStatus loading_status() const override;
    void on_enter(SceneContext&) override;
    void on_exit(SceneContext&) override;
    void handle_input(SceneContext&, const input::InputFrame&) override;
    [[nodiscard]] ui::UiActionResult handle_ui_action(
        SceneContext&, ui::UiActionId, const ui::UiActionArguments&) override;
    void fixed_update(SceneContext&, const simulation::TickContext&) override;
    void frame_update(SceneContext&, double) override;
    void build_presentation(SceneContext&) override;

    [[nodiscard]] const world::WorldPlan* plan() const noexcept {
        return plan_ ? &*plan_ : nullptr;
    }
    [[nodiscard]] bool simulationFailed() const noexcept { return simulation_failed_; }
    [[nodiscard]] MassBattlePresentationProfile massBattleProfile() const noexcept {
        return mass_battle_profile_;
    }
    // Balanced/Stress live-pose budget; Quality always animates every unit.
    [[nodiscard]] std::size_t liveAnimationBudget() const noexcept;
    [[nodiscard]] std::size_t massBattleUnitCount() const noexcept;
    [[nodiscard]] const camera::RtsCameraSettings& rtsControls() const noexcept {
        return rts_controls_;
    }
#if GENOMES_HAS_INFANTRY
    [[nodiscard]] bool massBattleAtlasReady() const noexcept {
        return mass_battle_pose_atlas_ready_;
    }
#endif

private:
    void finalize_plan(world::WorldPlan plan);
#if GENOMES_HAS_INFANTRY
    enum class MassBattleLoadStage : std::uint8_t {
        Inactive,
        Starting,
        CreateSimulation,
        CompileModel,
        Ready,
        Failed,
    };

    void advance_mass_battle_loading();
    void initialize_infantry_animation();
    void evaluate_infantry_animation(const simulation::TickContext& context);
    void publish_infantry_animation_result();
    void request_mass_battle_pose_atlas();
    void schedule_mass_battle_presentation();
    void consume_mass_battle_presentation();
    void set_mass_battle_profile(MassBattlePresentationProfile profile) noexcept;
#endif

    application::WorldGenerationConfig config_{};
    std::shared_ptr<const buildings::FrozenBuildingProfile> building_profile_;
    std::optional<world::WorldPlan> plan_;
    std::shared_ptr<const std::vector<buildings::BuildingGenerationResult>> resolved_buildings_;
    std::unique_ptr<gameplay::WorldScenario> scenario_;
    std::shared_ptr<const terrain::HeightField> terrain_;
    std::shared_ptr<const terrain::TerrainMesh> terrain_mesh_;
    std::shared_ptr<const gameplay::WorldScenarioArtifact> world_artifacts_;
    std::shared_ptr<const render::RenderMesh> render_terrain_mesh_;
    std::shared_ptr<const render::RenderMesh> render_world_mesh_;
    std::optional<world_render::WorldMeshArtifact> world_mesh_artifact_;
    std::shared_ptr<render::RenderMesh> render_infantry_mesh_;
    MassBattlePresentationProfile mass_battle_profile_{
        MassBattlePresentationProfile::Balanced};
    static constexpr std::uint32_t MassBattleUnitsPerTeam = 1000U;
    std::size_t live_animation_budget_{96U};
    camera::RtsCameraSettings rts_controls_{};
    static constexpr std::size_t MassBattlePoseVariantCount = 8U;
    static constexpr std::size_t MassBattlePoseAtlasSize = 44U;
    std::array<std::shared_ptr<render::RenderMesh>, MassBattlePoseAtlasSize>
        mass_battle_pose_meshes_{};
    jobs::JobHandle mass_battle_atlas_job_;
    std::shared_ptr<std::array<std::shared_ptr<render::RenderMesh>, MassBattlePoseAtlasSize>>
        pending_mass_battle_pose_meshes_;
    bool mass_battle_pose_atlas_ready_{false};
    bool mass_battle_pose_atlas_failed_{false};
    struct MassBattlePresentationBatch final {
        std::vector<render::RenderInstance> instances;
#if GENOMES_HAS_INFANTRY
        std::vector<gameplay::InfantryMassBattleRenderState> states;
#else
        std::vector<std::byte> states;
#endif
        std::uint64_t tick{0};
    };
    std::unique_ptr<jobs::JobGroup> mass_battle_presentation_group_;
    std::shared_ptr<MassBattlePresentationBatch> pending_mass_battle_presentation_;
    std::shared_ptr<const MassBattlePresentationBatch> ready_mass_battle_presentation_;
    std::size_t mass_battle_visible_units_{0U};
    std::size_t mass_battle_active_pose_slots_{0U};
    std::array<std::size_t, 8U> mass_battle_archetype_counts_{};
    std::array<std::size_t, 4U> mass_battle_lod_counts_{};
    std::size_t mass_battle_evaluated_poses_{0U};
    std::shared_ptr<const render::SkinnedMeshPrototype> infantry_skinned_prototype_;
    camera::CameraRequest camera_request_{};
    combat::TacticalAIProfile tactical_ai_profile_{};
#if GENOMES_HAS_INFANTRY
    std::unique_ptr<gameplay::BattlefieldRuntime> battlefield_runtime_;
    std::unique_ptr<gameplay::InfantryMassBattleRuntime> mass_battle_runtime_;
    infantry::InfantryModelCompiler infantry_model_compiler_;
    proc::GeneratorRegistry procedural_registry_;
    std::unique_ptr<proc::ProceduralRuntime> procedural_runtime_;
    std::shared_ptr<const infantry::InfantryModelArtifact> infantry_model_artifact_;
    struct InfantryAnimationAgent final {
        simulation::EntityId entity{};
        std::optional<infantry::LocomotionController> locomotion;
        std::optional<infantry::LocomotionState> locomotion_state;
        infantry::AnimationTransitionRuntime transition_runtime{};
        std::optional<infantry::FaceAnimator> face;
        infantry::GroundContactRuntime ground_runtime{};
        std::optional<infantry::AnimationWeaponOverlay> weapon_overlay;
        float weapon_readiness{0.0F};
        foundation::Vec3 previous_position{};
        float previous_heading{0.0F};
        bool has_previous_motion{false};
        infantry::AnimationLODState lod{};
    };
    std::optional<infantry::PresentationAnimation> animation_system_;
    std::vector<InfantryAnimationAgent> animation_agents_;
    std::vector<simulation::EntityId> live_animation_entities_;
    std::vector<infantry::AnimationPose> animation_poses_;
    render::PoseSnapshotExchange pose_exchange_;
    infantry::AnimationEvaluationHandle animation_job_;
    // Presentation may continue while pose evaluation is in flight. After a
    // bounded number of frames without a newer immutable pose, use the
    // compatible atlas (when ready) or bind pose rather than replaying stale
    // animation indefinitely.
    std::uint32_t pose_stall_frames_{0U};
    std::uint64_t last_pose_tick_{0U};
    std::uint64_t last_pose_revision_{0U};
    std::optional<simulation::TickContext> pending_animation_context_;
    std::shared_ptr<std::optional<foundation::Error>> pending_animation_error_;
    MassBattleLoadStage mass_battle_load_stage_{MassBattleLoadStage::Inactive};
#endif
    BattlefieldSceneMode mode_{BattlefieldSceneMode::Tactical};
    std::uint64_t scene_epoch_{0U};
    bool simulation_failed_{false};
    std::unique_ptr<world::WorldRegionStreamer> region_streamer_;
    jobs::JobSystem* jobs_{nullptr};
    jobs::CancelToken cancellation_{};
    std::string generation_error_;
    float terrain_min_height_{0.0F};
    float terrain_max_height_{0.0F};
};

} // namespace genomes::game_scenes
