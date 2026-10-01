#include <genomes/runtime/UnitLabScene.hpp>
#include <genomes/game_scenes/ApplicationCommand.hpp>
#include <genomes/runtime/UnitLabCommandParsing.hpp>
#include <genomes/runtime/InfantryPresentation.hpp>

#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/infantry/EquipmentCatalog.hpp>
#include <genomes/infantry/RagdollSchema.hpp>
#include <genomes/infantry/RigSchema.hpp>
#include <genomes/render/ProceduralMeshes.hpp>
#include <genomes/render/SkinnedDeformer.hpp>

#include <cmath>
#include <algorithm>
#include <array>
#include <charconv>
#include <string>
#include <string_view>
#include <span>
#include <type_traits>
#include <iterator>
#include <vector>

namespace genomes::runtime {

UnitLabViewport unitLabViewport(int framebuffer_width, int framebuffer_height,
                                double ui_scale) noexcept {
    const float width = static_cast<float>(std::max(1, framebuffer_width));
    const float height = static_cast<float>(std::max(1, framebuffer_height));
    const float scale = static_cast<float>(std::clamp(ui_scale, 0.75, 1.50));
    const float rail = 68.0F * scale;
    const float inspector = 340.0F * scale;
    const float top = (56.0F + 48.0F) * scale;
    const float caption = 34.0F * scale;
    const float free_center_x = (rail + width - inspector) * 0.5F;
    const float free_center_y = (top + height - caption) * 0.5F;
    return {0.0F, 0.0F, 1.0F, 1.0F,
            std::clamp(free_center_x * 2.0F / width - 1.0F, -1.0F, 1.0F),
            std::clamp(1.0F - free_center_y * 2.0F / height, -1.0F, 1.0F)};
}

namespace {

using Vec3 = foundation::Vec3;

template <typename T>
[[nodiscard]] std::string numberText(T value) {
    std::array<char, 64> buffer{};
    const auto converted = [&] {
        if constexpr (std::is_floating_point_v<T>)
            return std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                 std::chars_format::general);
        else
            return std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    }();
    return converted.ec == std::errc{} ? std::string{buffer.data(), converted.ptr}
                                        : std::string{};
}

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
    dirty_.mark(flag);
}

bool UnitLabScene::applyCommand(SceneContext& context, SetVariation command) {
    if (!infantry::isValidVariation(command.value)) return false;
    variation_ = command.value;
    rebuildModel(&context);
    return true;
}

bool UnitLabScene::applyCommand(SceneContext&, SetCameraMode command) {
    if (static_cast<std::uint8_t>(command.value) >
        static_cast<std::uint8_t>(UnitLabCameraMode::Hands)) return false;
    camera_mode_ = command.value;
    markDirty(UnitLabDirtyFlag::Presentation);
    markDirty(UnitLabDirtyFlag::Ui);
    return true;
}

bool UnitLabScene::applyCommand(SceneContext&, SetLocomotionPreset command) {
    if (static_cast<std::uint8_t>(command.value) >
        static_cast<std::uint8_t>(infantry::BipedPreset::CrouchWalk) ||
        !locomotion_ || !locomotion_state_) return false;
    if (!locomotion_->setPreset(*locomotion_state_, command.value)) return false;
    markDirty(UnitLabDirtyFlag::Pose);
    return true;
}

bool UnitLabScene::applyCommand(SceneContext&, SetExpression command) {
    if (static_cast<std::uint8_t>(command.value) >= infantry::kFaceExpressionCount) return false;
    expression_ = command.value;
    expression_intensity_ = expression_ == infantry::FaceExpression::Neutral ? 0.0F : 1.0F;
    if (face_animator_) (void)face_animator_->setExpression(expression_, expression_intensity_);
    markDirty(UnitLabDirtyFlag::Pose);
    markDirty(UnitLabDirtyFlag::Ui);
    return true;
}

bool UnitLabScene::applyCommand(SceneContext& context, SetEquipmentSlot command) {
    const auto slots = infantry::EquipmentCatalog::slots();
    const auto slot = std::find_if(slots.begin(), slots.end(), [&](const auto& candidate) {
        return candidate.slot == command.slot;
    });
    if (slot == slots.end()) return false;
    if (command.value.specified) {
        if (command.value.empty) {
            if (command.value.definition_id != 0U) return false;
        } else {
            const auto* item = infantry::EquipmentCatalog::findItem(command.value.definition_id);
            if (item == nullptr || !item->allows(command.slot)) return false;
        }
    } else if (command.value.empty || command.value.definition_id != 0U) {
        return false;
    }
    equipment_overrides_.slots[infantry::equipmentSlotIndex(command.slot)] = command.value;
    selected_equipment_slot_ = static_cast<std::size_t>(std::distance(slots.begin(), slot));
    rebuildModel(&context);
    return true;
}

bool UnitLabScene::applyCommand(SceneContext& context, SetGeneOverride command) {
    if (static_cast<std::size_t>(command.gene) >= infantry::GenomeGeneCount ||
        !std::isfinite(command.value)) return false;
    if (!genome_overrides_.set(command.gene, std::clamp(command.value, 0.0, 1.0))) return false;
    selected_genome_gene_ = command.gene;
    rebuildModel(&context);
    return true;
}

bool UnitLabScene::applyCommand(SceneContext&, SetAppearancePreset command) {
    if (command.value != 0U && command.value != kInspectionOliveAppearancePreset) {
        return false;
    }
    appearance_preset_ = command.value;
    markDirty(UnitLabDirtyFlag::Material);
    markDirty(UnitLabDirtyFlag::Ui);
    return true;
}

