#include <genomes/runtime/UnitLabScene.hpp>

#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/infantry/EquipmentCatalog.hpp>
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
} // namespace

namespace infantry_presentation {

std::shared_ptr<const render::SkinnedMeshPrototype> makePrototype(
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
    const auto tint = [](foundation::Color color, float factor) noexcept {
        return foundation::Color{std::clamp(color.r * factor, 0.0F, 1.0F),
                                 std::clamp(color.g * factor, 0.0F, 1.0F),
                                 std::clamp(color.b * factor, 0.0F, 1.0F), color.a};
    };
    const auto appendBox = [&mesh](foundation::Vec3 center, foundation::Vec3 dimensions,
                                   foundation::Color color, infantry::BoneId bone,
                                   std::uint32_t material_region) {
        const foundation::Vec3 half{dimensions.x * 0.5F, dimensions.y * 0.5F,
                                    dimensions.z * 0.5F};
        const foundation::Vec3 c = center;
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
                vertex.color = color;
                vertex.material_region = static_cast<std::uint16_t>(material_region);
                vertex.bone_indices[0] = static_cast<std::uint16_t>(bone);
                vertex.bone_weights[0] = 1.0F;
                mesh->vertices.push_back(vertex);
            }
            mesh->indices.insert(mesh->indices.end(),
                                 {face_base, face_base + 1U, face_base + 2U,
                                 face_base, face_base + 2U, face_base + 3U});
        }
    };
    const auto appendEllipsoid = [&mesh](foundation::Vec3 center, foundation::Vec3 radii,
                                         foundation::Color color, infantry::BoneId bone,
                                         std::uint32_t material_region) {
        constexpr std::size_t segments = 16U;
        constexpr std::size_t rows = 6U;
        std::vector<std::uint32_t> previous;
        for (std::size_t row = 0U; row <= rows; ++row) {
            const float phi = -1.57079632679F +
                              3.14159265359F * static_cast<float>(row) /
                                  static_cast<float>(rows);
            const float cp = std::cos(phi);
            const float sp = std::sin(phi);
            std::vector<std::uint32_t> ring;
            ring.reserve(segments);
            for (std::size_t segment = 0U; segment < segments; ++segment) {
                const float angle = 6.28318530718F * static_cast<float>(segment) /
                                    static_cast<float>(segments);
                const float ca = std::cos(angle);
                const float sa = std::sin(angle);
                render::SkinnedMeshVertex vertex{};
                vertex.position = {center.x + radii.x * cp * ca,
                                   center.y + radii.y * sp,
                                   center.z + radii.z * cp * sa};
                vertex.normal = {cp * ca, sp, cp * sa};
                vertex.uv = {static_cast<float>(segment) / static_cast<float>(segments),
                             static_cast<float>(row) / static_cast<float>(rows)};
                vertex.color = color;
                vertex.material_region = static_cast<std::uint16_t>(material_region);
                vertex.bone_indices[0] = static_cast<std::uint16_t>(bone);
                vertex.bone_weights[0] = 1.0F;
                ring.push_back(static_cast<std::uint32_t>(mesh->vertices.size()));
                mesh->vertices.push_back(vertex);
            }
            if (!previous.empty()) {
                for (std::size_t segment = 0U; segment < segments; ++segment) {
                    const std::size_t next = (segment + 1U) % segments;
                    mesh->indices.insert(mesh->indices.end(),
                                         {previous[segment], ring[segment], previous[next],
                                          previous[next], ring[segment], ring[next]});
                }
            }
            previous = std::move(ring);
        }
    };
    const auto appendGearBox = [&appendBox, &appendEllipsoid, &tint](
                                  const infantry::GearPiece& piece) {
        const auto* definition = infantry::EquipmentCatalog::findItem(piece.definition_id);
        const std::string_view style = definition == nullptr ? std::string_view{} : definition->style;
        const foundation::Vec3 c = piece.center;
        const foundation::Vec3 d = piece.dimensions;
        const foundation::Color dark = tint(piece.color, 0.58F);
        const foundation::Color edge = tint(piece.color, 0.78F);

        // Base clothing is already part of AppearanceCompiler. Do not cover
        // the articulated jacket, trousers, hands or boots with cubes.
        if (piece.slot != infantry::EquipmentSlot::TorsoBase &&
            piece.slot != infantry::EquipmentSlot::Legs &&
            piece.slot != infantry::EquipmentSlot::Feet &&
            piece.slot != infantry::EquipmentSlot::Hands) {
            appendBox(c, d, piece.color, piece.bone, piece.material_region);
        }
        switch (piece.slot) {
        case infantry::EquipmentSlot::Head:
            appendEllipsoid({c.x, c.y + d.y * 0.10F, c.z},
                            {d.x * 0.60F, d.y * 0.52F, d.z * 0.66F},
                            piece.color, piece.bone, piece.material_region + 100U);
            appendBox({c.x, c.y - d.y * 0.22F, c.z + d.z * 0.05F},
                      {d.x * 1.18F, d.y * 0.10F, d.z * 1.05F}, edge, piece.bone,
                      piece.material_region + 101U);
            break;
        case infantry::EquipmentSlot::TorsoArmor:
            appendBox({c.x, c.y, c.z + d.z * 0.58F}, {d.x * 0.86F, d.y * 0.82F,
                                                       d.z * 0.20F}, edge, piece.bone,
                      piece.material_region + 100U);
            appendBox({c.x - d.x * 0.48F, c.y + d.y * 0.05F, c.z},
                      {d.x * 0.12F, d.y * 0.76F, d.z * 0.72F}, dark, piece.bone,
                      piece.material_region + 101U);
            appendBox({c.x + d.x * 0.48F, c.y + d.y * 0.05F, c.z},
                      {d.x * 0.12F, d.y * 0.76F, d.z * 0.72F}, dark, piece.bone,
                      piece.material_region + 102U);
            break;
        case infantry::EquipmentSlot::ChestRig:
            for (int index = -1; index <= 1; ++index) {
                appendBox({c.x + static_cast<float>(index) * d.x * 0.28F,
                           c.y - d.y * 0.05F, c.z + d.z * 0.57F},
                          {d.x * 0.24F, d.y * 0.52F, d.z * 0.18F}, dark, piece.bone,
                          piece.material_region + 100U + static_cast<std::uint32_t>(index + 1));
                appendBox({c.x + static_cast<float>(index) * d.x * 0.28F,
                           c.y + d.y * 0.23F, c.z + d.z * 0.68F},
                          {d.x * 0.22F, d.y * 0.08F, d.z * 0.04F}, edge, piece.bone,
                          piece.material_region + 104U);
            }
            break;
        case infantry::EquipmentSlot::Back:
            appendEllipsoid({c.x, c.y, c.z - d.z * 0.52F},
                            {d.x * 0.52F, d.y * 0.48F, d.z * 0.58F},
                            piece.color, piece.bone, piece.material_region + 100U);
            appendBox({c.x, c.y + d.y * 0.45F, c.z - d.z * 0.54F},
                      {d.x * 0.72F, d.y * 0.12F, d.z * 0.10F}, edge, piece.bone,
                      piece.material_region + 101U);
            break;
        case infantry::EquipmentSlot::LeftHip:
        case infantry::EquipmentSlot::RightHip:
        case infantry::EquipmentSlot::LeftThigh:
        case infantry::EquipmentSlot::RightThigh:
        case infantry::EquipmentSlot::Utility1:
        case infantry::EquipmentSlot::Utility2:
        case infantry::EquipmentSlot::Utility3:
            appendBox({c.x, c.y + d.y * 0.48F, c.z + d.z * 0.04F},
                      {d.x * 0.86F, d.y * 0.12F, d.z * 0.92F}, edge, piece.bone,
                      piece.material_region + 100U);
            appendBox({c.x, c.y - d.y * 0.18F, c.z + d.z * 0.56F},
                      {d.x * 0.18F, d.y * 0.54F, d.z * 0.08F}, dark, piece.bone,
                      piece.material_region + 101U);
            break;
        case infantry::EquipmentSlot::PrimaryWeapon:
            // The old native path rendered the rifle as one vertical box.
            // Split it into receiver, stock, magazine and barrel so its
            // silhouette reads as a rifle even before weapon animation owns it.
            appendBox({c.x, c.y, c.z - d.z * 0.12F},
                      {d.x * 0.72F, d.y * 0.72F, d.z * 0.42F}, dark, piece.bone,
                      piece.material_region + 100U);
            appendBox({c.x, c.y - d.y * 0.70F, c.z - d.z * 0.02F},
                      {d.x * 0.38F, d.y * 0.62F, d.z * 0.22F}, edge, piece.bone,
                      piece.material_region + 101U);
            appendBox({c.x, c.y + d.y * 0.02F, c.z + d.z * 0.42F},
                      {d.x * 0.30F, d.y * 0.30F, d.z * 0.70F}, edge, piece.bone,
                      piece.material_region + 102U);
            appendBox({c.x, c.y + d.y * 0.04F, c.z - d.z * 0.46F},
                      {d.x * 0.48F, d.y * 0.42F, d.z * 0.28F}, dark, piece.bone,
                      piece.material_region + 103U);
            break;
        case infantry::EquipmentSlot::SecondaryWeapon:
        case infantry::EquipmentSlot::MeleeWeapon:
        case infantry::EquipmentSlot::Throwable:
            appendEllipsoid(c, {d.x * 0.52F, d.y * 0.48F, d.z * 0.48F},
                            style == "grenade" ? edge : dark, piece.bone,
                            piece.material_region + 100U);
            break;
        default:
            break;
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

std::vector<std::array<float, 16U>> makePalette(
    const infantry::SkeletonData& skeleton,
    std::span<const infantry::RigTransform> pose_bones) {
    const auto bones = skeleton.bones();
    std::vector<std::array<float, 16U>> palette;
    palette.reserve(bones.size());
    std::array<std::array<float, 16U>, infantry::kRigBoneCount> world_matrices{};
    for (std::size_t index = 0U; index < bones.size(); ++index) {
        const auto& bone = bones[index];
        const auto& local = pose_bones.empty() ? bone.local_bind : pose_bones[index];
        const auto local_matrix = transformMatrix(local);
        world_matrices[index] = bone.parent == infantry::kInvalidBoneIndex
            ? local_matrix
            : multiply(world_matrices[bone.parent], local_matrix);
        palette.push_back(multiply(world_matrices[index], transformMatrix(bone.inverse_bind)));
    }
    return palette;
}

std::vector<std::array<float, 16U>> makeBindPalette(const infantry::SkeletonData& skeleton) {
    return makePalette(skeleton, {});
}

} // namespace infantry_presentation

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
        const auto skinned = infantry_presentation::makePrototype(
            *model_artifact_, face_animator_ ? &face_animator_->output() : nullptr);
        context.presentation.skinned_prototypes.push_back(skinned);
        render::SkinnedBonePalette palette{};
        palette.instance_id = foundation::stable_id("unit-lab.infantry.instance");
        const auto pose_bones = animation_pose_
            ? std::span<const infantry::RigTransform>(animation_pose_->bones)
            : std::span<const infantry::RigTransform>{};
        palette.matrices = infantry_presentation::makePalette(model_artifact_->skeleton,
                                                               pose_bones);
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
