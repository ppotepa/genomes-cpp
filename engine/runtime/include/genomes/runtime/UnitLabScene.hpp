#pragma once

#include <genomes/render/RenderTypes.hpp>
#include <genomes/infantry/InfantryModelCompiler.hpp>
#include <genomes/infantry/LocomotionController.hpp>
#include <genomes/infantry/FaceAnimation.hpp>
#include <genomes/infantry/AnimationSystem.hpp>
#include <genomes/runtime/Scene.hpp>
#include <genomes/jobs/JobHandle.hpp>

#include <memory>
#include <optional>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace genomes::runtime {

enum class UnitLabCameraMode : std::uint8_t {
    ThreeQuarter,
    Front,
    Side,
    Back,
    Face,
    Hands,
};

enum class UnitLabDirtyFlag : std::uint8_t {
    Geometry,
    Material,
    Pose,
    Ui,
};

class UnitLabScene final : public Scene {
public:
    ~UnitLabScene() override;

    [[nodiscard]] foundation::SceneId id() const noexcept override;

    void on_enter(SceneContext&) override;
    void on_exit(SceneContext&) override;
    void handle_input(SceneContext&, const input::InputFrame&) override;
    ui::UiActionResult handle_ui_action(
        SceneContext&, ui::UiActionId, const ui::UiActionArguments&) override;
    void fixed_update(SceneContext&, double) override;
    void frame_update(SceneContext&, double) override;
    void build_presentation(SceneContext&) override;

private:
    void markDirty(UnitLabDirtyFlag flag) noexcept;
    bool activateControl(SceneContext&, std::uint8_t control);
    void rebuildModel(SceneContext* context = nullptr);
    void publishModelResult(foundation::Result<infantry::InfantryModelArtifact,
                                               foundation::Error>&& result);

    double elapsed_seconds_{0.0};
    std::uint64_t fixed_tick_{0};
    float fixed_accumulator_{0.0F};
    std::uint64_t preview_seed_{0x5EED2026ull};
    float variation_{1.0F};
    std::uint32_t detail_level_{2U};
    std::size_t loadout_index_{0U};
    std::uint8_t genome_override_mode_{0U};
    infantry::GenomeOverrides genome_overrides_{};
    UnitLabCameraMode camera_mode_{UnitLabCameraMode::ThreeQuarter};
    infantry::FaceExpression expression_{infantry::FaceExpression::Neutral};
    float expression_intensity_{0.0F};
    bool show_surface_{true};
    bool show_wireframe_{false};
    bool show_skeleton_{false};
    bool show_bounds_{false};
    bool show_normals_{false};
    std::optional<infantry::BoneId> debug_weight_bone_;
    bool animation_paused_{false};
    bool geometry_dirty_{true};
    bool material_dirty_{true};
    bool pose_dirty_{true};
    bool ui_dirty_{true};
    std::shared_ptr<const render::RenderMesh> unit_prototype_;
    std::shared_ptr<const render::SkinnedMeshPrototype> skinned_prototype_;
    foundation::StableId skinned_prototype_model_key_{0};
    infantry::InfantryModelCompiler model_compiler_;
    std::optional<infantry::InfantryModelArtifact> model_artifact_;
    std::optional<infantry::LocomotionController> locomotion_;
    std::optional<infantry::LocomotionState> locomotion_state_;
    std::optional<infantry::FaceAnimator> face_animator_;
    std::optional<infantry::AnimationSystem> animation_system_;
    std::optional<infantry::AnimationPose> animation_pose_;
    std::optional<foundation::Error> last_generation_error_;
    struct PendingModelResult final {
        mutable std::mutex mutex;
        std::optional<infantry::InfantryModelCompiler::CompileRevision> revision;
        std::optional<foundation::Result<infantry::InfantryModelArtifact, foundation::Error>> result;
    };
    std::shared_ptr<PendingModelResult> pending_model_result_;
    jobs::JobHandle model_job_;
    infantry::InfantryModelCompiler::CompileRevision model_revision_{0};
};

} // namespace genomes::runtime