bool UnitLabScene::applyCommand(SceneContext& context, const UnitLabCommand& command) {
    return std::visit([this, &context](const auto& typed) {
        return applyCommand(context, typed);
    }, command);
}

bool UnitLabScene::executeControl(SceneContext& context, Control control) {
    switch (control) {
    case Control::Regenerate: ++preview_seed_; rebuildModel(&context); break;
    case Control::Detail: detail_level_ = detail_level_ >= 3U ? 1U : detail_level_ + 1U; rebuildModel(&context); break;
    case Control::CycleCamera:
        return applyCommand(context, {static_cast<UnitLabCameraMode>(
            (static_cast<std::uint8_t>(camera_mode_) + 1U) % 6U)});
    case Control::ToggleSurface: show_surface_ = !show_surface_; markDirty(UnitLabDirtyFlag::Presentation); markDirty(UnitLabDirtyFlag::Ui); break;
    case Control::ToggleWireframe: show_wireframe_ = !show_wireframe_; markDirty(UnitLabDirtyFlag::Presentation); markDirty(UnitLabDirtyFlag::Ui); break;
    case Control::ToggleSkeleton: show_skeleton_ = !show_skeleton_; markDirty(UnitLabDirtyFlag::Presentation); markDirty(UnitLabDirtyFlag::Ui); break;
    case Control::ToggleBounds: show_bounds_ = !show_bounds_; markDirty(UnitLabDirtyFlag::Presentation); markDirty(UnitLabDirtyFlag::Ui); break;
    case Control::ToggleNormals: show_normals_ = !show_normals_; markDirty(UnitLabDirtyFlag::Presentation); markDirty(UnitLabDirtyFlag::Ui); break;
    case Control::TogglePause: animation_paused_ = !animation_paused_; markDirty(UnitLabDirtyFlag::Pose); break;
    case Control::CycleExpression:
        expression_ = static_cast<infantry::FaceExpression>(
            (static_cast<std::uint8_t>(expression_) + 1U) % infantry::kFaceExpressionCount);
        expression_intensity_ = expression_ == infantry::FaceExpression::Neutral ? 0.0F : 1.0F;
        if (face_animator_) (void)face_animator_->setExpression(expression_, expression_intensity_);
        markDirty(UnitLabDirtyFlag::Pose); break;
    case Control::CycleWeightBone:
        debug_weight_bone_ = debug_weight_bone_
            ? static_cast<infantry::BoneId>((static_cast<std::uint16_t>(*debug_weight_bone_) + 1U) % infantry::kRigBoneCount)
            : infantry::BoneId::Hips;
        markDirty(UnitLabDirtyFlag::Presentation); markDirty(UnitLabDirtyFlag::Ui); break;
    case Control::CycleVariation:
        return applyCommand(context, SetVariation{variation_ < 1.0F ? 1.0F : variation_ < 1.5F ? 1.5F
                                                   : variation_ < 1.75F ? 1.75F : 0.5F});
    case Control::CycleLoadout:
        if (!infantry::infantryLoadouts().empty()) {
            loadout_index_ = (loadout_index_ + 1U) % infantry::infantryLoadouts().size();
            rebuildModel(&context);
        }
        break;
    case Control::CycleGenomePreset:
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
    case Control::ReturnToMenu:
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::ReturnToMainMenu);
        break;
    case Control::CycleLocomotion:
        if (locomotion_ && locomotion_state_) {
            const auto p = locomotion_state_->preset;
            const auto next = p == infantry::BipedPreset::Idle ? infantry::BipedPreset::Walk
                : p == infantry::BipedPreset::Walk ? infantry::BipedPreset::Run
                : p == infantry::BipedPreset::Run ? infantry::BipedPreset::Crouch
                : p == infantry::BipedPreset::Crouch ? infantry::BipedPreset::CrouchWalk
                : infantry::BipedPreset::Idle;
            return applyCommand(context, {next});
        }
        break;
    case Control::CycleExpressionIntensity:
        if (expression_ != infantry::FaceExpression::Neutral) {
            expression_intensity_ += 0.25F;
            if (expression_intensity_ > 1.001F) expression_intensity_ = 0.25F;
            if (face_animator_) (void)face_animator_->setExpression(expression_, expression_intensity_);
            markDirty(UnitLabDirtyFlag::Pose);
        }
        break;
    case Control::NextGenomeGene: {
        const auto next = (static_cast<std::size_t>(selected_genome_gene_) + 1U) %
                          infantry::GenomeGeneCount;
        selected_genome_gene_ = static_cast<infantry::GenomeGene>(next);
        markDirty(UnitLabDirtyFlag::Ui);
        break;
    }
    case Control::DecreaseGenomeGene:
    case Control::IncreaseGenomeGene: {
        double value = 0.5;
        if (const auto override = genome_overrides_.get(selected_genome_gene_); override) {
            value = *override;
        } else if (model_artifact_) {
            value = model_artifact_->genome.geneValue(selected_genome_gene_);
        }
        value = std::clamp(value + (control == Control::DecreaseGenomeGene ? -0.10 : 0.10), 0.0, 1.0);
        genome_override_mode_ = 0U;
        return applyCommand(context, {selected_genome_gene_, value});
    }
    case Control::ClearGenomeGene: {
        const auto index = static_cast<std::size_t>(selected_genome_gene_);
        if (index < genome_overrides_.genes.size()) genome_overrides_.genes[index].reset();
        genome_override_mode_ = 0U;
        rebuildModel(&context);
        break;
    }
    case Control::ClearAllGenomeGenes:
        genome_overrides_ = {};
        genome_override_mode_ = 0U;
        rebuildModel(&context);
        break;
    case Control::CycleEquipmentSlot: {
        const auto slots = infantry::EquipmentCatalog::slots();
        if (!slots.empty()) selected_equipment_slot_ = (selected_equipment_slot_ + 1U) % slots.size();
        markDirty(UnitLabDirtyFlag::Ui);
        break;
    }
    case Control::CycleEquipmentItem: {
        const auto slots = infantry::EquipmentCatalog::slots();
        const auto items = infantry::EquipmentCatalog::items();
        if (slots.empty()) break;
        const auto slot = slots[selected_equipment_slot_ % slots.size()].slot;
        const auto slot_index = infantry::equipmentSlotIndex(slot);
        std::vector<foundation::StableId> allowed;
        allowed.reserve(items.size());
        for (const auto& item : items) if (item.allows(slot)) allowed.push_back(item.id);
        auto current = equipment_overrides_.slots[slot_index];
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
        return applyCommand(context, {slot, current});
    }
    case Control::ClearEquipment:
        equipment_overrides_ = {};
        rebuildModel(&context);
        break;
    default: return false;
    }
    return true;
}

