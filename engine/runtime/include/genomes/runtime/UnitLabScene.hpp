#pragma once

#include <genomes/render/RenderTypes.hpp>
#include <genomes/infantry/InfantryModelCompiler.hpp>
#include <genomes/infantry/LocomotionController.hpp>
#include <genomes/infantry/FaceAnimation.hpp>
#include <genomes/infantry/AnimationSystem.hpp>
#include <genomes/runtime/Scene.hpp>
#include <genomes/runtime/UnitLabModelRequestGate.hpp>
#include <genomes/jobs/JobHandle.hpp>

#include <memory>
#include <optional>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <variant>

namespace genomes::runtime {

struct UnitLabViewport final {
    float left;
    float top;
    float width;
    float height;
    float projection_offset_x;
    float projection_offset_y;
};
[[nodiscard]] UnitLabViewport unitLabViewport(int framebuffer_width, int framebuffer_height,
                                               double ui_scale) noexcept;

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
    Presentation,
    Ui,
};

struct SetVariation final {
    float value{1.0F};
};

struct SetCameraMode final {
    UnitLabCameraMode value{UnitLabCameraMode::ThreeQuarter};
};

struct SetLocomotionPreset final {
    infantry::BipedPreset value{infantry::BipedPreset::Idle};
};

struct SetExpression final {
    infantry::FaceExpression value{infantry::FaceExpression::Neutral};
};

struct SetEquipmentSlot final {
    infantry::EquipmentSlot slot{infantry::EquipmentSlot::Head};
    infantry::EquipmentOverride value{};
};

struct SetGeneOverride final {
    infantry::GenomeGene gene{infantry::GenomeGene::Height};
    double value{0.5};
};

using UnitLabCommand = std::variant<SetVariation, SetCameraMode,
                                    SetLocomotionPreset, SetExpression,
                                    SetEquipmentSlot, SetGeneOverride>;

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
    enum class Control : std::uint8_t {
        Regenerate, Detail, CycleCamera, ToggleSurface, ToggleWireframe,
        ToggleSkeleton, ToggleBounds, ToggleNormals, TogglePause, CycleExpression,
        CycleWeightBone, CycleVariation, CycleLoadout, CycleGenomePreset, ReturnToMenu,
        CycleLocomotion, CycleExpressionIntensity, NextGenomeGene, DecreaseGenomeGene,
        IncreaseGenomeGene, ClearGenomeGene, ClearAllGenomeGenes, CycleEquipmentSlot,
        CycleEquipmentItem, ClearEquipment,
    };

    void markDirty(UnitLabDirtyFlag flag) noexcept;
    bool applyCommand(SceneContext&, SetVariation);
    bool applyCommand(SceneContext&, SetCameraMode);
    bool applyCommand(SceneContext&, SetLocomotionPreset);
    bool applyCommand(SceneContext&, SetExpression);
    bool applyCommand(SceneContext&, SetEquipmentSlot);
    bool applyCommand(SceneContext&, SetGeneOverride);
    bool applyCommand(SceneContext&, const UnitLabCommand&);
    bool executeControl(SceneContext&, Control);
    void rebuildModel(SceneContext* context = nullptr);
    void startModelRequest(SceneContext&, infantry::InfantryModelRequest,
                           UnitLabModelRequestToken);
    void publishModelResult(foundation::Result<infantry::InfantryModelCompileResult,
                                               foundation::Error>&& result);

    double elapsed_seconds_{0.0};
    std::uint64_t fixed_tick_{0};
    float fixed_accumulator_{0.0F};
    std::uint64_t preview_seed_{0x5EED2026ull};
    float variation_{1.0F};
    std::uint32_t detail_level_{2U};
    std::size_t loadout_index_{0U};
    std::size_t palette_index_{0U};
    float equipment_wear_{0.0F};
    infantry::InfantrySide side_{infantry::InfantrySide::SideA};
    std::uint8_t active_tab_{0U};
    std::uint8_t genome_override_mode_{0U};
    infantry::GenomeOverrides genome_overrides_{};
    infantry::GenomeGene selected_genome_gene_{infantry::GenomeGene::Height};
    std::size_t selected_equipment_slot_{0U};
    infantry::EquipmentOverrideSet equipment_overrides_{};
    UnitLabCameraMode camera_mode_{UnitLabCameraMode::ThreeQuarter};
    infantry::FaceExpression expression_{infantry::FaceExpression::Neutral};
    float expression_intensity_{0.0F};
    bool show_surface_{true};
    bool show_wireframe_{false};
    bool show_skeleton_{false};
    bool show_bounds_{false};
    bool show_normals_{false};
    bool auto_rotate_{true};
    std::optional<infantry::BoneId> debug_weight_bone_;
    bool animation_paused_{false};
    float animation_speed_{1.0F};
    bool geometry_dirty_{true};
    bool material_dirty_{true};
    bool pose_dirty_{true};
    bool presentation_dirty_{true};
    bool ui_dirty_{true};
    std::shared_ptr<const render::RenderMesh> unit_prototype_;
    std::shared_ptr<const render::SkinnedMeshPrototype> skinned_prototype_;
    std::optional<ui::UiViewportMetrics> last_ui_viewport_metrics_;
    foundation::StableId skinned_prototype_model_key_{0};
    infantry::InfantryModelCompiler model_compiler_;
    std::shared_ptr<const infantry::InfantryModelArtifact> model_artifact_;
    std::optional<infantry::LocomotionController> locomotion_;
    std::optional<infantry::LocomotionState> locomotion_state_;
    std::optional<infantry::FaceAnimator> face_animator_;
    std::optional<infantry::AnimationSystem> animation_system_;
    std::optional<infantry::AnimationPose> animation_pose_;
    std::optional<foundation::Error> last_generation_error_;
    struct PendingModelResult final {
        mutable std::mutex mutex;
        std::optional<std::uint64_t> revision;
        std::optional<foundation::StableId> request_key;
        std::optional<foundation::Result<infantry::InfantryModelCompileResult,
                                         foundation::Error>> result;
    };
    std::shared_ptr<PendingModelResult> pending_model_result_;
    jobs::JobHandle model_job_;
    UnitLabModelRequestGate model_request_gate_;
    std::optional<infantry::InfantryModelRequest> queued_model_request_;
};

} // namespace genomes::runtime
