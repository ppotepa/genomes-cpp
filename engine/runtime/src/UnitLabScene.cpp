#include <genomes/runtime/UnitLabScene.hpp>

#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/render/ProceduralMeshes.hpp>
#include <genomes/render/SkinnedDeformer.hpp>

#include <cmath>
#include <algorithm>
#include <string>

namespace genomes::runtime {

namespace {

[[nodiscard]] std::array<float, 16U> transformMatrix(
    const infantry::RigTransform& transform) noexcept {
    const auto& q = transform.rotation;
    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;
    const float xy = q.x * q.y;
    const float xz = q.x * q.z;
    const float yz = q.y * q.z;
    const float wx = q.w * q.x;
    const float wy = q.w * q.y;
    const float wz = q.w * q.z;
    return {
        (1.0F - 2.0F * (yy + zz)) * transform.scale.x,
        (2.0F * (xy + wz)) * transform.scale.x,
        (2.0F * (xz - wy)) * transform.scale.x,
        0.0F,
        (2.0F * (xy - wz)) * transform.scale.y,
        (1.0F - 2.0F * (xx + zz)) * transform.scale.y,
        (2.0F * (yz + wx)) * transform.scale.y,
        0.0F,
        (2.0F * (xz + wy)) * transform.scale.z,
        (2.0F * (yz - wx)) * transform.scale.z,
        (1.0F - 2.0F * (xx + yy)) * transform.scale.z,
        0.0F,
        transform.translation.x,
        transform.translation.y,
        transform.translation.z,
        1.0F};
}

[[nodiscard]] std::array<float, 16U> multiply(
    const std::array<float, 16U>& left,
    const std::array<float, 16U>& right) noexcept {
    std::array<float, 16U> result{};
    for (std::size_t column = 0U; column < 4U; ++column) {
        for (std::size_t row = 0U; row < 4U; ++row) {
            for (std::size_t k = 0U; k < 4U; ++k) {
                result[column * 4U + row] +=
                    left[k * 4U + row] * right[column * 4U + k];
            }
        }
    }
    return result;
}
std::shared_ptr<const render::SkinnedMeshPrototype> makeSkinnedPrototype(
    const infantry::InfantryModelArtifact& model,
    const infantry::FaceOutput* face_output) {
    auto mesh = std::make_shared<render::SkinnedMeshPrototype>();
    mesh->mesh_id = foundation::stable_id("mesh.unit-lab.infantry");
    mesh->revision = model.cache_key;
    const auto append = [&mesh](const infantry::AppearanceMesh& source) {
        const auto base = static_cast<std::uint32_t>(mesh->vertices.size());
        for (const auto& source_vertex : source.vertices) {
            render::SkinnedMeshVertex vertex{};
            vertex.position = source_vertex.position;
            vertex.normal = source_vertex.normal;
            vertex.uv = source_vertex.uv;
            vertex.color = source_vertex.color;
            vertex.material_region = source_vertex.material_region;
            for (std::size_t i = 0; i < source_vertex.influences.size(); ++i) {
                vertex.bone_indices[i] = source_vertex.influences[i].bone_index;
                vertex.bone_weights[i] = source_vertex.influences[i].weight;
            }
            mesh->vertices.push_back(vertex);
        }
        for (const auto index : source.indices) {
            mesh->indices.push_back(base + index);
        }
    };
    append(model.appearance.body);
    append(model.appearance.hair);
    const auto appendGearBox = [&mesh](const infantry::GearPiece& piece) {
        const foundation::Vec3 half{piece.dimensions.x * 0.5F,
                                    piece.dimensions.y * 0.5F,
                                    piece.dimensions.z * 0.5F};
        const foundation::Vec3 c = piece.center;
        constexpr foundation::Vec3 corners[] = {
            {-1.0F, -1.0F, -1.0F}, {1.0F, -1.0F, -1.0F},
            {1.0F, 1.0F, -1.0F},   {-1.0F, 1.0F, -1.0F},
            {-1.0F, -1.0F, 1.0F},  {1.0F, -1.0F, 1.0F},
            {1.0F, 1.0F, 1.0F},    {-1.0F, 1.0F, 1.0F}};
        constexpr std::array<std::array<std::uint32_t, 4U>, 6U> faces{{
            {{0U, 1U, 2U, 3U}}, {{5U, 4U, 7U, 6U}}, {{4U, 0U, 3U, 7U}},
            {{1U, 5U, 6U, 2U}}, {{3U, 2U, 6U, 7U}}, {{4U, 5U, 1U, 0U}}}};
        constexpr foundation::Vec3 normals[] = {
            {0.0F, 0.0F, -1.0F}, {0.0F, 0.0F, 1.0F}, {-1.0F, 0.0F, 0.0F},
            {1.0F, 0.0F, 0.0F},  {0.0F, 1.0F, 0.0F},  {0.0F, -1.0F, 0.0F}};
        constexpr foundation::Vec2 uv[] = {
            {0.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 1.0F}};
        for (std::size_t face = 0U; face < faces.size(); ++face) {
            const std::uint32_t face_base = static_cast<std::uint32_t>(mesh->vertices.size());
            for (std::size_t corner = 0U; corner < 4U; ++corner) {
                const foundation::Vec3 unit = corners[faces[face][corner]];
                render::SkinnedMeshVertex vertex{};
                vertex.position = {c.x + unit.x * half.x, c.y + unit.y * half.y,
                                   c.z + unit.z * half.z};
                vertex.normal = normals[face];
                vertex.uv = uv[corner];
                vertex.color = piece.color;
                vertex.material_region = static_cast<std::uint16_t>(piece.material_region);
                vertex.bone_indices[0] = static_cast<std::uint16_t>(piece.bone);
                vertex.bone_weights[0] = 1.0F;
                mesh->vertices.push_back(vertex);
            }
            mesh->indices.insert(mesh->indices.end(),
                                 {face_base, face_base + 1U, face_base + 2U,
                                  face_base, face_base + 2U, face_base + 3U});
        }
    };
    for (const auto& piece : model.gear.pieces) {
        appendGearBox(piece);
    }
    mesh->morph_target_count = static_cast<std::uint32_t>(
        std::min<std::size_t>(model.appearance.morphs.size(), mesh->morphs.size()));
    if (face_output != nullptr) {
        mesh->morph_weights[0] = face_output->eyelids_close;
        mesh->morph_weights[1] = face_output->eyelids_arc;
        // Morph order is eyelidsClose, eyelidsArc, neckFlex, handsRelax.
        // Jaw rotation is a bone controller, not neckFlex.
        mesh->morph_weights[2] = 0.0F;
        mesh->morph_weights[3] = 0.0F;
    }
    for (std::size_t index = 0; index < mesh->morph_target_count; ++index) {
        mesh->morphs[index].position_deltas = model.appearance.morphs[index].position_deltas;
        mesh->morphs[index].normal_deltas = model.appearance.morphs[index].normal_deltas;
    }
    return mesh;
}
} // namespace

foundation::SceneId UnitLabScene::id() const noexcept {
    return foundation::scene_id("scene.unit-lab");
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
    infantry::InfantryModelRequest request{};
    request.seed = 0x5EED2026ull;
    request.variation = 1.0F;
    request.detail_level = 2U;
    if (auto compiled = model_compiler_.compile(request); compiled) {
        model_artifact_ = std::move(compiled.value());
        if (auto locomotion = infantry::LocomotionController::create(
                model_artifact_->phenotype.body); locomotion) {
            locomotion_ = std::move(locomotion.value());
            locomotion_state_ = locomotion_->initialState();
        }
        if (auto face = infantry::FaceAnimator::create(
                request.seed, model_artifact_->phenotype.face); face) {
            face_animator_ = std::move(face.value());
        }
        if (auto animation = infantry::AnimationSystem::create(1U); animation) {
            animation_system_ = std::move(animation.value());
        }
    }
    context.ui.clear();
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
        if (locomotion_ && locomotion_state_) {
            (void)locomotion_->step(*locomotion_state_, fixed_dt);
        }
        if (animation_system_ && locomotion_ && locomotion_state_ && model_artifact_) {
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
        }
    }
}

