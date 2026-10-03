#include <genomes/infantry/InfantryModelCompiler.hpp>
#include <genomes/infantry/AppearanceCatalog.hpp>
#include <genomes/infantry/RigSchema.hpp>
#include <genomes/infantry/EquipmentCatalog.hpp>
#include <genomes/infantry/GearSurfaceGenerator.hpp>
#include <genomes/weapons/WeaponCatalog.hpp>
#include <genomes/render/SkinnedDeformer.hpp>
#include <genomes/game_scenes/InfantryPresentation.hpp>
#include <genomes/game_scenes/UnitLabScene.hpp>

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace {

using Matrix = std::array<float, 16U>;

Matrix localMatrix(const genomes::render::BoneLocalTransform& transform) {
    const auto& q = transform.rotation;
    const float xx=q.x*q.x, yy=q.y*q.y, zz=q.z*q.z;
    const float xy=q.x*q.y, xz=q.x*q.z, yz=q.y*q.z;
    const float wx=q.w*q.x, wy=q.w*q.y, wz=q.w*q.z;
    return {
        (1-2*(yy+zz))*transform.scale.x, (2*(xy+wz))*transform.scale.x,
        (2*(xz-wy))*transform.scale.x, 0,
        (2*(xy-wz))*transform.scale.y, (1-2*(xx+zz))*transform.scale.y,
        (2*(yz+wx))*transform.scale.y, 0,
        (2*(xz+wy))*transform.scale.z, (2*(yz-wx))*transform.scale.z,
        (1-2*(xx+yy))*transform.scale.z, 0,
        transform.translation.x, transform.translation.y, transform.translation.z, 1};
}

Matrix multiply(const Matrix& a, const Matrix& b) {
    Matrix out{};
    for (std::size_t column=0; column<4; ++column)
        for (std::size_t row=0; row<4; ++row)
            for (std::size_t k=0; k<4; ++k)
                out[column*4+row] += a[k*4+row] * b[column*4+k];
    return out;
}

void assertIdentity(const Matrix& matrix) {
    for (std::size_t column=0; column<4; ++column) {
        for (std::size_t row=0; row<4; ++row) {
            const float expected = row == column ? 1.0F : 0.0F;
            assert(std::abs(matrix[column*4+row] - expected) < 2.0e-3F);
        }
    }
}

} // namespace

