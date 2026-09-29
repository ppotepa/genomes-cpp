#include <genomes/runtime/InfantryPresentation.hpp>

#include <genomes/infantry/GearSurfaceGenerator.hpp>
#include <genomes/infantry/InfantryMaterials.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace genomes::runtime::infantry_presentation {

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

std::shared_ptr<const render::SkinnedMeshPrototype> makePrototype(
    const infantry::InfantryModelArtifact& model) {
    // Geometry is a model prototype: poses, morph weights and chunk transforms
    // live in the per-instance palette records.  Keep one immutable owner per
    // complete compiler key so Unit Lab and Battlefield share the same buffers.
    std::scoped_lock cache_lock(prototype_cache_mutex);
    if (const auto found = prototype_cache.find(model.cache_key);
        found != prototype_cache.end()) {
        if (auto existing = found->second.lock()) {
            return existing;
        }
        prototype_cache.erase(found);
    }
    auto mesh = std::make_shared<render::SkinnedMeshPrototype>();
    mesh->mesh_id = foundation::stableHashCombine(
        foundation::stable_id("mesh.infantry.prototype"), model.cache_key);
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
        for (const auto index : source.indices) {
            mesh->indices.push_back(base + index);
        }
    };
    append(model.appearance.body);
    append(model.appearance.hair);
    if (const auto gear_surface = infantry::GearSurfaceGenerator::build(model.gear);
        gear_surface) {
        append(gear_surface.value());
    }

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
    prototype_cache.emplace(model.cache_key, mesh);
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

} // namespace genomes::runtime::infantry_presentation