void UnitLabScene::publishModelResult(
    foundation::Result<infantry::InfantryModelCompileResult, foundation::Error>&& compiled) {
    if (!compiled) {
        last_generation_error_ = compiled.error();
        markDirty(UnitLabDirtyFlag::Ui);
        return;
    }
    model_artifact_ = std::move(compiled.value().artifact);
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
    markDirty(UnitLabDirtyFlag::Presentation);
    markDirty(UnitLabDirtyFlag::Ui);
}

void UnitLabScene::rebuildModel(SceneContext* context) {
    infantry::InfantryModelRequest request{};
    request.seed = preview_seed_;
    request.variation = variation_;
    request.detail_level = static_cast<infantry::InfantryDetail>(detail_level_);
    request.side = side_;
    request.wear = equipment_wear_;
    static constexpr std::array<foundation::Color, 4> uniforms{{
        infantry::kDefaultUniformColor,
        {0.20F, 0.25F, 0.16F, 1.0F}, {0.30F, 0.27F, 0.20F, 1.0F},
        {0.12F, 0.15F, 0.18F, 1.0F}}};
    request.palette.uniform = uniforms[palette_index_ % uniforms.size()];
    request.genome_overrides = genome_overrides_;
    request.uniform_color = request.palette.uniform;
    const auto loadouts = infantry::infantryLoadouts();
    if (!loadouts.empty()) {
        request.loadout_id = loadouts[loadout_index_ % loadouts.size()].id;
    }
    request.equipment_overrides = equipment_overrides_;
    const auto request_key = infantry::InfantryModelCompiler::canonicalRequestKey(request);
    if (context != nullptr && context->jobs != nullptr && !context->deterministic_capture) {
        const auto submission = model_request_gate_.submit(request_key);
        if (submission.queued) {
            // Keep at most one active compile and one overwriteable request.
            // The current prototype remains visible while the newest request waits.
            queued_model_request_ = std::move(request);
            markDirty(UnitLabDirtyFlag::Ui);
            return;
        }
        startModelRequest(*context, std::move(request), submission.token);
        return;
    }
    const auto submission = model_request_gate_.submit(request_key);
    auto result = model_compiler_.compile(request);
    const auto completion = model_request_gate_.complete(submission.token);
    if (completion.action == UnitLabModelRequestAction::Publish) {
        publishModelResult(std::move(result));
    }
}

void UnitLabScene::startModelRequest(SceneContext& context,
                                     infantry::InfantryModelRequest request,
                                     UnitLabModelRequestToken token) {
    const auto pending = std::make_shared<PendingModelResult>();
    pending_model_result_ = pending;
    model_job_ = context.jobs->submit(
            [this, request, token, pending](jobs::JobContext&) mutable {
                auto result = model_compiler_.compile(request);
                {
                    std::lock_guard lock(pending->mutex);
                    pending->revision = token.revision;
                    pending->request_key = token.request_key;
                    pending->result = std::move(result);
                }
            });
    markDirty(UnitLabDirtyFlag::Ui);
}

void UnitLabScene::on_enter(SceneContext& context) {
    model_request_gate_.cancel();
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
    dirty_.markAll();
    skinned_prototype_.reset();
    skinned_prototype_model_key_ = 0;
    rebuildModel(&context);
    dirty_.clear(UnitLabDirtyFlag::Geometry);
    dirty_.clear(UnitLabDirtyFlag::Material);
    dirty_.clear(UnitLabDirtyFlag::Presentation);
    context.ui.clear();
}

void UnitLabScene::on_exit(SceneContext&) {
    if (model_job_.valid()) {
        model_job_.wait();
        model_job_ = {};
    }
    pending_model_result_.reset();
    queued_model_request_.reset();
    model_request_gate_.cancel();
}

void UnitLabScene::handle_input(SceneContext& context, const input::InputFrame& input) {
    if (locomotion_ && locomotion_state_) {
        if (input.right_pressed) {
            const auto current = locomotion_state_->preset;
            const auto next = current == infantry::BipedPreset::Idle
                ? infantry::BipedPreset::Walk
                : current == infantry::BipedPreset::Walk
                    ? infantry::BipedPreset::Run
                    : current == infantry::BipedPreset::Run
                        ? infantry::BipedPreset::Crouch
                        : current == infantry::BipedPreset::Crouch
                            ? infantry::BipedPreset::CrouchWalk
                        : infantry::BipedPreset::Idle;
            (void)applyCommand(context, {next});
        } else if (input.left_pressed) {
            const auto current = locomotion_state_->preset;
            const auto previous = current == infantry::BipedPreset::Idle
                ? infantry::BipedPreset::CrouchWalk
                : current == infantry::BipedPreset::CrouchWalk
                    ? infantry::BipedPreset::Crouch
                : current == infantry::BipedPreset::Crouch
                    ? infantry::BipedPreset::Run
                    : current == infantry::BipedPreset::Run
                        ? infantry::BipedPreset::Walk
                        : infantry::BipedPreset::Idle;
            (void)applyCommand(context, {previous});
        }
    }
    if (input.cancel_pressed || input.confirm_pressed) {
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::ReturnToMainMenu);
    }
}

