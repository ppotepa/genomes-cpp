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

void UnitLabScene::rebuildModel() {
    infantry::InfantryModelRequest request{};
    request.seed = preview_seed_;
    request.variation = variation_;
    request.detail_level = detail_level_;
    request.genome_overrides = genome_overrides_;
    request.uniform_color = infantry::kDefaultUniformColor;
    const auto loadouts = infantry::infantryLoadouts();
    if (!loadouts.empty()) {
        request.loadout_id = loadouts[loadout_index_ % loadouts.size()].id;
    }
    const auto compiled = model_compiler_.compile(request);
    if (!compiled) {
        last_generation_error_ = compiled.error();
        markDirty(UnitLabDirtyFlag::Ui);
        return;
    }

    model_artifact_ = compiled.value();
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

void UnitLabScene::on_enter(SceneContext& context) {
    elapsed_seconds_ = 0.0;
    fixed_tick_ = 0;
    fixed_accumulator_ = 0.0F;
    camera_orbit_yaw_ = 0.0F;
    camera_orbit_pitch_ = 0.0F;
    camera_distance_scale_ = 1.0F;
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
    rebuildModel();
    geometry_dirty_ = false;
    material_dirty_ = false;
    context.ui.clear();
}

void UnitLabScene::handle_input(SceneContext& context, const input::InputFrame& input) {
    if (input.mouse_left_pressed && input.mouse_x >= 78.0F && input.mouse_x <= 500.0F) {
        constexpr float first_control_y = 266.0F;
        constexpr float control_step = 60.0F;
        const int control = static_cast<int>((input.mouse_y - first_control_y) / control_step);
        const float local_y = input.mouse_y -
                              (first_control_y + static_cast<float>(control) * control_step);
        if (control >= 0 && control <= 14 && local_y >= 0.0F && local_y <= 48.0F) {
            switch (control) {
            case 0:
                ++preview_seed_;
                rebuildModel();
                break;
            case 1:
                detail_level_ = detail_level_ >= 3U ? 1U : detail_level_ + 1U;
                rebuildModel();
                break;
            case 2:
                camera_mode_ = static_cast<UnitLabCameraMode>(
                    (static_cast<std::uint8_t>(camera_mode_) + 1U) % 5U);
                camera_orbit_yaw_ = 0.0F;
                camera_orbit_pitch_ = 0.0F;
                camera_distance_scale_ = 1.0F;
                markDirty(UnitLabDirtyFlag::Ui);
                break;
            case 3: show_surface_ = !show_surface_; markDirty(UnitLabDirtyFlag::Ui); break;
            case 4: show_wireframe_ = !show_wireframe_; markDirty(UnitLabDirtyFlag::Ui); break;
            case 5: show_skeleton_ = !show_skeleton_; markDirty(UnitLabDirtyFlag::Ui); break;
            case 6: show_bounds_ = !show_bounds_; markDirty(UnitLabDirtyFlag::Ui); break;
            case 7: show_normals_ = !show_normals_; markDirty(UnitLabDirtyFlag::Ui); break;
            case 8: animation_paused_ = !animation_paused_; markDirty(UnitLabDirtyFlag::Pose); break;
            case 9:
                expression_ = static_cast<infantry::FaceExpression>(
                    (static_cast<std::uint8_t>(expression_) + 1U) %
                    infantry::kFaceExpressionCount);
                expression_intensity_ = expression_ == infantry::FaceExpression::Neutral
                    ? 0.0F : 1.0F;
                if (face_animator_) {
                    (void)face_animator_->setExpression(expression_, expression_intensity_);
                }
                markDirty(UnitLabDirtyFlag::Pose);
                break;
            case 10:
                debug_weight_bone_ = debug_weight_bone_
                    ? static_cast<infantry::BoneId>(
                        (static_cast<std::uint16_t>(*debug_weight_bone_) + 1U) %
                        infantry::kRigBoneCount)
                    : infantry::BoneId::Hips;
                markDirty(UnitLabDirtyFlag::Ui);
                break;
            case 11:
                variation_ = variation_ < 1.0F ? 1.0F : variation_ < 1.5F ? 2.0F : 0.5F;
                rebuildModel();
                break;
            case 12:
                if (!infantry::infantryLoadouts().empty()) {
                    loadout_index_ = (loadout_index_ + 1U) % infantry::infantryLoadouts().size();
                    rebuildModel();
                }
                break;
            case 13:
                genome_override_mode_ = static_cast<std::uint8_t>(
                    (genome_override_mode_ + 1U) % 4U);
                genome_overrides_ = {};
                if (genome_override_mode_ == 1U) {
                    genome_overrides_.height = 1.65F;
                } else if (genome_override_mode_ == 2U) {
                    genome_overrides_.height = 1.90F;
                } else if (genome_override_mode_ == 3U) {
                    genome_overrides_.shoulder_width = 1.0F;
                    genome_overrides_.hip_width = 0.0F;
                }
                rebuildModel();
                break;
            case 14:
                context.commands.push({ApplicationCommandKind::ReturnToMainMenu});
                break;
            default: break;
            }
            return;
        }
    }
    if (input.mouse_left_down && input.mouse_x > 570.0F) {
        camera_orbit_yaw_ += input.mouse_delta_x * 0.008F;
        camera_orbit_pitch_ = std::clamp(
            camera_orbit_pitch_ - input.mouse_delta_y * 0.006F, -0.75F, 0.75F);
        markDirty(UnitLabDirtyFlag::Ui);
    }
    if (std::abs(input.mouse_wheel_y) > 0.001F) {
        camera_distance_scale_ = std::clamp(
            camera_distance_scale_ * std::exp(-input.mouse_wheel_y * 0.10F), 0.55F, 1.80F);
        markDirty(UnitLabDirtyFlag::Ui);
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
        if (!infantry::infantryLoadouts().empty()) {
            metrics += " | LOADOUT " + std::string(
                infantry::infantryLoadouts()[loadout_index_ % infantry::infantryLoadouts().size()].identifier);
        }
        metrics += " | EXPRESSION " + std::to_string(static_cast<int>(expression_)) +
                   "@" + std::to_string(expression_intensity_);
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
    context.presentation.clear();
    if (model_artifact_) {
        if (!skinned_prototype_ ||
            skinned_prototype_model_key_ != model_artifact_->cache_key) {
            skinned_prototype_ = infantry_presentation::makePrototype(*model_artifact_);
            skinned_prototype_model_key_ = model_artifact_->cache_key;
        }
        context.presentation.skinned_prototypes.push_back(skinned_prototype_);
        render::SkinnedBonePalette palette{};
        palette.instance_id = foundation::stable_id("unit-lab.infantry.instance");
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
        const float extent = std::max(0.5F,
            model_artifact_->appearance.maximum.y - model_artifact_->appearance.minimum.y);
        foundation::Vec3 camera_offset{0.0F, extent * 0.08F, extent * 2.35F};
        switch (camera_mode_) {
        case UnitLabCameraMode::Front: camera_offset = {0.0F, extent * 0.08F, extent * 2.35F}; break;
        case UnitLabCameraMode::Side: camera_offset = {extent * 2.35F, extent * 0.08F, 0.0F}; break;
        case UnitLabCameraMode::Back: camera_offset = {0.0F, extent * 0.08F, -extent * 2.35F}; break;
        case UnitLabCameraMode::Face: camera_offset = {0.0F, extent * 0.62F, extent * 1.15F}; break;
        case UnitLabCameraMode::ThreeQuarter: break;
        }
        const float horizontal = std::sqrt(camera_offset.x * camera_offset.x +
                                            camera_offset.z * camera_offset.z);
        const float base_angle = std::atan2(camera_offset.x, camera_offset.z);
        const float orbit_angle = base_angle + camera_orbit_yaw_;
        const float pitched_horizontal = horizontal * std::cos(camera_orbit_pitch_);
        camera_offset.x = pitched_horizontal * std::sin(orbit_angle);
        camera_offset.z = pitched_horizontal * std::cos(orbit_angle);
        camera_offset.y = (camera_offset.y - extent * 0.08F) *
                              std::cos(camera_orbit_pitch_) + extent * 0.08F +
                          horizontal * std::sin(camera_orbit_pitch_);
        camera_offset.x *= camera_distance_scale_;
        camera_offset.y = extent * 0.08F +
                          (camera_offset.y - extent * 0.08F) * camera_distance_scale_;
        camera_offset.z *= camera_distance_scale_;
        context.presentation.camera = {
            true, {center.x + camera_offset.x, center.y + camera_offset.y,
                   center.z + camera_offset.z},
            {center.x, center.y + (camera_mode_ == UnitLabCameraMode::Face
                                       ? extent * 0.68F : extent * 0.45F), center.z},
            {0.0F, 1.0F, 0.0F}, 0.72F, 0.05F, 100.0F};
        const float model_rotation = std::sin(static_cast<float>(elapsed_seconds_) * 0.35F) * 0.12F;
        const auto debugPoint = [model_rotation](Vec3 point) noexcept {
            return rotateY(point, model_rotation);
        };
        const auto addDebugLine = [&context, &debugPoint](Vec3 start, Vec3 end,
                                                           foundation::Color color) {
            context.presentation.debug_lines.push_back(
                {debugPoint(start), debugPoint(end), color});
        };
        if (show_bounds_) {
            const Vec3 minimum = model_artifact_->appearance.minimum;
            const Vec3 maximum = model_artifact_->appearance.maximum;
            const Vec3 corners[] = {
                {minimum.x, minimum.y, minimum.z}, {maximum.x, minimum.y, minimum.z},
                {maximum.x, maximum.y, minimum.z}, {minimum.x, maximum.y, minimum.z},
                {minimum.x, minimum.y, maximum.z}, {maximum.x, minimum.y, maximum.z},
                {maximum.x, maximum.y, maximum.z}, {minimum.x, maximum.y, maximum.z}};
            constexpr std::uint32_t edges[][2] = {
                {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 0U},
                {4U, 5U}, {5U, 6U}, {6U, 7U}, {7U, 4U},
                {0U, 4U}, {1U, 5U}, {2U, 6U}, {3U, 7U}};
            for (const auto& edge : edges) {
                addDebugLine(corners[edge[0]], corners[edge[1]],
                             {0.10F, 0.85F, 1.0F, 1.0F});
            }
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
            const auto& vertices = skinned_prototype_->vertices;
            if (show_normals_ || debug_weight_bone_) {
                for (std::size_t index = 0U; index < vertices.size(); index += 8U) {
                    const auto& vertex = vertices[index];
                    foundation::Color color{1.0F, 0.10F, 0.85F, 1.0F};
                    if (debug_weight_bone_) {
                        float weight = 0.0F;
                        const auto selected = static_cast<std::uint16_t>(*debug_weight_bone_);
                        for (std::size_t influence = 0U;
                             influence < vertex.bone_indices.size(); ++influence) {
                            if (vertex.bone_indices[influence] == selected) {
                                weight += vertex.bone_weights[influence];
                            }
                        }
                        const float clamped = std::clamp(weight, 0.0F, 1.0F);
                        color = {clamped, 0.12F + 0.76F * (1.0F - clamped),
                                 1.0F - clamped, 1.0F};
                    }
                    addDebugLine(vertex.position,
                                 add(vertex.position, multiply(vertex.normal, 0.045F)),
                                 color);
                }
            }
            if (show_wireframe_) {
                constexpr std::size_t kMaxDebugLines = 20'000U;
                for (std::size_t index = 0U;
                     index + 2U < skinned_prototype_->indices.size() &&
                     context.presentation.debug_lines.size() < kMaxDebugLines;
                     index += 3U) {
                    const auto vertex = [this](std::uint32_t position) noexcept -> Vec3 {
                        return skinned_prototype_->vertices[position].position;
                    };
                    const Vec3 a = vertex(skinned_prototype_->indices[index]);
                    const Vec3 b = vertex(skinned_prototype_->indices[index + 1U]);
                    const Vec3 c = vertex(skinned_prototype_->indices[index + 2U]);
                    addDebugLine(a, b, {0.15F, 1.0F, 0.35F, 1.0F});
                    addDebugLine(b, c, {0.15F, 1.0F, 0.35F, 1.0F});
                    addDebugLine(c, a, {0.15F, 1.0F, 0.35F, 1.0F});
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
                render::RenderInstanceFlagPreview});
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
