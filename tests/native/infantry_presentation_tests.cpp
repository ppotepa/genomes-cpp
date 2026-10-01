#include <genomes/infantry/InfantryModelCompiler.hpp>
#include <genomes/infantry/AppearanceCatalog.hpp>
#include <genomes/infantry/RigSchema.hpp>
#include <genomes/game_scenes/InfantryPresentation.hpp>
#include <genomes/game_scenes/UnitLabScene.hpp>

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
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
    const auto first = runtime::infantry_presentation::makePrototype(*model.value().artifact);
    const auto second = runtime::infantry_presentation::makePrototype(*model.value().artifact);
    assert(first);
    assert(first == second);
    const auto olive = runtime::infantry_presentation::makeMaterialVariant(
        *first, runtime::kInspectionOliveAppearancePreset, appearance_catalog.value());
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

    const auto bind_local = runtime::infantry_presentation::makeLocalPoses(
        model.value().artifact->skeleton, {});
    const auto bind_palette = runtime::infantry_presentation::makeBindPalette(
        model.value().artifact->skeleton);
    assert(bind_local.size() == infantry::kRigBoneCount);
    assert(bind_palette.size() == infantry::kRigBoneCount);

    auto changed = request;
    changed.wear = 0.5;
    const auto changed_model = compiler.compile(changed);
    assert(changed_model);
    const auto changed_prototype =
        runtime::infantry_presentation::makePrototype(*changed_model.value().artifact);
    assert(changed_prototype);
    assert(changed_prototype != first);
    assert(changed_prototype->mesh_id != first->mesh_id);
    return 0;
}
