#pragma once

#include <genomes/render/RenderTypes.hpp>
#include <genomes/infantry/InfantryModelCompiler.hpp>
#include <genomes/infantry/AppearanceCatalog.hpp>
#include <genomes/infantry/LocomotionController.hpp>
#include <genomes/infantry/FaceAnimation.hpp>
#include <genomes/infantry/PresentationAnimation.hpp>
#include <genomes/proc/ProceduralRuntime.hpp>
#include <genomes/runtime/Scene.hpp>

#include <memory>
#include <optional>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <variant>

namespace genomes::game_scenes {

using runtime::Scene;
using runtime::SceneContext;

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

// One owner for all Unit Lab invalidation categories. Keeping the mask typed
// prevents a new output from silently acquiring an unrelated rebuild path.
class UnitLabDirtyState final {
public:
    UnitLabDirtyState() noexcept = default;

    void mark(UnitLabDirtyFlag flag) noexcept {
        mask_ |= bit(flag);
    }
    void markAll() noexcept { mask_ = kAll; }
    void clear(UnitLabDirtyFlag flag) noexcept {
        mask_ &= static_cast<std::uint8_t>(~bit(flag));
    }
    void clearAll() noexcept { mask_ = 0U; }
    [[nodiscard]] bool contains(UnitLabDirtyFlag flag) const noexcept {
        return (mask_ & bit(flag)) != 0U;
    }

private:
    [[nodiscard]] static constexpr std::uint8_t bit(UnitLabDirtyFlag flag) noexcept {
        return static_cast<std::uint8_t>(1U << static_cast<std::uint8_t>(flag));
    }

    // Keep the aggregate mask a constant expression on both MSVC and Clang;
    // the five enum values are deliberately contiguous and occupy bits 0..4.
    static constexpr std::uint8_t kAll = 0x1FU;
    std::uint8_t mask_{kAll};
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

struct SetAnimationState final {
    infantry::AnimationState value{infantry::AnimationState::IDLE};
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

inline constexpr foundation::StableId kInspectionOliveAppearancePreset =
    infantry::kInspectionOliveAppearancePreset;
inline constexpr std::uint32_t kAppearancePresetSchemaVersion =
    infantry::kAppearancePresetSchemaVersion;

struct SetAppearancePreset final {
    foundation::StableId value{0};
};

using UnitLabCommand = std::variant<SetVariation, SetCameraMode,
                                    SetLocomotionPreset, SetAnimationState, SetExpression,
                                    SetEquipmentSlot, SetGeneOverride,
                                    SetAppearancePreset>;

class UnitLabScene final : public Scene {
public:
    explicit UnitLabScene(
        std::shared_ptr<const infantry::FrozenAppearanceCatalog> appearance_catalog = {})
        : appearance_catalog_{std::move(appearance_catalog)} {}
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
    bool applyCommand(SceneContext&, SetAnimationState);
    bool applyCommand(SceneContext&, SetExpression);
    bool applyCommand(SceneContext&, SetEquipmentSlot);
    bool applyCommand(SceneContext&, SetGeneOverride);
    bool applyCommand(SceneContext&, SetAppearancePreset);
    bool applyCommand(SceneContext&, const UnitLabCommand&);
    bool executeControl(SceneContext&, Control);
    void rebuildModel(SceneContext* context = nullptr);
    void startModelRequest(SceneContext&, infantry::InfantryModelRequest);
    void seekAnimation(float phase) noexcept;
    void publishModelResult(foundation::Result<infantry::InfantryModelCompileResult,
                                               foundation::Error>&& result);
    [[nodiscard]] foundation::Result<infantry::InfantryModelCompileResult, foundation::Error>
    compileModel(const infantry::InfantryModelRequest& request);

    double elapsed_seconds_{0.0};
    std::uint64_t fixed_tick_{0};
    float fixed_accumulator_{0.0F};
    std::uint64_t preview_seed_{0x5EED2026ull};
    float variation_{1.0F};
    std::uint32_t detail_level_{2U};
    std::size_t loadout_index_{0U};
    std::size_t palette_index_{0U};
    foundation::StableId appearance_preset_{0};
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
    float locomotion_crouch_{0.0F};
    float locomotion_speed_mps_{0.0F};
    float animation_transition_seconds_{0.20F};
    UnitLabDirtyState dirty_{};
    std::shared_ptr<const render::RenderMesh> unit_prototype_;
    std::shared_ptr<const render::SkinnedMeshPrototype> skinned_prototype_;
    std::optional<ui::UiViewportMetrics> last_ui_viewport_metrics_;
    foundation::StableId skinned_prototype_model_key_{0};
    infantry::InfantryModelCompiler model_compiler_;
    proc::ProceduralRuntime* shared_procedural_runtime_{nullptr};
    std::shared_ptr<const infantry::InfantryModelArtifact> model_artifact_;
    std::shared_ptr<const infantry::FrozenAppearanceCatalog> appearance_catalog_;
    std::optional<infantry::LocomotionController> locomotion_;
    std::optional<infantry::LocomotionState> locomotion_state_;
    infantry::AnimationTransitionRuntime transition_runtime_{};
    std::optional<infantry::FaceAnimator> face_animator_;
    std::optional<infantry::PresentationAnimation> animation_system_;
    std::optional<infantry::AnimationPose> animation_pose_;
    std::optional<foundation::Error> last_generation_error_;
    proc::GenerationChannel model_channel_;
    proc::GenerationTicket<infantry::InfantryModelCompileResult> model_ticket_;
};

} // namespace genomes::game_scenes
