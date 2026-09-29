#include <genomes/render/MaterialDescriptor.hpp>
#include <genomes/render/SkeletonPrototype.hpp>

#include <cassert>
#include <limits>

int main() {
    using namespace genomes;

    render::MaterialDescriptor material{};
    assert(!material.valid());
    material.material_id = foundation::stable_id("material.test");
    material.roughness = 0.5F;
    material.metalness = 0.25F;
    assert(material.valid());
    material.roughness = 1.5F;
    assert(!material.valid());

    render::MeshMaterialGroup group{0U, 6U, 0U};
    assert(group.valid(6U, 1U));
    assert(!render::MeshMaterialGroup{1U, 5U, 0U}.valid(6U, 1U));
    assert(!render::MeshMaterialGroup{0U, 6U, 1U}.valid(6U, 1U));

    render::RenderSkeletonPrototype skeleton{};
    skeleton.skeleton_id = foundation::stable_id("skeleton.test");
    render::RenderSkeletonBone root{};
    root.parent = render::kInvalidRenderBoneIndex;
    root.name_id = foundation::stable_id("root");
    root.inverse_bind = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    render::RenderSkeletonBone child = root;
    child.parent = 0U;
    child.name_id = foundation::stable_id("child");
    child.local_bind.translation.y = 1.0F;
    skeleton.bones = {root, child};
    assert(skeleton.valid());

    auto invalid_parent = skeleton;
    invalid_parent.bones[1].parent = 1U;
    assert(!invalid_parent.valid());

    auto invalid_transform = skeleton;
    invalid_transform.bones[1].local_bind.scale.x = 0.0F;
    assert(!invalid_transform.valid());

    auto invalid_matrix = skeleton;
    invalid_matrix.bones[0].inverse_bind[0] =
        std::numeric_limits<float>::quiet_NaN();
    assert(!invalid_matrix.valid());
    return 0;
}
