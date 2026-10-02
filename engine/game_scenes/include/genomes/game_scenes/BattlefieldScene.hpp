#pragma once

#include <genomes/combat/TacticalAI.hpp>
#include <genomes/buildings/BuildingModel.hpp>
#include <genomes/buildings/BuildingProfile.hpp>
#if GENOMES_HAS_INFANTRY
#include <genomes/gameplay/BattlefieldRuntime.hpp>
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

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace genomes::game_scenes {

using runtime::Scene;
using runtime::SceneContext;

class BattlefieldScene final : public Scene {
public:
    BattlefieldScene(application::WorldGenerationConfig config,
                     std::shared_ptr<const buildings::FrozenBuildingProfile> building_profile,
                     combat::TacticalAIProfile tactical_ai_profile = {})
        : config_{std::move(config)}, building_profile_{std::move(building_profile)},
          tactical_ai_profile_{tactical_ai_profile} {}

    [[nodiscard]] foundation::SceneId id() const noexcept override;
    void on_enter(SceneContext&) override;
    void on_exit(SceneContext&) override;
    void handle_input(SceneContext&, const input::InputFrame&) override;
    void fixed_update(SceneContext&, const simulation::TickContext&) override;
    void frame_update(SceneContext&, double) override;
    void build_presentation(SceneContext&) override;

    [[nodiscard]] const world::WorldPlan* plan() const noexcept {
        return plan_ ? &*plan_ : nullptr;
    }
    [[nodiscard]] bool simulationFailed() const noexcept { return simulation_failed_; }

private:
    void finalize_plan(world::WorldPlan plan);
#if GENOMES_HAS_INFANTRY
    void initialize_infantry_animation();
    void evaluate_infantry_animation(const simulation::TickContext& context);
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
    std::shared_ptr<const render::SkinnedMeshPrototype> infantry_skinned_prototype_;
    camera::CameraRequest camera_request_{};
    combat::TacticalAIProfile tactical_ai_profile_{};
#if GENOMES_HAS_INFANTRY
    std::unique_ptr<gameplay::BattlefieldRuntime> battlefield_runtime_;
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
#endif
    bool simulation_failed_{false};
    std::unique_ptr<world::WorldRegionStreamer> region_streamer_;
    jobs::JobSystem* jobs_{nullptr};
    std::string generation_error_;
    float terrain_min_height_{0.0F};
    float terrain_max_height_{0.0F};
};

} // namespace genomes::game_scenes