ui::UiActionResult UnitLabScene::handle_ui_action(
    SceneContext& context, ui::UiActionId action, const ui::UiActionArguments& arguments) {
    const auto value_of = [&]() -> std::string_view {
        for (const auto& argument : arguments) if (argument.first == "value") return argument.second;
        return {};
    };
    const auto key_of = [&]() -> std::string_view {
        for (const auto& argument : arguments) if (argument.first == "key") return argument.second;
        return {};
    };
    const auto text = value_of();
    if (action == foundation::stable_id("unit.tab")) {
        static constexpr std::array<std::string_view, 6> names{
            "model", "equipment", "genome", "skeleton", "animation", "face"};
        const auto it = std::find(names.begin(), names.end(), key_of());
        if (it == names.end()) return ui::UiActionResult::Rejected;
        active_tab_ = static_cast<std::uint8_t>(std::distance(names.begin(), it));
        markDirty(UnitLabDirtyFlag::Ui);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("unit.seed")) {
        std::uint64_t value = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
            return ui::UiActionResult::Rejected;
        preview_seed_ = value;
        rebuildModel(&context);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("unit.genome") && !key_of().empty()) {
        const auto command = parseUnitLabRmlCommand("unit.genome", key_of(), text);
        if (!command) return ui::UiActionResult::Rejected;
        return applyCommand(context, command.value())
            ? ui::UiActionResult::Handled : ui::UiActionResult::Rejected;
    }
    if (action == foundation::stable_id("unit.weight") && !text.empty()) {
        if (text == "off") {
            debug_weight_bone_.reset();
        } else {
            std::uint16_t index = 0;
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), index);
            if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
                index >= infantry::kRigBoneCount) return ui::UiActionResult::Rejected;
            debug_weight_bone_ = static_cast<infantry::BoneId>(index);
        }
        markDirty(UnitLabDirtyFlag::Presentation);
        markDirty(UnitLabDirtyFlag::Ui);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("unit.genome-reset")) {
        genome_overrides_ = {};
        rebuildModel(&context);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("unit.genome-preset") && !text.empty()) {
        genome_overrides_ = {};
        if (text == "short") {
            (void)genome_overrides_.set(infantry::GenomeGene::Height, (1.65 - 1.60) / 0.35);
        } else if (text == "tall") {
            (void)genome_overrides_.set(infantry::GenomeGene::Height, (1.90 - 1.60) / 0.35);
        } else if (text == "broad") {
            (void)genome_overrides_.set(infantry::GenomeGene::BodyShoulderBreadth, 1.0);
            (void)genome_overrides_.set(infantry::GenomeGene::BodyHipBreadth, 0.0);
        } else return ui::UiActionResult::Rejected;
        rebuildModel(&context);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("unit.equipment-item") && !key_of().empty()) {
        const auto command = parseUnitLabRmlCommand("unit.equipment-item", key_of(), text);
        if (!command) return ui::UiActionResult::Rejected;
        return applyCommand(context, command.value())
            ? ui::UiActionResult::Handled : ui::UiActionResult::Rejected;
    }
    if (action == foundation::stable_id("unit.loadout-select")) {
        const auto loadouts = infantry::infantryLoadouts();
        const auto it = std::find_if(loadouts.begin(), loadouts.end(), [&](const auto& loadout) {
            return loadout.identifier == text;
        });
        if (it == loadouts.end()) return ui::UiActionResult::Rejected;
        loadout_index_ = static_cast<std::size_t>(std::distance(loadouts.begin(), it));
        rebuildModel(&context);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("unit.wear")) {
        double value = 0.0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value,
                                            std::chars_format::general);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
            !std::isfinite(value)) return ui::UiActionResult::Rejected;
        equipment_wear_ = std::clamp(static_cast<float>(value), 0.0F, 1.0F);
        rebuildModel(&context);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("unit.phase")) {
        double value = 0.0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value,
                                            std::chars_format::general);
        if (!locomotion_state_ || parsed.ec != std::errc{} || !std::isfinite(value))
            return ui::UiActionResult::Rejected;
        locomotion_state_->phase = std::clamp(value, 0.0, 1.0);
        animation_paused_ = true;
        markDirty(UnitLabDirtyFlag::Pose);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("unit.animation-speed")) {
        double value = 0.0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value,
                                            std::chars_format::general);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
            !std::isfinite(value)) return ui::UiActionResult::Rejected;
        animation_speed_ = std::clamp(static_cast<float>(value), 0.0F, 2.0F);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("unit.palette")) {
        static constexpr std::array<std::string_view, 4> names{"Standard", "Forest", "Field", "Night"};
        const auto it = std::find(names.begin(), names.end(), text);
        if (it == names.end()) return ui::UiActionResult::Rejected;
        palette_index_ = static_cast<std::size_t>(std::distance(names.begin(), it));
        rebuildModel(&context);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("unit.appearance-preset")) {
        const auto command = parseUnitLabRmlCommand(
            "unit.appearance-preset", {}, text);
        if (!command) return ui::UiActionResult::Rejected;
        return applyCommand(context, command.value())
            ? ui::UiActionResult::Handled : ui::UiActionResult::Rejected;
    }
    if (action == foundation::stable_id("unit.side")) {
        static constexpr std::array<std::string_view, 3> names{"Side A", "Side B", "Neutral"};
        const auto it = std::find(names.begin(), names.end(), text);
        if (it == names.end()) return ui::UiActionResult::Rejected;
        side_ = static_cast<infantry::InfantrySide>(std::distance(names.begin(), it));
        rebuildModel(&context);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("unit.detail")) {
        std::uint32_t value = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) return ui::UiActionResult::Rejected;
        detail_level_ = std::clamp(value, 1U, 3U);
        rebuildModel(&context);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("unit.variation")) {
        const auto command = parseUnitLabRmlCommand("unit.variation", {}, text);
        if (!command) return ui::UiActionResult::Rejected;
        return applyCommand(context, command.value())
            ? ui::UiActionResult::Handled : ui::UiActionResult::Rejected;
    }
    const auto bool_value = [&]() -> bool { return text == "true" || text == "1"; };
    if (action == foundation::stable_id("unit.surface")) { show_surface_ = bool_value(); markDirty(UnitLabDirtyFlag::Presentation); markDirty(UnitLabDirtyFlag::Ui); return ui::UiActionResult::Handled; }
    if (action == foundation::stable_id("unit.wireframe")) { show_wireframe_ = bool_value(); markDirty(UnitLabDirtyFlag::Presentation); markDirty(UnitLabDirtyFlag::Ui); return ui::UiActionResult::Handled; }
    if (action == foundation::stable_id("unit.skeleton")) { show_skeleton_ = bool_value(); markDirty(UnitLabDirtyFlag::Presentation); markDirty(UnitLabDirtyFlag::Ui); return ui::UiActionResult::Handled; }
    if (action == foundation::stable_id("unit.bounds")) { show_bounds_ = bool_value(); markDirty(UnitLabDirtyFlag::Presentation); markDirty(UnitLabDirtyFlag::Ui); return ui::UiActionResult::Handled; }
    if (action == foundation::stable_id("unit.normals")) { show_normals_ = bool_value(); markDirty(UnitLabDirtyFlag::Presentation); markDirty(UnitLabDirtyFlag::Ui); return ui::UiActionResult::Handled; }
    if (action == foundation::stable_id("unit.auto-rotate")) { auto_rotate_ = bool_value(); markDirty(UnitLabDirtyFlag::Presentation); markDirty(UnitLabDirtyFlag::Ui); return ui::UiActionResult::Handled; }
    if (action == foundation::stable_id("unit.camera-reset")) return applyCommand(context, {UnitLabCameraMode::ThreeQuarter}) ? ui::UiActionResult::Handled : ui::UiActionResult::Rejected;
    if (action == foundation::stable_id("unit.camera") && !text.empty()) {
        const auto command = parseUnitLabRmlCommand("unit.camera", {}, text);
        if (!command) return ui::UiActionResult::Rejected;
        return applyCommand(context, command.value())
            ? ui::UiActionResult::Handled : ui::UiActionResult::Rejected;
    }
    if (action == foundation::stable_id("unit.locomotion") && !text.empty()) {
        if (!locomotion_ || !locomotion_state_) return ui::UiActionResult::Rejected;
        const auto command = parseUnitLabRmlCommand("unit.locomotion", {}, text);
        if (!command) return ui::UiActionResult::Rejected;
        return applyCommand(context, command.value())
            ? ui::UiActionResult::Handled : ui::UiActionResult::Rejected;
    }
    if (action == foundation::stable_id("unit.expression")) {
        const auto command = parseUnitLabRmlCommand("unit.expression", {}, text);
        return command && applyCommand(context, command.value())
            ? ui::UiActionResult::Handled : ui::UiActionResult::Rejected;
    }
    if (action == foundation::stable_id("unit.expression-intensity")) {
        double value = 0.0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value,
                                            std::chars_format::general);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || !std::isfinite(value)) return ui::UiActionResult::Rejected;
        expression_intensity_ = std::clamp(static_cast<float>(value), 0.0F, 1.0F);
        if (face_animator_) (void)face_animator_->setExpression(expression_, expression_intensity_);
        markDirty(UnitLabDirtyFlag::Pose);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("unit.genome-clear") && !key_of().empty()) {
        const auto gene = infantry::genomeGeneFromName(key_of());
        if (!gene) return ui::UiActionResult::Rejected;
        genome_overrides_.genes[static_cast<std::size_t>(*gene)].reset();
        rebuildModel(&context);
        return ui::UiActionResult::Handled;
    }
    struct Binding { ui::UiActionId id; Control control; };
    static constexpr Binding bindings[] = {
        {foundation::stable_id("unit.regenerate"), Control::Regenerate}, {foundation::stable_id("unit.detail"), Control::Detail},
        {foundation::stable_id("unit.camera"), Control::CycleCamera}, {foundation::stable_id("unit.surface"), Control::ToggleSurface},
        {foundation::stable_id("unit.wireframe"), Control::ToggleWireframe}, {foundation::stable_id("unit.skeleton"), Control::ToggleSkeleton},
        {foundation::stable_id("unit.bounds"), Control::ToggleBounds}, {foundation::stable_id("unit.normals"), Control::ToggleNormals},
        {foundation::stable_id("unit.pause"), Control::TogglePause}, {foundation::stable_id("unit.expression"), Control::CycleExpression},
        {foundation::stable_id("unit.weight"), Control::CycleWeightBone}, {foundation::stable_id("unit.variation"), Control::CycleVariation},
        {foundation::stable_id("unit.loadout"), Control::CycleLoadout}, {foundation::stable_id("unit.genome-preset"), Control::CycleGenomePreset},
        {foundation::stable_id("unit.back"), Control::ReturnToMenu}, {foundation::stable_id("unit.locomotion"), Control::CycleLocomotion},
        {foundation::stable_id("unit.expression-intensity"), Control::CycleExpressionIntensity},
        {foundation::stable_id("unit.genome-next"), Control::NextGenomeGene},
        {foundation::stable_id("unit.genome-minus"), Control::DecreaseGenomeGene},
        {foundation::stable_id("unit.genome-plus"), Control::IncreaseGenomeGene},
        {foundation::stable_id("unit.genome-clear"), Control::ClearGenomeGene},
        {foundation::stable_id("unit.genome-clear-all"), Control::ClearAllGenomeGenes},
        {foundation::stable_id("unit.equipment-slot"), Control::CycleEquipmentSlot},
        {foundation::stable_id("unit.equipment-item"), Control::CycleEquipmentItem},
        {foundation::stable_id("unit.equipment-clear"), Control::ClearEquipment}
    };
    for (const auto& binding : bindings) if (action == binding.id)
        return executeControl(context, binding.control)
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
            (void)locomotion_->step(*locomotion_state_, fixed_dt * animation_speed_);
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
                std::span<infantry::AnimationEntity>(&entity, 1U), fixed_tick_,
                fixed_dt * animation_speed_,
                nullptr);
            if (!animation_system_->currentSnapshot().poses.empty()) {
                animation_pose_ = animation_system_->currentSnapshot().poses.front();
            }
            markDirty(UnitLabDirtyFlag::Pose);
            markDirty(UnitLabDirtyFlag::Ui);
        }
    }
}