int main() {
    using namespace genomes;

    const auto appearance_catalog = infantry::loadAppearanceCatalog(
        std::filesystem::path{GENOMES_SOURCE_DIR} / "mods/core/profiles/appearance.json");
    assert(appearance_catalog);
    assert(appearance_catalog.value().frozen());
    const auto* olive_definition = appearance_catalog.value().find(
        infantry::kInspectionOliveAppearancePreset);
    assert(olive_definition != nullptr);
    assert(olive_definition->schema_version == infantry::kAppearancePresetSchemaVersion);
    assert(olive_definition->material_region == infantry::AppearanceMaterialRegion::UniformCloth);

    infantry::InfantryModelCompiler compiler;
    infantry::InfantryModelRequest request{};
    request.seed = 8841U;

    const auto model = compiler.compile(request);
    assert(model);
    const auto first = game_scenes::infantry_presentation::makePrototype(*model.value().artifact);
    const auto second = game_scenes::infantry_presentation::makePrototype(*model.value().artifact);
    assert(first);
    assert(first == second);
    const auto olive = game_scenes::infantry_presentation::makeMaterialVariant(
        *first, game_scenes::kInspectionOliveAppearancePreset, appearance_catalog.value());
    assert(olive && olive != first);
    assert(olive->revision != first->revision);
    assert(olive->vertices.size() == first->vertices.size());
    assert(olive->indices == first->indices);
    assert(olive->bones.size() == first->bones.size());
    assert(first->revision == model.value().artifact->cache_key);
    assert(first->skeleton);
    assert(first->skeleton->valid());
    assert(first->skeleton->skeleton_id == model.value().artifact->skeleton.cacheKey());
    assert(first->skeleton->bones.size() == infantry::kRigBoneCount);
    const auto& artifact = *model.value().artifact;
    const auto gear = infantry::GearSurfaceGenerator::build(artifact.gear);
    assert(gear);
    const auto weapon_begin = artifact.appearance.body.vertices.size() +
        artifact.appearance.hair.vertices.size() + gear.value().vertices.size();
    const auto* equipment = artifact.gear.equipment.item(infantry::EquipmentSlot::PrimaryWeapon);
    assert(equipment);
    const auto* item = infantry::EquipmentCatalog::findItem(equipment->definition_id);
    assert(item);
    const auto* definition = weapons::WeaponCatalog::find(item->identifier);
    assert(definition);
    const auto weapon = weapons::WeaponGeometryGenerator::build(*definition,
        {artifact.gear.equipment.equipment_seed, equipment->variant.size,
         artifact.gear.wear, static_cast<std::uint32_t>(artifact.gear.detail_level)});
    assert(weapon);
    assert(!weapon.value().mesh.vertices.empty());
    assert(first->vertices.size() == weapon_begin + weapon.value().mesh.vertices.size());
    const auto socket = artifact.gear.fit.socket(infantry::EquipmentSocketId::WeaponBack);
    const auto* socket_bone = artifact.skeleton.find(socket.bone);
    assert(socket_bone);
    const auto rotate = [](foundation::Vec3 v, infantry::RigQuaternion q) {
        const foundation::Vec3 t{2 * (q.y*v.z-q.z*v.y), 2 * (q.z*v.x-q.x*v.z),
                                  2 * (q.x*v.y-q.y*v.x)};
        return foundation::Vec3{v.x+q.w*t.x+q.y*t.z-q.z*t.y,
                                v.y+q.w*t.y+q.z*t.x-q.x*t.z,
                                v.z+q.w*t.z+q.x*t.y-q.y*t.x};
    };
    const auto close = [](foundation::Vec3 a, foundation::Vec3 b) {
        assert(std::abs(a.x-b.x) < 2.0e-4F);
        assert(std::abs(a.y-b.y) < 2.0e-4F);
        assert(std::abs(a.z-b.z) < 2.0e-4F);
    };
    const auto bind = game_scenes::infantry_presentation::makeBindPalette(artifact.skeleton);
    const auto deformed = render::deformSkinnedCPU(*first, bind, {});
    for (std::size_t i = 0; i < weapon.value().mesh.vertices.size(); ++i) {
        const auto source = weapon.value().mesh.vertices[i].position;
        const float c = std::cos(.24F), s = std::sin(.24F);
        const auto offset = rotate({c*source.x+s*source.y, source.z, s*source.x-c*source.y},
                                   socket_bone->world_bind.rotation);
        const foundation::Vec3 expected{socket.position.x*artifact.gear.fit.height+offset.x,
            socket.position.y*artifact.gear.fit.height+offset.y,
            socket.position.z*artifact.gear.fit.height+offset.z};
        close(first->vertices[weapon_begin+i].position, expected);
        close(deformed.vertices[weapon_begin+i].position, expected);
        assert(first->vertices[weapon_begin+i].bone_indices[0] == static_cast<std::uint16_t>(socket.bone));
    }
    const auto* hand_bone = artifact.skeleton.find(infantry::BoneId::HandR);
    assert(hand_bone);
    const auto held_prototype = game_scenes::infantry_presentation::makePrototype(
        artifact, game_scenes::infantry_presentation::PrototypePreparation::OptimizeDrawOrder,
        nullptr, game_scenes::infantry_presentation::WeaponPoseAttachment::RightHand);
    assert(held_prototype);
    assert(held_prototype != first);
    assert(held_prototype->mesh_id != first->mesh_id);
    assert(held_prototype->vertices.size() == first->vertices.size());
    const auto held_bind = game_scenes::infantry_presentation::makeBindPalette(artifact.skeleton);
    const auto held_deformed = render::deformSkinnedCPU(*held_prototype, held_bind, {});
    const auto& grip_rotation = weapon.value().primary_grip.local_rotation;
    const infantry::RigQuaternion grip_inverse{
        -grip_rotation[0], -grip_rotation[1], -grip_rotation[2], grip_rotation[3]};
    const auto& hand_rotation = hand_bone->world_bind.rotation;
    const infantry::RigQuaternion attachment_rotation{
        hand_rotation.w * grip_inverse.x + hand_rotation.x * grip_inverse.w +
            hand_rotation.y * grip_inverse.z - hand_rotation.z * grip_inverse.y,
        hand_rotation.w * grip_inverse.y - hand_rotation.x * grip_inverse.z +
            hand_rotation.y * grip_inverse.w + hand_rotation.z * grip_inverse.x,
        hand_rotation.w * grip_inverse.z + hand_rotation.x * grip_inverse.y -
            hand_rotation.y * grip_inverse.x + hand_rotation.z * grip_inverse.w,
        hand_rotation.w * grip_inverse.w - hand_rotation.x * grip_inverse.x -
            hand_rotation.y * grip_inverse.y - hand_rotation.z * grip_inverse.z};
    for (std::size_t i = 0; i < weapon.value().mesh.vertices.size(); ++i) {
        const auto& vertex = held_prototype->vertices[weapon_begin+i];
        const auto expected = hand_bone->world_bind.translation + rotate(
            weapon.value().mesh.vertices[i].position - weapon.value().primary_grip.local_position,
            attachment_rotation);
        close(vertex.position, expected);
        close(held_deformed.vertices[weapon_begin+i].position, expected);
        assert(vertex.bone_indices[0] == static_cast<std::uint16_t>(infantry::BoneId::HandR));
        assert(std::abs(vertex.bone_weights[0] - 1.0F) < 1.0e-6F);
    }
    // Gear shares the body's model-space bind convention too.
    for (std::size_t i = 0; i < first->vertices.size(); ++i)
        close(deformed.vertices[i].position, first->vertices[i].position);
    assert(first->conservative_bounds_radius > model.value().artifact->phenotype.body.height);
    const auto inside_bound = [&](foundation::Vec3 point) {
        const auto center = first->conservative_bounds_center;
        const float x=point.x-center.x,y=point.y-center.y,z=point.z-center.z;
        return x*x+y*y+z*z <=
            first->conservative_bounds_radius*first->conservative_bounds_radius + 1.0e-4F;
    };
    for (const auto& vertex : first->vertices) assert(inside_bound(vertex.position));
    for (std::size_t vertex=0; vertex<first->vertices.size(); ++vertex) {
        foundation::Vec3 combined=first->vertices[vertex].position;
        for (std::size_t morph=0; morph<first->morph_target_count; ++morph) {
            const auto& delta=first->morphs[morph].position_deltas[vertex];
            combined.x+=delta.x;combined.y+=delta.y;combined.z+=delta.z;
        }
        assert(inside_bound(combined));
    }

    std::array<Matrix, infantry::kRigBoneCount> world{};
    for (std::size_t index=0; index<first->skeleton->bones.size(); ++index) {
        const auto& bone = first->skeleton->bones[index];
        const auto local = localMatrix(bone.local_bind);
        world[index] = bone.parent == render::kInvalidRenderBoneIndex
            ? local : multiply(world[bone.parent], local);
        assertIdentity(multiply(world[index], bone.inverse_bind));
    }

    assert(!first->materials.empty());
    std::size_t covered = 0U;
    for (const auto& material : first->materials) assert(material.valid());
    for (const auto& group : first->material_groups) {
        assert(group.valid(first->indices.size(), first->materials.size()));
        assert(group.first_index == covered);
        covered += group.index_count;
    }
    assert(covered == first->indices.size());

    for (std::size_t morph=0; morph<first->morph_target_count; ++morph) {
        assert(first->morphs[morph].position_deltas.size() == first->vertices.size());
        assert(first->morphs[morph].normal_deltas.size() == first->vertices.size());
    }

    const auto bind_local = game_scenes::infantry_presentation::makeLocalPoses(
        model.value().artifact->skeleton, {});
    const auto bind_palette = game_scenes::infantry_presentation::makeBindPalette(
        model.value().artifact->skeleton);
    assert(bind_local.size() == infantry::kRigBoneCount);
    assert(bind_palette.size() == infantry::kRigBoneCount);

    auto changed = request;
    changed.wear = 0.5;
    const auto changed_model = compiler.compile(changed);
    assert(changed_model);
    const auto changed_prototype =
        game_scenes::infantry_presentation::makePrototype(*changed_model.value().artifact);
    assert(changed_prototype);
    assert(changed_prototype != first);
    assert(changed_prototype->mesh_id != first->mesh_id);

    const auto* rifle = weapons::WeaponCatalog::find("rifle");
    assert(rifle != nullptr);
    weapons::WeaponPoseTasks weapon_tasks{};
    weapon_tasks.weapon_id = rifle->id;
    weapon_tasks.primary = {weapons::HandOwnership::Primary, {1.0F, 2.0F, 3.0F},
                            0.75F, 0.80F, true};
    weapon_tasks.support = {weapons::HandOwnership::Support, {4.0F, 5.0F, 6.0F},
                            0.50F, 0.60F, true};
    weapon_tasks.aim_direction = {0.0F, 0.0F, 1.0F};
    weapon_tasks.readiness = 0.75F;
    weapon_tasks.recoil = 0.40F;
    const auto overlay = game_scenes::infantry_presentation::copyWeaponPoseTasks(
        weapon_tasks, {10.0F, 20.0F, 30.0F});
    assert(overlay.valid());
    assert(overlay.weapon_id == static_cast<std::uint64_t>(rifle->id));
    assert(overlay.primary.owner == infantry::AnimationHandOwner::Primary);
    assert(overlay.support.owner == infantry::AnimationHandOwner::Support);
    assert(std::abs(overlay.readiness - weapon_tasks.readiness) < 1.0e-6F);
    assert(std::abs(overlay.recoil - weapon_tasks.recoil) < 1.0e-6F);
    assert(overlay.targets_are_world);
    return 0;
}
