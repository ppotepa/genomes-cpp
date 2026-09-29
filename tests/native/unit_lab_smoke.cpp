#include <genomes/render/NullRenderer.hpp>
#include <genomes/infantry/InfantryMaterials.hpp>
#include <genomes/runtime/SceneDirector.hpp>
#include <genomes/runtime/UnitLabScene.hpp>

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <thread>

int main() {
    genomes::render::NullRenderer renderer;
    genomes::ui::UiRuntime ui;
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
    const auto& prototype = *presentation.skinned_prototypes.front();
    assert(prototype.indices.size() % 3U == 0U);
    for (std::size_t offset = 0U; offset < prototype.indices.size(); offset += 3U) {
        const auto i0 = prototype.indices[offset];
        const auto i1 = prototype.indices[offset + 1U];
        const auto i2 = prototype.indices[offset + 2U];
        assert(i0 < prototype.vertices.size() && i1 < prototype.vertices.size() &&
               i2 < prototype.vertices.size());
        const auto& p0 = prototype.vertices[i0].position;
        const auto& p1 = prototype.vertices[i1].position;
        const auto& p2 = prototype.vertices[i2].position;
        const genomes::foundation::Vec3 a{p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
        const genomes::foundation::Vec3 b{p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};
        const genomes::foundation::Vec3 n{a.y * b.z - a.z * b.y,
                                          a.z * b.x - a.x * b.z,
                                          a.x * b.y - a.y * b.x};
        assert(std::isfinite(n.x) && std::isfinite(n.y) && std::isfinite(n.z));
        assert(n.x * n.x + n.y * n.y + n.z * n.z > 1.0e-12F);
    }
    assert(presentation.skinned_palettes.front().matrices.size() == 69U);
    assert(presentation.skinned_prototypes.front()->morph_target_count == 4U);
    for (std::size_t morph = 0U; morph < 4U; ++morph) {
        assert(presentation.skinned_prototypes.front()->morphs[morph].position_deltas.size() ==
               presentation.skinned_prototypes.front()->vertices.size());
        assert(presentation.skinned_prototypes.front()->morphs[morph].normal_deltas.size() ==
               presentation.skinned_prototypes.front()->vertices.size());
    }
    assert(presentation.instance_prototypes.size() == 1U);
    assert(!presentation.instance_prototypes.front()->vertices.empty());
    assert(presentation.camera.valid());
    const auto stable_prototype = presentation.skinned_prototypes.front();
    bool has_explicit_uniform_color = false;
    for (const auto& vertex : stable_prototype->vertices) {
        if (vertex.color.r < 0.95F || vertex.color.g < 0.95F || vertex.color.b < 0.95F) {
            has_explicit_uniform_color = true;
            break;
        }
    }
    assert(has_explicit_uniform_color);
    bool has_equipment_material = false;
    for (const auto& vertex : stable_prototype->vertices) {
        if (vertex.material_region == static_cast<std::uint16_t>(
                genomes::infantry::AppearanceMaterialRegion::EquipmentMetal) ||
            vertex.material_region == static_cast<std::uint16_t>(
                genomes::infantry::AppearanceMaterialRegion::EquipmentPaint)) {
            has_equipment_material = true;
            break;
        }
    }
    assert(has_equipment_material);

    director.handle_input({.mouse_left_pressed = true, .mouse_x = 100.0F,
                           .mouse_y = 386.0F, .events = {}}); // camera preset
    director.frame_update(1.0 / 60.0);
    assert(presentation.skinned_prototypes.front() == stable_prototype);

    director.handle_input({.mouse_left_pressed = true, .mouse_x = 100.0F,
                           .mouse_y = 276.0F, .events = {}}); // regenerate
    director.frame_update(1.0 / 60.0);
    assert(presentation.skinned_prototypes.front() != stable_prototype);
    const auto regenerated_prototype = presentation.skinned_prototypes.front();

    director.handle_input({.right_pressed = true, .events = {}});
    for (int tick = 0; tick < 30; ++tick) {
        director.fixed_update(1.0 / 60.0);
    }
    director.frame_update(1.0 / 60.0);
    assert(presentation.skinned_prototypes.front() == regenerated_prototype);
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

    genomes::runtime::SceneCommandQueue gpu_commands;
    genomes::ui::UiRuntime gpu_ui;
    genomes::render::PresentationSnapshot gpu_presentation;
    genomes::runtime::SceneContext gpu_context{gpu_commands, gpu_ui, gpu_presentation};
    gpu_context.render_capabilities.gpu_skinning = true;
    genomes::runtime::UnitLabScene gpu_scene;
    gpu_scene.on_enter(gpu_context);
    gpu_scene.build_presentation(gpu_context);
    assert(gpu_presentation.skinned_prototypes.size() == 1U);
    assert(gpu_presentation.skinned_palettes.size() == 1U);
    assert(gpu_presentation.instance_prototypes.empty());
    const auto initial_camera_position = gpu_presentation.camera.position;
    gpu_scene.handle_input(gpu_context, {.mouse_left_pressed = true,
                                         .mouse_x = 100.0F, .mouse_y = 506.0F,
                                         .events = {}});
    gpu_scene.handle_input(gpu_context, {.mouse_left_pressed = true,
                                         .mouse_x = 100.0F, .mouse_y = 566.0F,
                                         .events = {}});
    gpu_scene.handle_input(gpu_context, {.mouse_left_pressed = true,
                                         .mouse_x = 100.0F, .mouse_y = 626.0F,
                                         .events = {}});
    gpu_scene.handle_input(gpu_context, {.mouse_left_pressed = true,
                                         .mouse_x = 100.0F, .mouse_y = 686.0F,
                                         .events = {}});
    gpu_scene.handle_input(gpu_context, {.mouse_left_pressed = true,
                                         .mouse_x = 100.0F, .mouse_y = 866.0F,
                                         .events = {}});
    gpu_scene.handle_input(gpu_context, {.mouse_left_down = true,
                                         .mouse_x = 700.0F, .mouse_delta_x = 22.0F,
                                         .mouse_delta_y = -8.0F, .mouse_wheel_y = 1.0F,
                                         .events = {}});
    gpu_scene.handle_input(gpu_context, {.mouse_left_pressed = true,
                                         .mouse_x = 100.0F, .mouse_y = 926.0F,
                                         .events = {}});
    gpu_scene.handle_input(gpu_context, {.mouse_left_pressed = true,
                                         .mouse_x = 100.0F, .mouse_y = 986.0F,
                                         .events = {}});
    gpu_scene.handle_input(gpu_context, {.mouse_left_pressed = true,
                                         .mouse_x = 100.0F, .mouse_y = 1046.0F,
                                         .events = {}});
    gpu_scene.build_presentation(gpu_context);
    assert(!gpu_presentation.debug_lines.empty());
    assert(std::abs(gpu_presentation.camera.position.x - initial_camera_position.x) > 0.0001F ||
           std::abs(gpu_presentation.camera.position.y - initial_camera_position.y) > 0.0001F ||
           std::abs(gpu_presentation.camera.position.z - initial_camera_position.z) > 0.0001F);

    // The production scene path is asynchronous when a JobSystem is supplied.
    // Rapid requests invalidate the older compiler revision; only the newest
    // completed model may reach the presentation snapshot.
    genomes::jobs::JobSystem async_jobs{2U};
    genomes::render::NullRenderer async_renderer;
    genomes::ui::UiRuntime async_ui;
    genomes::render::PresentationSnapshot async_presentation;
    genomes::runtime::SceneDirector async_director(async_renderer, async_ui,
                                                    async_presentation, &async_jobs);
    async_director.register_scene(unit_lab_id, [] {
        return std::make_unique<genomes::runtime::UnitLabScene>();
    });
    assert(async_director.start(unit_lab_id));
    for (int frame = 0; frame < 5 && async_presentation.skinned_prototypes.empty();
         ++frame) {
        async_director.frame_update(1.0 / 60.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::this_thread::yield();
    }
    assert(async_presentation.skinned_prototypes.size() == 1U);
    const auto async_initial = async_presentation.skinned_prototypes.front();
    async_director.handle_input({.mouse_left_pressed = true, .mouse_x = 100.0F,
                                 .mouse_y = 276.0F, .events = {}});
    async_director.handle_input({.mouse_left_pressed = true, .mouse_x = 100.0F,
                                 .mouse_y = 276.0F, .events = {}});
    for (int frame = 0; frame < 5 &&
         !async_presentation.skinned_prototypes.empty() &&
         async_presentation.skinned_prototypes.front() == async_initial; ++frame) {
        async_director.frame_update(1.0 / 60.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::this_thread::yield();
    }
    assert(!async_presentation.skinned_prototypes.empty());
    assert(async_presentation.skinned_prototypes.front() != async_initial);
    return 0;
}
