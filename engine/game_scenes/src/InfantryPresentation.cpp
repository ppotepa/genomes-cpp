#include <genomes/game_scenes/InfantryPresentation.hpp>
#include <genomes/infantry/AppearanceCatalog.hpp>

#include <genomes/infantry/GearSurfaceGenerator.hpp>
#include <genomes/infantry/EquipmentCatalog.hpp>
#include <genomes/infantry/InfantryMaterials.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/proc/ProceduralRuntime.hpp>
#include <genomes/render/SkinnedMeshOptimizer.hpp>
#include <genomes/weapons/WeaponProcedural.hpp>
#include <genomes/weapons/WeaponCatalog.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace genomes::game_scenes::infantry_presentation {

namespace {

std::mutex prototype_cache_mutex;
std::unordered_map<foundation::StableId,
                   std::weak_ptr<const render::SkinnedMeshPrototype>> prototype_cache;

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

[[nodiscard]] render::SkinnedBoneTransform presentationTransform(
    const infantry::RigTransform& source) noexcept {
    render::SkinnedBoneTransform result{};
    result.translation = source.translation;
    result.rotation = {source.rotation.x, source.rotation.y, source.rotation.z, source.rotation.w};
    result.scale = source.scale;
    return result;
}

[[nodiscard]] render::MaterialDescriptor materialForRegion(std::uint16_t region) noexcept {
    using infantry::AppearanceMaterialRegion;
    render::MaterialDescriptor material{};
    material.material_id = foundation::stableHashCombine(
        foundation::stable_id("material.infantry.region"), region);
    material.revision = 1U;
    material.base_color = {1.0F, 1.0F, 1.0F, 1.0F};
    material.roughness = 0.82F;
    material.vertex_color = true;
    switch (static_cast<AppearanceMaterialRegion>(region)) {
    case AppearanceMaterialRegion::Skin:
    case AppearanceMaterialRegion::Eyelid:
    case AppearanceMaterialRegion::Ear:
    case AppearanceMaterialRegion::Nose:
    case AppearanceMaterialRegion::Lip:
    case AppearanceMaterialRegion::SkinHand:
        material.roughness = 0.68F; break;
    case AppearanceMaterialRegion::Hair:
        material.roughness = 0.55F; material.double_sided = true; break;
    case AppearanceMaterialRegion::EyeSclera:
    case AppearanceMaterialRegion::Iris:
    case AppearanceMaterialRegion::Pupil:
        material.roughness = 0.22F; break;
    case AppearanceMaterialRegion::BootLeather:
    case AppearanceMaterialRegion::EquipmentLeather:
        material.roughness = 0.46F; break;
    case AppearanceMaterialRegion::EquipmentMetal:
        material.roughness = 0.28F; material.metalness = 0.72F; break;
    case AppearanceMaterialRegion::EquipmentPaint:
        material.roughness = 0.52F; material.metalness = 0.08F;
        material.instance_tint = true; break;
    case AppearanceMaterialRegion::UniformCloth:
    case AppearanceMaterialRegion::Trousers:
    case AppearanceMaterialRegion::EquipmentCloth:
        material.instance_tint = true; break;
    case AppearanceMaterialRegion::Mouth:
        material.roughness = 0.58F; break;
    }
    return material;
}

[[nodiscard]] std::uint16_t triangleMaterial(const render::SkinnedMeshPrototype& mesh,
                                             std::size_t offset) noexcept {
    const auto a = mesh.vertices[mesh.indices[offset]].material_region;
    const auto b = mesh.vertices[mesh.indices[offset + 1U]].material_region;
    const auto c = mesh.vertices[mesh.indices[offset + 2U]].material_region;
    if (a == b || a == c) return a;
    if (b == c) return b;
    return a;
}

void buildMaterialGroups(render::SkinnedMeshPrototype& mesh) {
    constexpr std::uint16_t count =
        static_cast<std::uint16_t>(infantry::AppearanceMaterialRegion::EquipmentPaint) + 1U;
    mesh.materials.clear();
    mesh.material_groups.clear();
    mesh.materials.reserve(count);
    for (std::uint16_t region = 0U; region < count; ++region)
        mesh.materials.push_back(materialForRegion(region));
    if (mesh.indices.size() < 3U) return;
    std::size_t run_start = 0U;
    std::uint16_t current = std::min<std::uint16_t>(triangleMaterial(mesh, 0U), count - 1U);
    for (std::size_t offset = 3U; offset + 2U < mesh.indices.size(); offset += 3U) {
        const auto next = std::min<std::uint16_t>(triangleMaterial(mesh, offset), count - 1U);
        if (next == current) continue;
        mesh.material_groups.push_back({static_cast<std::uint32_t>(run_start),
                                        static_cast<std::uint32_t>(offset - run_start),
                                        current});
        run_start = offset;
        current = next;
    }
    mesh.material_groups.push_back({static_cast<std::uint32_t>(run_start),
                                    static_cast<std::uint32_t>(mesh.indices.size() - run_start),
                                    current});
}

[[nodiscard]] std::array<float, 16U> multiply(
    const std::array<float, 16U>& left,
    const std::array<float, 16U>& right) noexcept {
    std::array<float, 16U> result{};
    for (std::size_t column = 0U; column < 4U; ++column) {
        for (std::size_t row = 0U; row < 4U; ++row) {
            for (std::size_t inner = 0U; inner < 4U; ++inner) {
                result[column * 4U + row] +=
                    left[inner * 4U + row] * right[column * 4U + inner];
            }
        }
    }
    return result;
}

} // namespace

