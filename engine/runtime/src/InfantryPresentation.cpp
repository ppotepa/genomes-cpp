#include <genomes/runtime/InfantryPresentation.hpp>

#include <genomes/infantry/GearSurfaceGenerator.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <utility>

namespace genomes::runtime::infantry_presentation {

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

} // namespace genomes::runtime::infantry_presentation
