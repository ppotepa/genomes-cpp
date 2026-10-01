#include <genomes/render/MaterialDescriptor.hpp>
#include <genomes/render/SkeletonPrototype.hpp>
#include <genomes/ui/UiTypes.hpp>

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
    assert((!render::MeshMaterialGroup{1U, 5U, 0U}.valid(6U, 1U)));
    assert((!render::MeshMaterialGroup{0U, 6U, 1U}.valid(6U, 1U)));

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

    ui::UiRenderFrame ui_frame{};
    ui_frame.viewport_width = 1280U; ui_frame.viewport_height = 720U;
    ui::UiDrawCommand left{}; left.primitive = ui::UiDrawPrimitive::Quad;
    left.rect = {0.0F, 0.0F, 64.0F, 720.0F}; left.color = {0.1F, 0.2F, 0.3F, 0.9F};
    ui::UiDrawCommand right = left; right.rect = {975.0F, 0.0F, 305.0F, 720.0F};
    ui_frame.commands = {left, right};
    assert(ui::valid_ui_frame(ui_frame));
    auto invalid_ui = ui_frame;
    for (auto& command : invalid_ui.commands) command.color.a = 0.0F;
    assert(!ui::valid_ui_frame(invalid_ui));
    invalid_ui = ui_frame;
    for (auto& command : invalid_ui.commands) command.rect.x = 1400.0F;
    assert(!ui::valid_ui_frame(invalid_ui));
    invalid_ui = ui_frame; invalid_ui.commands = {left};
    invalid_ui.commands.front().rect = {0.0F, 0.0F, 1280.0F, 720.0F};
    invalid_ui.commands.front().color = {1.0F, 1.0F, 1.0F, 1.0F};
    assert(!ui::valid_ui_frame(invalid_ui));
    return 0;
}