void UnitLabScene::frame_update(SceneContext& context, double) {
    if (model_job_.valid() && model_job_.isComplete()) {
        if (pending_model_result_) {
            std::optional<foundation::Result<infantry::InfantryModelCompileResult,
                                              foundation::Error>> result;
            std::uint64_t revision = 0U;
            foundation::StableId request_key = 0U;
            {
                std::lock_guard lock(pending_model_result_->mutex);
                if (pending_model_result_->revision && pending_model_result_->request_key &&
                    pending_model_result_->result) {
                    revision = *pending_model_result_->revision;
                    request_key = *pending_model_result_->request_key;
                    result = std::move(pending_model_result_->result);
                }
            }
            const auto completion = model_request_gate_.complete({revision, request_key});
            if (result && completion.action == UnitLabModelRequestAction::Publish) {
                publishModelResult(std::move(*result));
            }
            if (completion.action == UnitLabModelRequestAction::StartPending &&
                queued_model_request_) {
                auto request = std::move(*queued_model_request_);
                queued_model_request_.reset();
                startModelRequest(context, std::move(request), completion.next);
            }
        }
        model_job_ = {};
        pending_model_result_.reset();
    }
    if (!dirty_.contains(UnitLabDirtyFlag::Ui)) return;
    dirty_.clear(UnitLabDirtyFlag::Ui);
    context.ui.clear();
    std::string metrics = model_artifact_
        ? "MODEL READY | SEED " + numberText(preview_seed_) +
          " | RIFLEMAN | DETAIL " + numberText(detail_level_)
        : "MODEL COMPILATION FAILED";
    if (last_generation_error_) {
        metrics += " | PREVIOUS MODEL / ERROR " +
                   std::string(last_generation_error_->message);
    }
    if (model_artifact_) {
        const auto& appearance = model_artifact_->appearance;
        metrics += " | BONES " + numberText(model_artifact_->skeleton.bones().size());
        metrics += " | VERTICES " + numberText(appearance.body.vertices.size() +
                                                     appearance.hair.vertices.size());
        metrics += " | TRIANGLES " + numberText(
            (appearance.body.indices.size() + appearance.hair.indices.size()) / 3U);
        metrics += " | MORPHS " + numberText(appearance.morphs.size());
        metrics += " | CACHE H" + numberText(model_compiler_.cacheHits());
        metrics += "/M" + numberText(model_compiler_.cacheMisses());
        metrics += show_surface_ ? " | SURFACE" : " | SURFACE OFF";
        metrics += show_wireframe_ ? " | WIREFRAME" : "";
        metrics += show_skeleton_ ? " | SKELETON" : "";
        metrics += show_bounds_ ? " | BOUNDS" : "";
        metrics += show_normals_ ? " | NORMALS" : "";
        metrics += debug_weight_bone_
            ? " | WEIGHT " + numberText(static_cast<std::uint16_t>(*debug_weight_bone_))
            : "";
        metrics += " | CAMERA " + numberText(static_cast<int>(camera_mode_));
        metrics += " | VAR " + numberText(variation_);
        metrics += " | OVERRIDE " + numberText(genome_override_mode_);
        metrics += " | GENE " + std::string(infantry::genomeGeneName(selected_genome_gene_));
        const auto selected_override = genome_overrides_.get(selected_genome_gene_);
        metrics += selected_override ? "=" + numberText(*selected_override) : "=seed";
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
        metrics += " | EXPRESSION " + numberText(static_cast<int>(expression_)) +
                   "@" + numberText(expression_intensity_);
        metrics += " | GPU UPLOAD " + numberText(context.render_telemetry.mesh_uploads);
        metrics += " | PALETTE " + numberText(context.render_telemetry.palette_updates);
        metrics += " | DRAW " + numberText(context.render_telemetry.draw_calls);
        metrics += " | UI " + numberText(context.render_telemetry.ui_draw_calls);
    }
    auto& model = context.ui.model();
    (void)model.set("title", std::string{"UNIT LAB"});
    (void)model.set("description", std::string{"Procedural infantry prototypes"});
    (void)model.set("metrics", std::move(metrics));
    (void)model.set("seed", numberText(preview_seed_));
    ui::UiFieldState detail{}; detail.value = static_cast<std::int64_t>(detail_level_);
    detail.commit_policy = ui::UiCommitPolicy::OnChange; detail.minimum = 1.0; detail.maximum = 3.0;
    (void)model.set_field("detail", std::move(detail));
    ui::UiFieldState variation{}; variation.value = static_cast<double>(variation_);
    variation.commit_policy = ui::UiCommitPolicy::Live; variation.minimum = 0.0; variation.maximum = 1.75; variation.step = 0.05;
    (void)model.set_field("variation", std::move(variation));
    (void)model.set("animation_paused", animation_paused_);
    (void)model.set("surface", show_surface_);
    (void)model.set("wireframe", show_wireframe_);
    (void)model.set("skeleton", show_skeleton_);
    (void)model.set("bounds", show_bounds_);
    (void)model.set("normals", show_normals_);
    (void)model.set("auto_rotate", auto_rotate_);
    static constexpr std::array<std::string_view, 6> tab_names{
        "model", "equipment", "genome", "skeleton", "animation", "face"};
    for (std::size_t index = 0; index < tab_names.size(); ++index)
        (void)model.set("tab_" + std::string{tab_names[index]}, index == active_tab_);
    (void)model.set("wear", static_cast<double>(equipment_wear_));
    (void)model.set("animation_speed", static_cast<double>(animation_speed_));
    (void)model.set("animation_phase", locomotion_state_ ? locomotion_state_->phase : 0.0);
    (void)model.set("expression_intensity", static_cast<double>(expression_intensity_));
    (void)model.set("expression_intensity_enabled", expression_ != infantry::FaceExpression::Neutral);
    (void)model.set("pause_label", std::string{animation_paused_ ? "Resume" : "Pause"});
    const bool updating = model_job_.valid() && !model_job_.isComplete();
    (void)model.set("status_compact", std::string{last_generation_error_ ? "Error" : updating ? "Updating" : "Ready"});
    (void)model.set("equipment_seed", model_artifact_
        ? numberText(model_artifact_->equipment.equipment_seed) : std::string{"--"});

    std::vector<ui::UiTableRow> genes;
    genes.reserve(infantry::GenomeGeneCount);
    for (std::size_t index = 0; index < infantry::GenomeGeneCount; ++index) {
        const auto gene = static_cast<infantry::GenomeGene>(index);
        const auto override = genome_overrides_.get(gene);
        const double value = override.value_or(model_artifact_
            ? model_artifact_->genome.geneValue(gene) : 0.5);
        genes.push_back({{"id", std::string{infantry::genomeGeneName(gene)}},
                         {"group", index < 2 ? std::string{"Root"} :
                                   index < 19 ? std::string{"Body"} : std::string{"Face"}},
                         {"label", std::string{infantry::genomeGeneName(gene)}},
                         {"value", value}, {"enabled", true},
                         {"selected", gene == selected_genome_gene_},
                         {"overridden", override.has_value()}});
    }
    (void)model.set_list("genes", std::move(genes));

    std::vector<ui::UiTableRow> slots;
    for (const auto& definition : infantry::EquipmentCatalog::slots()) {
        const auto index = infantry::equipmentSlotIndex(definition.slot);
        std::string value{"auto"};
        const auto& override = equipment_overrides_.slots[index];
        if (override.specified) {
            value = override.empty ? "none" : numberText(override.definition_id);
            if (!override.empty) if (const auto* item = infantry::EquipmentCatalog::findItem(
                    override.definition_id); item != nullptr) value = std::string{item->identifier};
        } else if (model_artifact_) {
            if (const auto* item = model_artifact_->equipment.item(definition.slot); item != nullptr)
                if (const auto* catalog = infantry::EquipmentCatalog::findItem(item->definition_id);
                    catalog != nullptr) value = std::string{catalog->identifier};
        }
        const std::string_view identifier = definition.identifier;
        const std::string group = identifier.find("weapon") != std::string_view::npos
            ? "Weapons" : identifier.find("armor") != std::string_view::npos ||
              identifier.find("plate") != std::string_view::npos ? "Armor" :
              identifier.find("pouch") != std::string_view::npos ? "Attachments" : "Apparel";
        slots.push_back({{"id", std::string{definition.identifier}}, {"group", group},
                         {"label", std::string{definition.identifier}}, {"value", std::move(value)},
                         {"enabled", true}, {"selected", index == selected_equipment_slot_},
                         {"overridden", override.specified}});
    }
    (void)model.set_list("equipment_slots", std::move(slots));
    std::vector<ui::UiTableRow> equipment_items;
    for (const auto& slot : infantry::EquipmentCatalog::slots()) {
        const auto& selected = equipment_overrides_.slots[infantry::equipmentSlotIndex(slot.slot)];
        equipment_items.push_back({{"id", std::string{"auto"}}, {"group", std::string{slot.identifier}},
            {"label", std::string{"Auto"}}, {"value", std::string{"auto"}},
            {"enabled", true}, {"selected", !selected.specified}});
        equipment_items.push_back({{"id", std::string{"none"}}, {"group", std::string{slot.identifier}},
            {"label", std::string{"None"}}, {"value", std::string{"none"}},
            {"enabled", true}, {"selected", selected.specified && selected.empty}});
        for (const auto& item : infantry::EquipmentCatalog::items()) {
            if (!item.allows(slot.slot)) continue;
            equipment_items.push_back({{"id", std::string{item.identifier}},
                {"group", std::string{slot.identifier}}, {"label", std::string{item.identifier}},
                {"value", std::string{item.identifier}}, {"enabled", true},
                {"selected", selected.specified && !selected.empty && selected.definition_id == item.id}});
        }
    }
    (void)model.set_list("equipment_items", std::move(equipment_items));
    std::vector<ui::UiTableRow> loadouts;
    for (const auto& loadout : infantry::infantryLoadouts())
        loadouts.push_back({{"id", std::string{loadout.identifier}},
            {"label", std::string{loadout.identifier}}, {"value", std::string{loadout.identifier}},
            {"enabled", true}, {"selected", loadouts.size() == loadout_index_}});
    (void)model.set_list("loadouts", std::move(loadouts));

    std::vector<ui::UiTableRow> bones;
    if (model_artifact_) {
        const auto ragdoll = infantry::RagdollSchema::build(
            model_artifact_->phenotype.body, model_artifact_->skeleton);
        const auto skeleton_bones = model_artifact_->skeleton.bones();
        const auto schema = infantry::rigSchema();
        for (std::size_t index = 0; index < skeleton_bones.size(); ++index) {
            const auto& bone = skeleton_bones[index];
            const std::string parent = bone.parent == infantry::kInvalidBoneIndex
                ? std::string{"root"} : std::string{schema[bone.parent].name};
            bones.push_back({{"id", numberText(index)}, {"group", std::string{"Hierarchy"}},
                             {"label", std::string{schema[index].name}},
                             {"value", std::string{"parent: "} + parent},
                             {"enabled", true}, {"selected", debug_weight_bone_ &&
                                static_cast<std::size_t>(*debug_weight_bone_) == index},
                             {"overridden", ragdoll && std::any_of(ragdoll.value().bodies().begin(),
                                ragdoll.value().bodies().end(), [index](const auto& body) {
                                    return static_cast<std::size_t>(body.bone) == index;
                                })}});
        }
        (void)model.set("ragdoll_readout", ragdoll
            ? numberText(ragdoll.value().bodies().size()) + " bodies / " +
              numberText(ragdoll.value().constraints().size()) + " constraints"
            : std::string{"Unavailable"});
    }
    (void)model.set_list("bones", std::move(bones));
    (void)model.set("busy", model_job_.valid() && !model_job_.isComplete());
    (void)model.set("error", last_generation_error_ ?
        std::string{last_generation_error_->message} : std::string{});
}