void appendWeapon(render::SkinnedMeshPrototype&,
                  const infantry::InfantryModelArtifact&,
                  proc::ProceduralRuntime*, WeaponPoseAttachment);

std::shared_ptr<const render::SkinnedMeshPrototype> makePrototype(
    const infantry::InfantryModelArtifact& model, PrototypePreparation preparation,
    proc::ProceduralRuntime* procedural_runtime, WeaponPoseAttachment weapon_attachment) {
    const std::uint64_t optimizer_fingerprint =
        preparation == PrototypePreparation::OptimizeDrawOrder
            ? geometry::indexOptimizerFingerprint() : 0U;
    auto cache_key = optimizer_fingerprint == 0U ? model.cache_key :
        foundation::stableHashCombine(model.cache_key, optimizer_fingerprint);
    if (weapon_attachment == WeaponPoseAttachment::RightHand) {
        cache_key = foundation::stableHashCombine(
            cache_key, foundation::stable_id("infantry.weapon-attachment.right-hand"));
    }
    {
        std::scoped_lock cache_lock(prototype_cache_mutex);
        if (const auto found = prototype_cache.find(cache_key); found != prototype_cache.end()) {
            if (auto existing = found->second.lock()) return existing;
            prototype_cache.erase(found);
        }
    }
    // Expensive generation/preparation never holds the shared cache lock.
    // Concurrent equivalent candidates are reconciled at publication below.
    auto mesh = std::make_shared<render::SkinnedMeshPrototype>();
    mesh->mesh_id = foundation::stableHashCombine(
        foundation::stable_id("mesh.infantry.prototype"), cache_key);
    mesh->revision = model.cache_key;
    const auto skeleton_bones = model.skeleton.bones();
    mesh->bones.reserve(skeleton_bones.size());
    for (const auto& bone : skeleton_bones) {
        mesh->bones.push_back({bone.parent, presentationTransform(bone.local_bind)});
    }
    auto skeleton = std::make_shared<render::RenderSkeletonPrototype>();
    skeleton->skeleton_id = model.skeleton.cacheKey();
    skeleton->revision = model.skeleton.cacheKey();
    skeleton->bones.reserve(skeleton_bones.size());
    for (const auto& bone : skeleton_bones) {
        skeleton->bones.push_back({bone.parent,
                                   foundation::stable_id(bone.name),
                                   {bone.local_bind.translation,
                                    {bone.local_bind.rotation.x, bone.local_bind.rotation.y,
                                     bone.local_bind.rotation.z, bone.local_bind.rotation.w},
                                    bone.local_bind.scale},
                                   transformMatrix(bone.inverse_bind)});
    }
    mesh->skeleton = std::move(skeleton);
    const auto append = [&mesh](const infantry::AppearanceMesh& source) {
        const auto base = static_cast<std::uint32_t>(mesh->vertices.size());
        for (const auto& source_vertex : source.vertices) {
            render::SkinnedMeshVertex vertex{};
            vertex.position = source_vertex.position;
            vertex.normal = source_vertex.normal;
            vertex.uv = source_vertex.uv;
            vertex.color = source_vertex.color;
            vertex.material_region = source_vertex.material_region;
            for (std::size_t i = 0U; i < source_vertex.influences.size(); ++i) {
                vertex.bone_indices[i] = source_vertex.influences[i].bone_index;
                vertex.bone_weights[i] = source_vertex.influences[i].weight;
            }
            mesh->vertices.push_back(vertex);
        }
        for (const auto index : source.indices) mesh->indices.push_back(base + index);
    };
    append(model.appearance.body);
    append(model.appearance.hair);
    if (const auto gear_surface = infantry::GearSurfaceGenerator::build(model.gear); gear_surface) {
        append(gear_surface.value());
    }
    appendWeapon(*mesh, model, procedural_runtime, weapon_attachment);

    mesh->morph_target_count = static_cast<std::uint32_t>(
        std::min<std::size_t>(model.appearance.morphs.size(), mesh->morphs.size()));
    const std::size_t body_vertex_count = model.appearance.body.vertices.size();
    for (std::size_t index = 0U; index < mesh->morph_target_count; ++index) {
        mesh->morphs[index].position_deltas = model.appearance.morphs[index].position_deltas;
        mesh->morphs[index].normal_deltas = model.appearance.morphs[index].normal_deltas;
        if (mesh->morphs[index].position_deltas.size() == body_vertex_count) {
            mesh->morphs[index].position_deltas.resize(mesh->vertices.size(), {});
            mesh->morphs[index].normal_deltas.resize(mesh->vertices.size(), {});
        }
    }
    buildMaterialGroups(*mesh);

    float conservative_radius = 0.0F;
    for (std::size_t vertex = 0U; vertex < mesh->vertices.size(); ++vertex) {
        const auto& position = mesh->vertices[vertex].position;
        const float base = std::sqrt(position.x * position.x +
                                     position.y * position.y +
                                     position.z * position.z);
        float morph_budget = 0.0F;
        for (std::size_t morph = 0U; morph < mesh->morph_target_count; ++morph) {
            if (vertex >= mesh->morphs[morph].position_deltas.size()) continue;
            const auto& delta = mesh->morphs[morph].position_deltas[vertex];
            morph_budget += std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        }
        conservative_radius = std::max(conservative_radius, base + morph_budget);
    }
    conservative_radius += std::max(0.15F, model.phenotype.body.height * 0.12F);
    mesh->conservative_bounds_center = {};
    mesh->conservative_bounds_radius = conservative_radius;

    if (optimizer_fingerprint != 0U) {
        const auto optimized = render::optimizeSkinnedDrawOrder(*mesh);
        if (!optimized) {
            // Optional performance preparation may fall back to the CURRENT
            // unmodified mesh, never to an old model or partially written IB.
            // Do not cache a rejected preparation under the optimized identity.
            std::clog << "Infantry index optimization skipped for model " << model.cache_key
                      << ": " << optimized.error().message << '\n';
            return makePrototype(model, PrototypePreparation::ReferenceOrder, procedural_runtime);
        }
    }
    {
        std::scoped_lock cache_lock(prototype_cache_mutex);
        auto& entry = prototype_cache[cache_key];
        if (auto existing = entry.lock()) return existing;
        entry = mesh;
        if (prototype_cache.size() > 512U) {
            for (auto it = prototype_cache.begin(); it != prototype_cache.end();) {
                if (it->second.expired()) it = prototype_cache.erase(it);
                else ++it;
            }
        }
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

std::shared_ptr<const render::SkinnedMeshPrototype> makeMaterialVariant(
    const render::SkinnedMeshPrototype& prototype,
    foundation::StableId appearance_preset,
    const infantry::FrozenAppearanceCatalog& catalog) {
    const auto* definition = catalog.find(appearance_preset);
    if (definition == nullptr || !catalog.frozen()) {
        return {};
    }
    auto variant = std::make_shared<render::SkinnedMeshPrototype>(prototype);
    variant->revision = foundation::stableHashCombine(prototype.revision, appearance_preset);
    variant->mesh_id = foundation::stableHashCombine(prototype.mesh_id, appearance_preset);
    for (auto& vertex : variant->vertices) {
        if (vertex.material_region == static_cast<std::uint16_t>(
                definition->material_region)) {
            vertex.color = definition->color;
        }
    }
    return variant;
}

[[nodiscard]] foundation::Vec3 rotateQuaternion(
    foundation::Vec3 value, const infantry::RigQuaternion& quaternion) noexcept {
    const foundation::Vec3 q{quaternion.x, quaternion.y, quaternion.z};
    const foundation::Vec3 uv{
        q.y * value.z - q.z * value.y,
        q.z * value.x - q.x * value.z,
        q.x * value.y - q.y * value.x};
    const foundation::Vec3 uuv{
        q.y * uv.z - q.z * uv.y,
        q.z * uv.x - q.x * uv.z,
        q.x * uv.y - q.y * uv.x};
    return {value.x + 2.0F * (quaternion.w * uv.x + uuv.x),
            value.y + 2.0F * (quaternion.w * uv.y + uuv.y),
            value.z + 2.0F * (quaternion.w * uv.z + uuv.z)};
}

void appendWeapon(render::SkinnedMeshPrototype& mesh,
                  const infantry::InfantryModelArtifact& model,
                  proc::ProceduralRuntime* procedural_runtime,
                  WeaponPoseAttachment attachment) {
    const auto* equipment = model.gear.equipment.item(infantry::EquipmentSlot::PrimaryWeapon);
    if (equipment == nullptr) return;
    const auto* item = infantry::EquipmentCatalog::findItem(equipment->definition_id);
    if (item == nullptr) return;
    const auto* definition = weapons::WeaponCatalog::find(item->identifier);
    if (definition == nullptr) return;
    const weapons::WeaponVariant variant{model.gear.equipment.equipment_seed,
                                         equipment->variant.size,
                                         model.gear.wear,
                                         static_cast<std::uint32_t>(model.gear.detail_level)};
    foundation::Result<weapons::WeaponArtifact, foundation::Error> built =
        weapons::WeaponGeometryGenerator::build(*definition, variant);
    if (procedural_runtime != nullptr &&
        procedural_runtime->registry().find(proc::generatorId("weapons.artifact")) != nullptr) {
        proc::GenerationRequest<weapons::WeaponGenerationRequest, weapons::WeaponArtifact>
            request;
        request.generator = proc::generatorId("weapons.artifact");
        request.input = std::make_shared<const weapons::WeaponGenerationRequest>(
            weapons::WeaponGenerationRequest{*definition, variant});
        request.seed_path = proc::SeedPath(variant.seed);
        request.options.input_hash = foundation::stableHashCombine(
            static_cast<std::uint64_t>(definition->id), variant.seed);
        request.options.retained_bytes = sizeof(weapons::WeaponArtifact);
        const auto generated = procedural_runtime->generateInline(request);
        if (generated) {
            built = foundation::Result<weapons::WeaponArtifact, foundation::Error>::success(
                *generated.value());
        } else {
            built = foundation::Result<weapons::WeaponArtifact, foundation::Error>::failure(
                generated.error());
        }
    }
    if (!built || built.value().mesh.vertices.empty() || built.value().mesh.indices.empty()) return;
    const bool held = attachment == WeaponPoseAttachment::RightHand;
    const auto& socket = model.gear.fit.socket(infantry::EquipmentSocketId::WeaponBack);
    const infantry::BoneId bone_id = held ? infantry::BoneId::HandR : socket.bone;
    const auto* bone = model.skeleton.find(bone_id);
    if (bone == nullptr) return;
    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto rotateToBack = [](foundation::Vec3 value) noexcept {
        // Match the reference long-weapon stow frame: Euler XYZ(-pi/2, 0, -.24).
        // The barrel points up along the back, not out perpendicular to it.
        const float c = std::cos(.24F), s = std::sin(.24F);
        return foundation::Vec3{c * value.x + s * value.y, value.z,
                                s * value.x - c * value.y};
    };
    const auto material = [](std::uint32_t region) noexcept -> std::uint16_t {
        using infantry::AppearanceMaterialRegion;
        return static_cast<std::uint16_t>(region == 1U
            ? AppearanceMaterialRegion::EquipmentCloth
            : region == 2U ? AppearanceMaterialRegion::EquipmentPaint
                           : AppearanceMaterialRegion::EquipmentMetal);
    };
    mesh.vertices.reserve(mesh.vertices.size() + built.value().mesh.vertices.size());
    for (const auto& source : built.value().mesh.vertices) {
        render::SkinnedMeshVertex vertex{};
        if (held) {
            // Align the weapon's authored grip frame to the right-hand bind
            // frame. The hand overlay then moves the complete weapon with the
            // same animated wrist instead of leaving a second copy on the back.
            const auto& grip_rotation = built.value().primary_grip.local_rotation;
            const infantry::RigQuaternion grip_inverse{
                -grip_rotation[0], -grip_rotation[1], -grip_rotation[2], grip_rotation[3]};
            const auto& hand_rotation = bone->world_bind.rotation;
            const infantry::RigQuaternion attachment_rotation{
                hand_rotation.w * grip_inverse.x + hand_rotation.x * grip_inverse.w +
                    hand_rotation.y * grip_inverse.z - hand_rotation.z * grip_inverse.y,
                hand_rotation.w * grip_inverse.y - hand_rotation.x * grip_inverse.z +
                    hand_rotation.y * grip_inverse.w + hand_rotation.z * grip_inverse.x,
                hand_rotation.w * grip_inverse.z + hand_rotation.x * grip_inverse.y -
                    hand_rotation.y * grip_inverse.x + hand_rotation.z * grip_inverse.w,
                hand_rotation.w * grip_inverse.w - hand_rotation.x * grip_inverse.x -
                    hand_rotation.y * grip_inverse.y - hand_rotation.z * grip_inverse.z};
            vertex.position = bone->world_bind.translation + rotateQuaternion(
                source.position - built.value().primary_grip.local_position,
                attachment_rotation);
            vertex.normal = rotateQuaternion(source.normal, attachment_rotation);
        } else {
            const auto local = rotateQuaternion(
                rotateToBack(source.position), bone->world_bind.rotation);
            // Fit sockets are normalized by body height; weapon geometry is in metres.
            // All skinned vertices remain in model bind space. The palette already
            // applies inverse_bind, so applying it here would subtract the bone twice.
            vertex.position = {
                socket.position.x * model.gear.fit.height + local.x,
                socket.position.y * model.gear.fit.height + local.y,
                socket.position.z * model.gear.fit.height + local.z};
            vertex.normal = rotateQuaternion(
                rotateToBack(source.normal), bone->world_bind.rotation);
        }
        vertex.uv = source.uv;
        vertex.color = source.color;
        vertex.material_region = material(source.material_region);
        vertex.bone_indices[0] = static_cast<std::uint16_t>(infantry::boneIndex(bone_id));
        vertex.bone_weights[0] = 1.0F;
        mesh.vertices.push_back(vertex);
    }
    for (const auto index : built.value().mesh.indices) mesh.indices.push_back(base + index);
}

std::vector<render::SkinnedBoneTransform> makeLocalPoses(
    const infantry::SkeletonData& skeleton,
    std::span<const infantry::RigTransform> pose_bones) {
    const auto bones = skeleton.bones();
    std::vector<render::SkinnedBoneTransform> result;
    result.reserve(bones.size());
    for (std::size_t index = 0U; index < bones.size(); ++index) {
        const auto& local = pose_bones.empty() ? bones[index].local_bind : pose_bones[index];
        result.push_back(presentationTransform(local));
    }
    return result;
}

infantry::AnimationWeaponOverlay copyWeaponPoseTasks(
    const weapons::WeaponPoseTasks& tasks, foundation::Vec3 root_position) noexcept {
    infantry::AnimationWeaponOverlay result{};
    const auto owner = [](weapons::HandOwnership value) noexcept {
        switch (value) {
        case weapons::HandOwnership::Primary:
            return infantry::AnimationHandOwner::Primary;
        case weapons::HandOwnership::Support:
            return infantry::AnimationHandOwner::Support;
        case weapons::HandOwnership::Free:
        default:
            return infantry::AnimationHandOwner::Free;
        }
    };
    result.weapon_id = static_cast<std::uint64_t>(tasks.weapon_id);
    result.primary = {owner(tasks.primary.owner), tasks.primary.target,
                      tasks.primary.weight, tasks.primary.curl, tasks.primary.attached};
    result.support = {owner(tasks.support.owner), tasks.support.target,
                      tasks.support.weight, tasks.support.curl, tasks.support.attached};
    result.root_position = root_position;
    result.aim_direction = tasks.aim_direction;
    result.readiness = tasks.readiness;
    result.recoil = tasks.recoil;
    result.targets_are_world = true;
    return result;
}

} // namespace genomes::game_scenes::infantry_presentation
