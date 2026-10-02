#pragma once

#include <genomes/combat/TacticalAI.hpp>
#include <genomes/buildings/BuildingModel.hpp>
#include <genomes/buildings/BuildingProfile.hpp>
#if GENOMES_HAS_INFANTRY
#include <genomes/gameplay/BattlefieldRuntime.hpp>
#include <genomes/gameplay/InfantryMassBattleRuntime.hpp>
#endif
#include <genomes/gameplay/WorldScenario.hpp>
#if GENOMES_HAS_INFANTRY
#include <genomes/infantry/InfantryModelCompiler.hpp>
#include <genomes/infantry/AnimationSystem.hpp>
#include <genomes/infantry/FaceAnimation.hpp>
#include <genomes/infantry/LocomotionController.hpp>
#endif
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/render/RenderTypes.hpp>
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
};

[[nodiscard]] constexpr MassBattleAnimationArchetype
massBattleAnimationArchetype(std::uint8_t variant) noexcept {
    switch (variant % 5U) {
    case 0U: return MassBattleAnimationArchetype::Idle;
    case 1U: return MassBattleAnimationArchetype::Walk;
    case 2U: return MassBattleAnimationArchetype::Run;
    case 3U: return MassBattleAnimationArchetype::Crouch;
    default: return MassBattleAnimationArchetype::CrouchWalk;
    }
}

class BattlefieldScene final : public Scene {
public:
    BattlefieldScene(application::WorldGenerationConfig config,
                     std::shared_ptr<const buildings::FrozenBuildingProfile> building_profile,
                     combat::TacticalAIProfile tactical_ai_profile = {},
                     BattlefieldSceneMode mode = BattlefieldSceneMode::Tactical)
        : config_{std::move(config)}, building_profile_{std::move(building_profile)},
          tactical_ai_profile_{tactical_ai_profile}, mode_{mode} {}

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
    static constexpr std::size_t MassBattlePoseVariantCount = 5U;
    static constexpr std::size_t MassBattlePoseAtlasSize = 34U;
    std::array<std::shared_ptr<render::RenderMesh>, MassBattlePoseAtlasSize>
        mass_battle_pose_meshes_{};
    bool mass_battle_pose_atlas_ready_{false};
    std::size_t mass_battle_visible_units_{0U};
    std::size_t mass_battle_active_pose_slots_{0U};
    std::array<std::size_t, 5U> mass_battle_archetype_counts_{};
    std::array<std::size_t, 4U> mass_battle_lod_counts_{};
    std::size_t mass_battle_evaluated_poses_{0U};
    std::shared_ptr<const render::SkinnedMeshPrototype> infantry_skinned_prototype_;
    camera::CameraRequest camera_request_{};
    combat::TacticalAIProfile tactical_ai_profile_{};
#if GENOMES_HAS_INFANTRY
    std::unique_ptr<gameplay::BattlefieldRuntime> battlefield_runtime_;
    std::unique_ptr<gameplay::InfantryMassBattleRuntime> mass_battle_runtime_;
    infantry::InfantryModelCompiler infantry_model_compiler_;
    std::shared_ptr<const infantry::InfantryModelArtifact> infantry_model_artifact_;
    struct InfantryAnimationAgent final {
        simulation::EntityId entity{};
        std::optional<infantry::LocomotionController> locomotion;
        std::optional<infantry::LocomotionState> locomotion_state;
        infantry::AnimationTransitionRuntime transition_runtime{};
        std::optional<infantry::FaceAnimator> face;
        infantry::GroundContactRuntime ground_runtime{};
        std::optional<infantry::AnimationWeaponOverlay> weapon_overlay;
        foundation::Vec3 previous_position{};
        float previous_heading{0.0F};
        bool has_previous_motion{false};
        infantry::AnimationLODState lod{};
    };
    std::optional<infantry::AnimationSystem> animation_system_;
    std::vector<InfantryAnimationAgent> animation_agents_;
    std::vector<infantry::AnimationPose> animation_poses_;
    MassBattleLoadStage mass_battle_load_stage_{MassBattleLoadStage::Inactive};
#endif
    BattlefieldSceneMode mode_{BattlefieldSceneMode::Tactical};
    bool simulation_failed_{false};
    std::unique_ptr<world::WorldRegionStreamer> region_streamer_;
    jobs::JobSystem* jobs_{nullptr};
    std::string generation_error_;
    float terrain_min_height_{0.0F};
    float terrain_max_height_{0.0F};
};

} // namespace genomes::game_scenes