void UnitLabScene::build_presentation(SceneContext& context) {
    if (model_artifact_) {
        if (dirty_.contains(UnitLabDirtyFlag::Geometry) ||
            dirty_.contains(UnitLabDirtyFlag::Material) || !skinned_prototype_ ||
            skinned_prototype_model_key_ != model_artifact_->cache_key) {
            const auto base_prototype = infantry_presentation::makePrototype(*model_artifact_);
            if (!base_prototype) return;
            skinned_prototype_ = appearance_preset_ == 0U
                ? base_prototype
                : infantry_presentation::makeMaterialVariant(*base_prototype,
                                                               appearance_preset_);
            if (!skinned_prototype_) return;
            skinned_prototype_model_key_ = model_artifact_->cache_key;
            dirty_.clear(UnitLabDirtyFlag::Geometry);
            dirty_.clear(UnitLabDirtyFlag::Material);
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
        dirty_.clear(UnitLabDirtyFlag::Pose);
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
        if (const auto& metrics = context.ui.viewport_metrics(); metrics && metrics->valid()) {
            last_ui_viewport_metrics_ = *metrics;
        }
        const auto viewport = [&] {
            if (!last_ui_viewport_metrics_) {
                return unitLabViewport(context.framebuffer_width, context.framebuffer_height,
                                       context.ui_scale);
            }
            const float width = static_cast<float>(std::max(1, context.framebuffer_width));
            const float height = static_cast<float>(std::max(1, context.framebuffer_height));
            const auto& metrics = *last_ui_viewport_metrics_;
            return UnitLabViewport{std::clamp(metrics.left / width, 0.0F, 1.0F),
                                   std::clamp(metrics.top / height, 0.0F, 1.0F),
                                   std::clamp(metrics.width / width, 0.0F, 1.0F),
                                   std::clamp(metrics.height / height, 0.0F, 1.0F),
                                   0.0F, 0.0F};
        }();
        context.presentation.camera.viewport_left = viewport.left;
        context.presentation.camera.viewport_top = viewport.top;
        context.presentation.camera.viewport_width = viewport.width;
        context.presentation.camera.viewport_height = viewport.height;
        context.presentation.camera.projection_offset_x = viewport.projection_offset_x;
        context.presentation.camera.projection_offset_y = viewport.projection_offset_y;
        context.presentation.camera.revision = foundation::stableHashCombine(
            model_artifact_->cache_key,
            static_cast<std::uint64_t>(camera_mode_));
        auto camera_request = context.presentation.camera.toRequest();
        camera_request.preset = camera::CameraPreset::UnitLab;
        camera_request.mode = camera::CameraMode::Orbit;
        context.publishCameraRequest(camera_request);
        const float model_rotation = auto_rotate_
            ? std::sin(static_cast<float>(elapsed_seconds_) * 0.35F) * 0.12F : 0.0F;
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
        dirty_.clear(UnitLabDirtyFlag::Presentation);
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
    dirty_.clear(UnitLabDirtyFlag::Presentation);
}

} // namespace genomes::runtime