void UnitLabScene::frame_update(SceneContext& context, double) {
    context.ui.clear();
    context.ui.add({foundation::stable_id("unit-lab.panel"), ui::UiNodeType::Panel,
                    "UNIT LAB", true, false, 520.0F, 560.0F});
    context.ui.add({foundation::stable_id("unit-lab.title"), ui::UiNodeType::Label,
                    "Procedural infantry prototypes", true, false, 0.0F, 0.0F});
    context.ui.add({foundation::stable_id("unit-lab.description"), ui::UiNodeType::Label,
                    "Native procedural body, face, hair and equipment preview.", true,
                    false, 0.0F, 0.0F});
    std::string metrics = model_artifact_
        ? "MODEL READY | SEED 1592598566 | RIFLEMAN | DETAIL 2"
        : "MODEL COMPILATION FAILED";
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
    }
    context.ui.add({foundation::stable_id("unit-lab.metrics"), ui::UiNodeType::Label,
                    std::move(metrics),
                    true, false, 0.0F, 0.0F});
    context.ui.add({foundation::stable_id("unit-lab.back"), ui::UiNodeType::Button,
                    "Back to main menu", true, true, 420.0F, 48.0F});
}

void UnitLabScene::build_presentation(SceneContext& context) {
    context.presentation.clear();
    if (model_artifact_) {
        const auto skinned = makeSkinnedPrototype(
            *model_artifact_, face_animator_ ? &face_animator_->output() : nullptr);
        context.presentation.skinned_prototypes.push_back(skinned);
        render::SkinnedBonePalette palette{};
        palette.instance_id = foundation::stable_id("unit-lab.infantry.instance");
        palette.matrices.reserve(model_artifact_->skeleton.bones().size());
        std::array<std::array<float, 16U>, infantry::kRigBoneCount> world_matrices{};
        const auto pose_bones = animation_pose_
            ? std::span<const infantry::RigTransform>(animation_pose_->bones)
            : std::span<const infantry::RigTransform>{};
        const auto bones = model_artifact_->skeleton.bones();
        for (std::size_t index = 0U; index < bones.size(); ++index) {
            const auto& bone = bones[index];
            const auto& local = pose_bones.empty() ? bone.local_bind : pose_bones[index];
            const auto local_matrix = transformMatrix(local);
            world_matrices[index] = bone.parent == infantry::kInvalidBoneIndex
                ? local_matrix
                : multiply(world_matrices[bone.parent], local_matrix);
            // Bind-space vertices must remain unchanged in the bind pose. The
            // palette is therefore world pose multiplied by inverse bind,
            // not the local bind transform itself.
            palette.matrices.push_back(multiply(world_matrices[index],
                                                transformMatrix(bone.inverse_bind)));
        }
        context.presentation.skinned_palettes.push_back(std::move(palette));
        auto render_mesh = std::make_shared<render::RenderMesh>(
            render::deformSkinnedCPU(*skinned, context.presentation.skinned_palettes.back().matrices));
        context.presentation.instance_prototypes.push_back(render_mesh);
        context.presentation.instances.push_back({
            foundation::stable_id("unit-lab.infantry.instance"), skinned->mesh_id,
            foundation::stable_id("material.unit-lab.uniform"), {0.0F, 0.0F, 0.0F},
            {1.0F, 1.0F, 1.0F},
            std::sin(static_cast<float>(elapsed_seconds_) * 0.35F) * 0.12F,
            fixed_tick_,
            render::RenderInstanceFlagPreview});
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
