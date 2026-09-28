#include <genomes/render/NullRenderer.hpp>
#include <genomes/runtime/SceneDirector.hpp>
#include <genomes/runtime/UnitLabScene.hpp>

#include <cassert>
#include <cmath>
#include <memory>

int main() {
    genomes::render::NullRenderer renderer;
    genomes::ui::UiDocument ui;
    genomes::render::PresentationSnapshot presentation;
    genomes::runtime::SceneDirector director(renderer, ui, presentation);
    const auto unit_lab_id = genomes::foundation::scene_id("scene.unit-lab");
    director.register_scene(unit_lab_id, [] {
        return std::make_unique<genomes::runtime::UnitLabScene>();
    });

    assert(director.start(unit_lab_id));
    director.frame_update(1.0 / 60.0);
    assert(presentation.skinned_prototypes.size() == 1U);
    assert(presentation.skinned_palettes.size() == 1U);
    assert(presentation.skinned_prototypes.front()->vertices.size() > 100U);
    assert(presentation.skinned_prototypes.front()->indices.size() > 300U);
    assert(presentation.skinned_palettes.front().matrices.size() == 69U);
    assert(presentation.instance_prototypes.size() == 1U);
    assert(!presentation.instance_prototypes.front()->vertices.empty());

    director.handle_input({.right_pressed = true});
    for (int tick = 0; tick < 30; ++tick) {
        director.fixed_update(1.0 / 60.0);
    }
    director.frame_update(1.0 / 60.0);
    const auto& palette = presentation.skinned_palettes.front().matrices;
    bool has_pose_rotation = false;
    for (const auto& matrix : palette) {
        if (std::abs(matrix[1]) > 1.0e-4F || std::abs(matrix[2]) > 1.0e-4F ||
            std::abs(matrix[4]) > 1.0e-4F || std::abs(matrix[6]) > 1.0e-4F) {
            has_pose_rotation = true;
            break;
        }
    }
    assert(has_pose_rotation);
    return 0;
}
