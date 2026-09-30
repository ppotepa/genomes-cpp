#include <genomes/runtime/UnitLabScene.hpp>
#include <genomes/runtime/InfantryPresentation.hpp>

#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/infantry/EquipmentCatalog.hpp>
#include <genomes/render/ProceduralMeshes.hpp>
#include <genomes/render/SkinnedDeformer.hpp>

#include <cmath>
#include <algorithm>
#include <array>
#include <string>
#include <span>
#include <iterator>
#include <vector>

namespace genomes::runtime {

namespace {

using Vec3 = foundation::Vec3;

[[nodiscard]] Vec3 add(Vec3 a, Vec3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] Vec3 multiply(Vec3 value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] Vec3 componentMultiply(Vec3 a, Vec3 b) noexcept {
    return {a.x * b.x, a.y * b.y, a.z * b.z};
}

[[nodiscard]] infantry::RigQuaternion multiply(infantry::RigQuaternion left,
                                                infantry::RigQuaternion right) noexcept {
    return {
        left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
        left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z};
}

[[nodiscard]] Vec3 rotate(infantry::RigQuaternion rotation, Vec3 value) noexcept {
    const infantry::RigQuaternion vector{value.x, value.y, value.z, 0.0F};
    const infantry::RigQuaternion inverse{-rotation.x, -rotation.y, -rotation.z, rotation.w};
    const infantry::RigQuaternion result = multiply(multiply(rotation, vector), inverse);
    return {result.x, result.y, result.z};
}

struct DebugBoneTransform final {
    Vec3 position{};
    infantry::RigQuaternion rotation{};
    Vec3 scale{1.0F, 1.0F, 1.0F};
};

[[nodiscard]] std::array<DebugBoneTransform, infantry::kRigBoneCount> debugPose(
    const infantry::SkeletonData& skeleton,
    std::span<const infantry::RigTransform> pose_bones,
    Vec3 root_position) noexcept {
    std::array<DebugBoneTransform, infantry::kRigBoneCount> result{};
    const auto bones = skeleton.bones();
    for (std::size_t index = 0U; index < bones.size() && index < result.size(); ++index) {
        const auto& record = bones[index];
        const auto& local = pose_bones.empty() ? record.local_bind : pose_bones[index];
        if (record.parent == infantry::kInvalidBoneIndex) {
            result[index] = {add(root_position, local.translation), local.rotation, local.scale};
            continue;
        }
        const auto& parent = result[record.parent];
        result[index].position = add(parent.position,
                                     rotate(parent.rotation,
                                            componentMultiply(local.translation, parent.scale)));
        result[index].rotation = multiply(parent.rotation, local.rotation);
        result[index].scale = componentMultiply(parent.scale, local.scale);
    }
    return result;
}

[[nodiscard]] Vec3 rotateY(Vec3 value, float angle) noexcept {
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    return {value.x * cosine - value.z * sine, value.y,
            value.x * sine + value.z * cosine};
}

} // namespace

UnitLabScene::~UnitLabScene() {
    if (model_job_.valid()) {
        model_job_.wait();
    }
}


foundation::SceneId UnitLabScene::id() const noexcept {
    return foundation::scene_id("scene.unit-lab");
}

void UnitLabScene::markDirty(UnitLabDirtyFlag flag) noexcept {
    switch (flag) {
    case UnitLabDirtyFlag::Geometry: geometry_dirty_ = true; break;
    case UnitLabDirtyFlag::Material: material_dirty_ = true; break;
    case UnitLabDirtyFlag::Pose: pose_dirty_ = true; break;
    case UnitLabDirtyFlag::Ui: ui_dirty_ = true; break;
    }
}

bool UnitLabScene::activateControl(SceneContext& context, std::uint8_t control) {
    switch (control) {
    case 0: ++preview_seed_; rebuildModel(&context); break;
    case 1: detail_level_ = detail_level_ >= 3U ? 1U : detail_level_ + 1U; rebuildModel(&context); break;
    case 2:
        camera_mode_ = static_cast<UnitLabCameraMode>((static_cast<std::uint8_t>(camera_mode_) + 1U) % 6U);
        markDirty(UnitLabDirtyFlag::Ui); break;
    case 3: show_surface_ = !show_surface_; markDirty(UnitLabDirtyFlag::Ui); break;
    case 4: show_wireframe_ = !show_wireframe_; markDirty(UnitLabDirtyFlag::Ui); break;
    case 5: show_skeleton_ = !show_skeleton_; markDirty(UnitLabDirtyFlag::Ui); break;
    case 6: show_bounds_ = !show_bounds_; markDirty(UnitLabDirtyFlag::Ui); break;
    case 7: show_normals_ = !show_normals_; markDirty(UnitLabDirtyFlag::Ui); break;
    case 8: animation_paused_ = !animation_paused_; markDirty(UnitLabDirtyFlag::Pose); break;
    case 9:
        expression_ = static_cast<infantry::FaceExpression>(
            (static_cast<std::uint8_t>(expression_) + 1U) % infantry::kFaceExpressionCount);
        expression_intensity_ = expression_ == infantry::FaceExpression::Neutral ? 0.0F : 1.0F;
        if (face_animator_) (void)face_animator_->setExpression(expression_, expression_intensity_);
        markDirty(UnitLabDirtyFlag::Pose); break;
    case 10:
        debug_weight_bone_ = debug_weight_bone_
            ? static_cast<infantry::BoneId>((static_cast<std::uint16_t>(*debug_weight_bone_) + 1U) % infantry::kRigBoneCount)
            : infantry::BoneId::Hips;
        markDirty(UnitLabDirtyFlag::Ui); break;
    case 11: variation_ = variation_ < 1.0F ? 1.0F : variation_ < 1.5F ? 2.0F : 0.5F; rebuildModel(&context); break;
    case 12:
        if (!infantry::infantryLoadouts().empty()) {
            loadout_index_ = (loadout_index_ + 1U) % infantry::infantryLoadouts().size();
            rebuildModel(&context);
        }
        break;
    case 13:
        genome_override_mode_ = static_cast<std::uint8_t>((genome_override_mode_ + 1U) % 4U);
        genome_overrides_ = {};
        if (genome_override_mode_ == 1U) {
            (void)genome_overrides_.set(infantry::GenomeGene::Height, (1.65 - 1.60) / 0.35);
        } else if (genome_override_mode_ == 2U) {
            (void)genome_overrides_.set(infantry::GenomeGene::Height, (1.90 - 1.60) / 0.35);
        } else if (genome_override_mode_ == 3U) {
            (void)genome_overrides_.set(infantry::GenomeGene::BodyShoulderBreadth, 1.0);
            (void)genome_overrides_.set(infantry::GenomeGene::BodyHipBreadth, 0.0);
        }
        rebuildModel(&context); break;
    case 14: context.commands.push({ApplicationCommandKind::ReturnToMainMenu}); break;
    case 15:
        if (locomotion_ && locomotion_state_) {
            const auto p = locomotion_state_->preset;
            const auto next = p == infantry::BipedPreset::Idle ? infantry::BipedPreset::Walk
                : p == infantry::BipedPreset::Walk ? infantry::BipedPreset::Run
                : p == infantry::BipedPreset::Run ? infantry::BipedPreset::Crouch
                : infantry::BipedPreset::Idle;
            (void)locomotion_->setPreset(*locomotion_state_, next);
            markDirty(UnitLabDirtyFlag::Pose);
        }
        break;
    case 16:
        if (expression_ != infantry::FaceExpression::Neutral) {
            expression_intensity_ += 0.25F;
            if (expression_intensity_ > 1.001F) expression_intensity_ = 0.25F;
            if (face_animator_) (void)face_animator_->setExpression(expression_, expression_intensity_);
            markDirty(UnitLabDirtyFlag::Pose);
        }
        break;
    case 17: {
        const auto next = (static_cast<std::size_t>(selected_genome_gene_) + 1U) %
                          infantry::GenomeGeneCount;
        selected_genome_gene_ = static_cast<infantry::GenomeGene>(next);
        markDirty(UnitLabDirtyFlag::Ui);
        break;
    }
    case 18:
    case 19: {
        double value = 0.5;
        if (const auto override = genome_overrides_.get(selected_genome_gene_); override) {
            value = *override;
        } else if (model_artifact_) {
            value = model_artifact_->genome.geneValue(selected_genome_gene_);
        }
        value = std::clamp(value + (control == 18 ? -0.10 : 0.10), 0.0, 1.0);
        (void)genome_overrides_.set(selected_genome_gene_, value);
        genome_override_mode_ = 0U;
        rebuildModel(&context);
        break;
    }
    case 20: {
        const auto index = static_cast<std::size_t>(selected_genome_gene_);
        if (index < genome_overrides_.genes.size()) genome_overrides_.genes[index].reset();
        genome_override_mode_ = 0U;
        rebuildModel(&context);
        break;
    }
    case 21:
        genome_overrides_ = {};
        genome_override_mode_ = 0U;
        rebuildModel(&context);
        break;
    case 22: {
        const auto slots = infantry::EquipmentCatalog::slots();
        if (!slots.empty()) selected_equipment_slot_ = (selected_equipment_slot_ + 1U) % slots.size();
        markDirty(UnitLabDirtyFlag::Ui);
        break;
    }
    case 23: {
        const auto slots = infantry::EquipmentCatalog::slots();
        const auto items = infantry::EquipmentCatalog::items();
        if (slots.empty()) break;
        const auto slot = slots[selected_equipment_slot_ % slots.size()].slot;
        const auto slot_index = infantry::equipmentSlotIndex(slot);
        std::vector<foundation::StableId> allowed;
        allowed.reserve(items.size());
        for (const auto& item : items) if (item.allows(slot)) allowed.push_back(item.id);
        auto& current = equipment_overrides_.slots[slot_index];
        if (!current.specified) {
            current = infantry::EquipmentOverride::nullValue();
        } else if (current.empty) {
            current = allowed.empty() ? infantry::EquipmentOverride::absent()
                                      : infantry::EquipmentOverride::item(allowed.front());
        } else {
            const auto found = std::find(allowed.begin(), allowed.end(), current.definition_id);
            if (found == allowed.end() || std::next(found) == allowed.end())
                current = infantry::EquipmentOverride::absent();
            else
                current = infantry::EquipmentOverride::item(*std::next(found));
        }
        rebuildModel(&context);
        break;
    }
    case 24:
        equipment_overrides_ = {};
        rebuildModel(&context);
        break;
    default: return false;
    }
    return true;
}

void UnitLabScene::publishModelResult(
    foundation::Result<infantry::InfantryModelArtifact, foundation::Error>&& compiled) {
    if (!compiled) {
        last_generation_error_ = compiled.error();
        markDirty(UnitLabDirtyFlag::Ui);
        return;
    }
    model_artifact_ = std::move(compiled.value());
    skinned_prototype_.reset();
    skinned_prototype_model_key_ = 0;
    locomotion_.reset();
    locomotion_state_.reset();
    face_animator_.reset();
    animation_system_.reset();
    animation_pose_.reset();
    if (auto locomotion = infantry::LocomotionController::create(
            model_artifact_->phenotype.body); locomotion) {
        locomotion_ = std::move(locomotion.value());
        locomotion_state_ = locomotion_->initialState();
    }
    if (auto face = infantry::FaceAnimator::create(
            preview_seed_, model_artifact_->phenotype.face); face) {
        face_animator_ = std::move(face.value());
    }
    if (auto animation = infantry::AnimationSystem::create(1U); animation) {
        animation_system_ = std::move(animation.value());
    }
    last_generation_error_.reset();
    markDirty(UnitLabDirtyFlag::Geometry);
    markDirty(UnitLabDirtyFlag::Material);
    markDirty(UnitLabDirtyFlag::Pose);
    markDirty(UnitLabDirtyFlag::Ui);
}

void UnitLabScene::rebuildModel(SceneContext* context) {
    // A JobHandle is single-owner in the scene.  Before replacing it, finish
    // the previous task so its lambda cannot outlive this scene and so the
    // compiler's revision cancellation is observed deterministically.
    if (model_job_.valid()) {
        if (model_revision_ != 0U) {
            model_compiler_.cancelRevision(model_revision_);
        }
        model_job_.wait();
        model_job_ = {};
        pending_model_result_.reset();
    }
    infantry::InfantryModelRequest request{};
    request.seed = preview_seed_;
    request.variation = variation_;
    request.detail_level = static_cast<infantry::InfantryDetail>(detail_level_);
    request.genome_overrides = genome_overrides_;
    request.uniform_color = infantry::kDefaultUniformColor;
    const auto loadouts = infantry::infantryLoadouts();
    if (!loadouts.empty()) {
        request.loadout_id = loadouts[loadout_index_ % loadouts.size()].id;
    }
    request.equipment_overrides = equipment_overrides_;
    // Every rebuild owns a revision.  A queued/older job can therefore not
    // publish a result after the preview has changed underneath it.
    const auto revision = model_compiler_.beginRevision();
    model_revision_ = revision;
    if (context != nullptr && context->jobs != nullptr &&
        !context->deterministic_capture) {
        const auto pending = std::make_shared<PendingModelResult>();
        pending_model_result_ = pending;
        model_job_ = context->jobs->submit(
            [this, request, revision, pending](jobs::JobContext&) mutable {
                auto result = model_compiler_.compile(request, revision);
                {
                    std::lock_guard lock(pending->mutex);
                    pending->revision = revision;
                    pending->result = std::move(result);
                }
            });
        markDirty(UnitLabDirtyFlag::Ui);
        return;
    }
    publishModelResult(model_compiler_.compile(request, revision));
}

void UnitLabScene::on_enter(SceneContext& context) {
    elapsed_seconds_ = 0.0;
    fixed_tick_ = 0;
    fixed_accumulator_ = 0.0F;
    unit_prototype_.reset();
    model_artifact_.reset();
    locomotion_.reset();
    locomotion_state_.reset();
    face_animator_.reset();
    animation_system_.reset();
    animation_pose_.reset();
    last_generation_error_.reset();
    geometry_dirty_ = true;
    material_dirty_ = true;
    pose_dirty_ = true;
    ui_dirty_ = true;
    skinned_prototype_.reset();
    skinned_prototype_model_key_ = 0;
    rebuildModel(&context);
    geometry_dirty_ = false;
    material_dirty_ = false;
    context.ui.clear();
}

void UnitLabScene::on_exit(SceneContext&) {
    if (model_revision_ != 0U) {
        model_compiler_.cancelRevision(model_revision_);
    }
    if (model_job_.valid()) {
        model_job_.wait();
        model_job_ = {};
    }
    pending_model_result_.reset();
}

void UnitLabScene::handle_input(SceneContext& context, const input::InputFrame& input) {
    if (input.mouse_left_pressed && input.mouse_x >= 78.0F && input.mouse_x <= 500.0F) {
        constexpr float first_control_y = 266.0F;
        constexpr float control_step = 60.0F;
        const int control = static_cast<int>((input.mouse_y - first_control_y) / control_step);
        const float local_y = input.mouse_y -
                              (first_control_y + static_cast<float>(control) * control_step);
        if (control >= 0 && control <= 14 && local_y >= 0.0F && local_y <= 48.0F) {
            if (activateControl(context, static_cast<std::uint8_t>(control))) return;
        }
    }

    if (locomotion_ && locomotion_state_) {
        if (input.right_pressed) {
            const auto current = locomotion_state_->preset;
            const auto next = current == infantry::BipedPreset::Idle
                ? infantry::BipedPreset::Walk
                : current == infantry::BipedPreset::Walk
                    ? infantry::BipedPreset::Run
                    : current == infantry::BipedPreset::Run
                        ? infantry::BipedPreset::Crouch
                        : infantry::BipedPreset::Idle;
            (void)locomotion_->setPreset(*locomotion_state_, next);
        } else if (input.left_pressed) {
            const auto current = locomotion_state_->preset;
            const auto previous = current == infantry::BipedPreset::Idle
                ? infantry::BipedPreset::Crouch
                : current == infantry::BipedPreset::Crouch
                    ? infantry::BipedPreset::Run
                    : current == infantry::BipedPreset::Run
                        ? infantry::BipedPreset::Walk
                        : infantry::BipedPreset::Idle;
            (void)locomotion_->setPreset(*locomotion_state_, previous);
        }
        markDirty(UnitLabDirtyFlag::Pose);
    }
    if (input.cancel_pressed || input.confirm_pressed) {
        context.commands.push({ApplicationCommandKind::ReturnToMainMenu});
    }
}

ui::UiActionResult UnitLabScene::handle_ui_action(
    SceneContext& context, ui::UiActionId action, const ui::UiActionArguments&) {
    struct Binding { ui::UiActionId id; std::uint8_t control; };
    static constexpr Binding bindings[] = {
        {foundation::stable_id("unit.regenerate"),0U}, {foundation::stable_id("unit.detail"),1U},
        {foundation::stable_id("unit.camera"),2U}, {foundation::stable_id("unit.surface"),3U},
        {foundation::stable_id("unit.wireframe"),4U}, {foundation::stable_id("unit.skeleton"),5U},
        {foundation::stable_id("unit.bounds"),6U}, {foundation::stable_id("unit.normals"),7U},
        {foundation::stable_id("unit.pause"),8U}, {foundation::stable_id("unit.expression"),9U},
        {foundation::stable_id("unit.weight"),10U}, {foundation::stable_id("unit.variation"),11U},
        {foundation::stable_id("unit.loadout"),12U}, {foundation::stable_id("unit.genome-preset"),13U},
        {foundation::stable_id("unit.back"),14U}, {foundation::stable_id("unit.locomotion"),15U},
        {foundation::stable_id("unit.expression-intensity"),16U},
        {foundation::stable_id("unit.genome-next"),17U},
        {foundation::stable_id("unit.genome-minus"),18U},
        {foundation::stable_id("unit.genome-plus"),19U},
        {foundation::stable_id("unit.genome-clear"),20U},
        {foundation::stable_id("unit.genome-clear-all"),21U},
        {foundation::stable_id("unit.equipment-slot"),22U},
        {foundation::stable_id("unit.equipment-item"),23U},
        {foundation::stable_id("unit.equipment-clear"),24U}
    };
    for (const auto& binding : bindings) if (action == binding.id)
        return activateControl(context, binding.control)
            ? ui::UiActionResult::Handled : ui::UiActionResult::Rejected;
    return ui::UiActionResult::Unknown;
}

void UnitLabScene::fixed_update(SceneContext&, double dt) {
    constexpr float fixed_dt = 1.0F / 60.0F;
    fixed_accumulator_ += static_cast<float>(std::max(0.0, dt));
    while (fixed_accumulator_ >= fixed_dt) {
        fixed_accumulator_ -= fixed_dt;
        ++fixed_tick_;
        elapsed_seconds_ = static_cast<double>(fixed_tick_) * fixed_dt;
        if (!animation_paused_ && locomotion_ && locomotion_state_) {
            (void)locomotion_->step(*locomotion_state_, fixed_dt);
        }
        if (!animation_paused_ && animation_system_ && locomotion_ && locomotion_state_ && model_artifact_) {
            infantry::AnimationEntity entity{};
            entity.semantic_id = foundation::stable_id("unit-lab.infantry");
            entity.skeleton = &model_artifact_->skeleton;
            entity.surface = &model_artifact_->appearance.body;
            entity.locomotion = &*locomotion_;
            entity.locomotion_state = &*locomotion_state_;
            entity.face = face_animator_ ? &*face_animator_ : nullptr;
            (void)animation_system_->evaluate(
                std::span<infantry::AnimationEntity>(&entity, 1U), fixed_tick_, fixed_dt,
                nullptr);
            if (!animation_system_->currentSnapshot().poses.empty()) {
                animation_pose_ = animation_system_->currentSnapshot().poses.front();
            }
            markDirty(UnitLabDirtyFlag::Pose);
        }
    }
}

void UnitLabScene::frame_update(SceneContext& context, double) {
    if (model_job_.valid() && model_job_.isComplete()) {
        if (pending_model_result_) {
            std::optional<foundation::Result<infantry::InfantryModelArtifact,
                                              foundation::Error>> result;
            infantry::InfantryModelCompiler::CompileRevision revision = 0U;
            {
                std::lock_guard lock(pending_model_result_->mutex);
                if (pending_model_result_->revision && pending_model_result_->result) {
                    revision = *pending_model_result_->revision;
                    result = std::move(pending_model_result_->result);
                }
            }
            if (result && revision == model_revision_) {
                publishModelResult(std::move(*result));
            }
        }
        model_job_ = {};
        pending_model_result_.reset();
    }
    ui_dirty_ = false;
    context.ui.clear();
    context.ui.add({foundation::stable_id("unit-lab.panel"), ui::UiWidgetType::Panel,
                    "UNIT LAB", true, false, 520.0F, 1240.0F});
    context.ui.add({foundation::stable_id("unit-lab.title"), ui::UiWidgetType::Label,
                    "Procedural infantry prototypes", true, false, 0.0F, 0.0F});
    context.ui.add({foundation::stable_id("unit-lab.description"), ui::UiWidgetType::Label,
                    "Native procedural body, face, hair and equipment preview.", true,
                    false, 0.0F, 0.0F});
    std::string metrics = model_artifact_
        ? "MODEL READY | SEED " + std::to_string(preview_seed_) +
          " | RIFLEMAN | DETAIL " + std::to_string(detail_level_)
        : "MODEL COMPILATION FAILED";
    if (last_generation_error_) {
        metrics += " | PREVIOUS MODEL / ERROR " +
                   std::string(last_generation_error_->message);
    }
    if (model_artifact_) {
        const auto& appearance = model_artifact_->appearance;
        metrics += " | BONES " + std::to_string(model_artifact_->skeleton.bones().size());
        metrics += " | VERTICES " + std::to_string(appearance.body.vertices.size() +
                                                     appearance.hair.vertices.size());
        metrics += " | TRIANGLES " + std::to_string(
            (appearance.body.indices.size() + appearance.hair.indices.size()) / 3U);
        metrics += " | MORPHS " + std::to_string(appearance.morphs.size());
        metrics += " | CACHE H" + std::to_string(model_compiler_.cacheHits());
        metrics += "/M" + std::to_string(model_compiler_.cacheMisses());
        metrics += show_surface_ ? " | SURFACE" : " | SURFACE OFF";
        metrics += show_wireframe_ ? " | WIREFRAME" : "";
        metrics += show_skeleton_ ? " | SKELETON" : "";
        metrics += show_bounds_ ? " | BOUNDS" : "";
        metrics += show_normals_ ? " | NORMALS" : "";
        metrics += debug_weight_bone_
            ? " | WEIGHT " + std::to_string(static_cast<std::uint16_t>(*debug_weight_bone_))
            : "";
        metrics += " | CAMERA " + std::to_string(static_cast<int>(camera_mode_));
        metrics += " | VAR " + std::to_string(variation_);
        metrics += " | OVERRIDE " + std::to_string(genome_override_mode_);
        metrics += " | GENE " + std::string(infantry::genomeGeneName(selected_genome_gene_));
        const auto selected_override = genome_overrides_.get(selected_genome_gene_);
        metrics += selected_override ? "=" + std::to_string(*selected_override) : "=seed";
        const auto equipment_slots = infantry::EquipmentCatalog::slots();
        if (!equipment_slots.empty()) {
            const auto slot = equipment_slots[selected_equipment_slot_ % equipment_slots.size()];
            metrics += " | SLOT " + std::string(slot.identifier);
            const auto& selected_equipment =
                equipment_overrides_.slots[infantry::equipmentSlotIndex(slot.slot)];
            if (!selected_equipment.specified) {
                metrics += "=loadout";
            } else if (selected_equipment.empty) {
                metrics += "=empty";
            } else if (const auto* item =
                           infantry::EquipmentCatalog::findItem(selected_equipment.definition_id)) {
                metrics += "=" + std::string(item->identifier);
            }
        }
        if (!infantry::infantryLoadouts().empty()) {
            metrics += " | LOADOUT " + std::string(
                infantry::infantryLoadouts()[loadout_index_ % infantry::infantryLoadouts().size()].identifier);
        }
        metrics += " | EXPRESSION " + std::to_string(static_cast<int>(expression_)) +
                   "@" + std::to_string(expression_intensity_);
        metrics += " | GPU UPLOAD " + std::to_string(context.render_telemetry.mesh_uploads);
        metrics += " | PALETTE " + std::to_string(context.render_telemetry.palette_updates);
        metrics += " | DRAW " + std::to_string(context.render_telemetry.draw_calls);
        metrics += " | UI " + std::to_string(context.render_telemetry.ui_draw_calls);
    }
    context.ui.add({foundation::stable_id("unit-lab.metrics"), ui::UiWidgetType::Label,
                    std::move(metrics),
                    true, false, 0.0F, 0.0F});
    context.ui.add({foundation::stable_id("unit-lab.regenerate"), ui::UiWidgetType::Button,
                    "Regenerate seed", true, false, 420.0F, 48.0F});
    context.ui.add({foundation::stable_id("unit-lab.detail"), ui::UiWidgetType::Button,
                    "Cycle detail level", true, false, 420.0F, 48.0F});
    context.ui.add({foundation::stable_id("unit-lab.camera"), ui::UiWidgetType::Button,
                    "Cycle camera preset", true, false, 420.0F, 48.0F});
    context.ui.add({foundation::stable_id("unit-lab.surface"), ui::UiWidgetType::Button,
                    show_surface_ ? "Surface: ON" : "Surface: OFF", true, false, 420.0F, 48.0F});
    context.ui.add({foundation::stable_id("unit-lab.wireframe"), ui::UiWidgetType::Button,
                    show_wireframe_ ? "Wireframe: ON" : "Wireframe: OFF", true, false,
                    420.0F, 48.0F});
    context.ui.add({foundation::stable_id("unit-lab.skeleton"), ui::UiWidgetType::Button,
                    show_skeleton_ ? "Skeleton: ON" : "Skeleton: OFF", true, false,
                    420.0F, 48.0F});
    context.ui.add({foundation::stable_id("unit-lab.bounds"), ui::UiWidgetType::Button,
                    show_bounds_ ? "Bounds: ON" : "Bounds: OFF", true, false, 420.0F, 48.0F});
    context.ui.add({foundation::stable_id("unit-lab.normals"), ui::UiWidgetType::Button,
                    show_normals_ ? "Normals: ON" : "Normals: OFF", true, false,
                    420.0F, 48.0F});
    context.ui.add({foundation::stable_id("unit-lab.pause"), ui::UiWidgetType::Button,
                    animation_paused_ ? "Animation: PAUSED" : "Animation: PLAYING", true,
                    false, 420.0F, 48.0F});
    context.ui.add({foundation::stable_id("unit-lab.expression"), ui::UiWidgetType::Button,
                    "Cycle expression", true, false, 420.0F, 48.0F});
    context.ui.add({foundation::stable_id("unit-lab.weight"), ui::UiWidgetType::Button,
                    debug_weight_bone_
                        ? "Cycle weight bone (" + std::to_string(
                            static_cast<std::uint16_t>(*debug_weight_bone_)) + ")"
                        : "Weight heatmap: OFF",
                    true, false, 420.0F, 48.0F});
    context.ui.add({foundation::stable_id("unit-lab.variation"), ui::UiWidgetType::Button,
                    "Cycle phenotype variation", true, false, 420.0F, 48.0F});
    context.ui.add({foundation::stable_id("unit-lab.loadout"), ui::UiWidgetType::Button,
                    "Cycle equipment loadout", true, false, 420.0F, 48.0F});
    context.ui.add({foundation::stable_id("unit-lab.overrides"), ui::UiWidgetType::Button,
                    "Cycle genome overrides", true, false, 420.0F, 48.0F});
    context.ui.add({foundation::stable_id("unit-lab.back"), ui::UiWidgetType::Button,
                    "Back to main menu", true, true, 420.0F, 48.0F});
}

void UnitLabScene::build_presentation(SceneContext& context) {
    if (model_artifact_) {
        if (!skinned_prototype_ ||
            skinned_prototype_model_key_ != model_artifact_->cache_key) {
            skinned_prototype_ = infantry_presentation::makePrototype(*model_artifact_);
            skinned_prototype_model_key_ = model_artifact_->cache_key;
        }
        context.presentation.skinned_prototypes.push_back(skinned_prototype_);
        render::SkinnedBonePalette palette{};
        palette.instance_id = foundation::stable_id("unit-lab.infantry.instance");
        palette.skeleton_id = model_artifact_->skeleton.cacheKey();
        palette.pose_revision = animation_pose_ ? animation_pose_->revision : fixed_tick_;
        if (face_animator_) {
            palette.morph_weights[0] = face_animator_->output().eyelids_close;
            palette.morph_weights[1] = face_animator_->output().eyelids_arc;
            palette.morph_weights[2] = face_animator_->output().neck_flex;
            palette.morph_weights[3] = face_animator_->output().hands_relax;
        }
        const auto pose_bones = animation_pose_
            ? std::span<const infantry::RigTransform>(animation_pose_->bones)
            : std::span<const infantry::RigTransform>{};
        palette.matrices = infantry_presentation::makePalette(model_artifact_->skeleton,
                                                               pose_bones);
        palette.local_poses = infantry_presentation::makeLocalPoses(model_artifact_->skeleton,
                                                                    pose_bones);
        context.presentation.skinned_palettes.push_back(std::move(palette));
        if (!context.render_capabilities.gpu_skinning) {
            auto render_mesh = std::make_shared<render::RenderMesh>(
                render::deformSkinnedCPU(*skinned_prototype_,
                                         context.presentation.skinned_palettes.back().matrices,
                                         context.presentation.skinned_palettes.back().morph_weights));
            context.presentation.instance_prototypes.push_back(render_mesh);
        }
        const foundation::Vec3 center{
            (model_artifact_->appearance.minimum.x + model_artifact_->appearance.maximum.x) * 0.5F,
            (model_artifact_->appearance.minimum.y + model_artifact_->appearance.maximum.y) * 0.5F,
            (model_artifact_->appearance.minimum.z + model_artifact_->appearance.maximum.z) * 0.5F};
        const float extent = std::max(
            0.5F, model_artifact_->appearance.maximum.y - model_artifact_->appearance.minimum.y);

        foundation::Vec3 camera_target{center.x, center.y + extent * 0.02F, center.z};
        foundation::Vec3 camera_offset{0.0F, extent * 0.06F, extent * 2.10F};
        switch (camera_mode_) {
        case UnitLabCameraMode::Front:
            camera_offset = {0.0F, extent * 0.06F, extent * 2.10F};
            break;
        case UnitLabCameraMode::Side:
            camera_offset = {extent * 2.10F, extent * 0.06F, 0.0F};
            break;
        case UnitLabCameraMode::Back:
            camera_offset = {0.0F, extent * 0.06F, -extent * 2.10F};
            break;
        case UnitLabCameraMode::ThreeQuarter:
            camera_offset = {extent * 1.48F, extent * 0.08F, extent * 1.48F};
            break;
        case UnitLabCameraMode::Face: {
            const auto& face = model_artifact_->phenotype.face;
            camera_target = {0.0F, face.eye_y - face.eye_radius * 0.10F,
                             face.frontZ(face.eye_y) - face.eye_radius * 0.30F};
            camera_offset = {0.0F, extent * 0.015F, extent * 0.56F};
            break;
        }
        case UnitLabCameraMode::Hands: {
            const auto* left = model_artifact_->skeleton.findAttachment(
                infantry::AttachmentPointId::LeftHand);
            const auto* right = model_artifact_->skeleton.findAttachment(
                infantry::AttachmentPointId::RightHand);
            if (left != nullptr && right != nullptr) {
                camera_target = multiply(add(left->world, right->world), 0.5F);
            }
            camera_offset = {0.0F, extent * 0.02F, extent * 0.72F};
            break;
        }
        }

        context.presentation.camera = {
            true,
            add(camera_target, camera_offset),
            camera_target,
            {0.0F, 1.0F, 0.0F},
            camera_mode_ == UnitLabCameraMode::Face ? 0.62F : 0.72F,
            0.025F,
            100.0F};
        // The left side belongs to the RmlUi inspector. The camera controller
        // consumes this bounded preset; the renderer does not own orbit input.
        context.presentation.camera.viewport_left = 0.40F;
        context.presentation.camera.viewport_width = 0.60F;
        context.presentation.camera.revision = foundation::stableHashCombine(
            model_artifact_->cache_key,
            static_cast<std::uint64_t>(camera_mode_));
        auto camera_request = context.presentation.camera.toRequest();
        camera_request.preset = camera::CameraPreset::UnitLab;
        camera_request.mode = camera::CameraMode::Orbit;
        context.publishCameraRequest(camera_request);
        const float model_rotation =
            std::sin(static_cast<float>(elapsed_seconds_) * 0.35F) * 0.12F;
        std::optional<render::RenderMesh> debug_deformed;
        if (show_bounds_ || show_normals_ || show_wireframe_ || debug_weight_bone_) {
            const auto& live_palette = context.presentation.skinned_palettes.back();
            debug_deformed = render::deformSkinnedCPU(
                *skinned_prototype_, live_palette.matrices, live_palette.morph_weights);
        }
        const auto debugPoint = [model_rotation](Vec3 point) noexcept {
            return rotateY(point, model_rotation);
        };
        const auto addDebugLine = [&context, &debugPoint](Vec3 start, Vec3 end,
                                                           foundation::Color color) {
            context.presentation.debug_lines.push_back(
                {debugPoint(start), debugPoint(end), color});
        };
        if (show_bounds_) {
            Vec3 minimum = model_artifact_->appearance.minimum;
            Vec3 maximum = model_artifact_->appearance.maximum;
            if (debug_deformed && !debug_deformed->vertices.empty()) {
                minimum = debug_deformed->vertices.front().position;
                maximum = minimum;
                for (const auto& vertex : debug_deformed->vertices) {
                    minimum.x = std::min(minimum.x, vertex.position.x);
                    minimum.y = std::min(minimum.y, vertex.position.y);
                    minimum.z = std::min(minimum.z, vertex.position.z);
                    maximum.x = std::max(maximum.x, vertex.position.x);
                    maximum.y = std::max(maximum.y, vertex.position.y);
                    maximum.z = std::max(maximum.z, vertex.position.z);
                }
            }
            const Vec3 corners[] = {
                {minimum.x, minimum.y, minimum.z}, {maximum.x, minimum.y, minimum.z},
                {maximum.x, maximum.y, minimum.z}, {minimum.x, maximum.y, minimum.z},
                {minimum.x, minimum.y, maximum.z}, {maximum.x, minimum.y, maximum.z},
                {maximum.x, maximum.y, maximum.z}, {minimum.x, maximum.y, maximum.z}};
            constexpr std::uint32_t edges[][2] = {
                {0U,1U},{1U,2U},{2U,3U},{3U,0U},
                {4U,5U},{5U,6U},{6U,7U},{7U,4U},
                {0U,4U},{1U,5U},{2U,6U},{3U,7U}};
            for (const auto& edge : edges)
                addDebugLine(corners[edge[0]], corners[edge[1]],
                             {0.10F, 0.85F, 1.0F, 1.0F});
        }
        if (show_skeleton_) {
            const auto pose = debugPose(
                model_artifact_->skeleton, pose_bones,
                animation_pose_ ? animation_pose_->root_position : Vec3{});
            const auto bones = model_artifact_->skeleton.bones();
            for (std::size_t index = 0U; index < bones.size(); ++index) {
                if (bones[index].parent == infantry::kInvalidBoneIndex) {
                    continue;
                }
                addDebugLine(pose[bones[index].parent].position, pose[index].position,
                             {1.0F, 0.82F, 0.12F, 1.0F});
            }
        }
        if (show_normals_ || show_wireframe_ || debug_weight_bone_) {
            const auto positionAt = [&](std::size_t index) noexcept {
                return debug_deformed ? debug_deformed->vertices[index].position
                                      : skinned_prototype_->vertices[index].position;
            };
            const auto normalAt = [&](std::size_t index) noexcept {
                return debug_deformed ? debug_deformed->vertices[index].normal
                                      : skinned_prototype_->vertices[index].normal;
            };
            if (show_normals_ || debug_weight_bone_) {
                for (std::size_t index = 0U;
                     index < skinned_prototype_->vertices.size(); index += 8U) {
                    foundation::Color debug_color{1.0F, 0.10F, 0.85F, 1.0F};
                    if (debug_weight_bone_) {
                        float weight = 0.0F;
                        const auto selected =
                            static_cast<std::uint16_t>(*debug_weight_bone_);
                        const auto& vertex = skinned_prototype_->vertices[index];
                        for (std::size_t influence = 0U;
                             influence < vertex.bone_indices.size(); ++influence) {
                            if (vertex.bone_indices[influence] == selected)
                                weight += vertex.bone_weights[influence];
                        }
                        const float clamped = std::clamp(weight, 0.0F, 1.0F);
                        debug_color = {clamped, 0.12F + 0.76F * (1.0F - clamped),
                                       1.0F - clamped, 1.0F};
                    }
                    const auto position = positionAt(index);
                    addDebugLine(position, add(position, multiply(normalAt(index), 0.045F)),
                                 debug_color);
                }
            }
            if (show_wireframe_) {
                constexpr std::size_t kMaxDebugLines = 20'000U;
                for (std::size_t index = 0U;
                     index + 2U < skinned_prototype_->indices.size() &&
                     context.presentation.debug_lines.size() < kMaxDebugLines;
                     index += 3U) {
                    const Vec3 a = positionAt(skinned_prototype_->indices[index]);
                    const Vec3 b = positionAt(skinned_prototype_->indices[index + 1U]);
                    const Vec3 d = positionAt(skinned_prototype_->indices[index + 2U]);
                    addDebugLine(a, b, {0.15F, 1.0F, 0.35F, 1.0F});
                    addDebugLine(b, d, {0.15F, 1.0F, 0.35F, 1.0F});
                    addDebugLine(d, a, {0.15F, 1.0F, 0.35F, 1.0F});
                }
            }
        }
        if (show_surface_) {
            context.presentation.instances.push_back({
                foundation::stable_id("unit-lab.infantry.instance"), skinned_prototype_->mesh_id,
                foundation::stable_id("material.unit-lab.uniform"), {0.0F, 0.0F, 0.0F},
                {1.0F, 1.0F, 1.0F},
                model_rotation,
                fixed_tick_,
                render::RenderInstanceFlagPreview |
                    render::RenderInstanceFlagCastShadow |
                    render::RenderInstanceFlagReceiveShadow});
        }
        return;
    }
    if (!unit_prototype_) {
        unit_prototype_ = render::procedural::make_box(
            foundation::stable_id("mesh.unit-lab.prototype"), {0.30F, 0.80F, 0.30F},
            {0.86F, 0.90F, 0.96F, 1.0F});
    }
    context.presentation.instance_prototypes.push_back(unit_prototype_);
    const float time = static_cast<float>(elapsed_seconds_);
    const float rotation = std::sin(time * 0.35F) * 0.25F;
    constexpr foundation::StableId mesh_id = foundation::stable_id("mesh.unit-lab.prototype");
    const foundation::StableId materials[] = {
        foundation::stable_id("material.unit-lab.blue"),
        foundation::stable_id("material.unit-lab.red"),
        foundation::stable_id("material.unit-lab.neutral"),
    };
    const foundation::Vec3 positions[] = {
        {-2.0F, 0.80F, 0.0F}, {0.0F, 0.80F, 0.0F}, {2.0F, 0.80F, 0.0F},
    };
    const foundation::Vec3 scales[] = {
        {0.90F, 0.90F, 0.90F}, {1.0F, 1.15F, 1.0F}, {1.10F, 0.82F, 1.10F},
    };
    for (std::size_t index = 0; index < 3; ++index) {
        context.presentation.instances.push_back({
            foundation::stableHashCombine(foundation::stable_id("unit-lab.instance"), index),
            mesh_id, materials[index], positions[index], scales[index], rotation,
            0, render::RenderInstanceFlagPreview});
    }
}

} // namespace genomes::runtime
