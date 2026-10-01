#include <genomes/render/NullRenderer.hpp>
#include <genomes/infantry/InfantryMaterials.hpp>
#include <genomes/runtime/SceneDirector.hpp>
#include <genomes/runtime/UnitLabScene.hpp>

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <tuple>
#include <string>
#include <variant>

int main() {
    for (const auto& [width, height, scale] : {
            std::tuple{1280, 720, 1.0}, std::tuple{1280, 720, 1.5},
            std::tuple{1920, 1080, 1.0}, std::tuple{1920, 1080, 1.5}}) {
        const auto viewport = genomes::runtime::unitLabViewport(width, height, scale);
        assert(viewport.left == 0.0F && viewport.top == 0.0F);
        assert(viewport.width == 1.0F && viewport.height == 1.0F);
        assert(std::isfinite(viewport.projection_offset_x));
        assert(std::isfinite(viewport.projection_offset_y));
    }
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
    assert(presentation.has_camera_request);
    assert(presentation.has_resolved_camera);
    assert(presentation.resolved_camera.viewport.width > 0 &&
           presentation.resolved_camera.viewport.height > 0);
    const auto initial_scene_epoch = presentation.scene_epoch;
    assert(initial_scene_epoch != 0U);
    const auto stable_prototype = presentation.skinned_prototypes.front();
    const auto home_camera = presentation.camera;
    genomes::input::InputFrame orbit{};
    orbit.viewport_width = 1280;
    orbit.viewport_height = 720;
    orbit.mouse_x = 600.0F;
    orbit.mouse_y = 360.0F;
    orbit.mouse_delta_x = 28.0F;
    orbit.mouse_delta_y = -12.0F;
    orbit.mouse_left_down = true;
    orbit.mouse_left_pressed = true;
    orbit.events.push_back({genomes::input::EventType::MouseButtonDown, 0, 0, 1,
                            orbit.mouse_x, orbit.mouse_y, 0.0F, 0.0F, {}});
    director.handle_input(orbit);
    assert(genomes::math::lengthSquared(presentation.camera.position - home_camera.position) >
           1.0e-8F);
    const auto target_before_pan = presentation.camera.target;
    genomes::input::InputFrame pan{};
    pan.viewport_width = 1280;
    pan.viewport_height = 720;
    pan.mouse_x = 600.0F;
    pan.mouse_y = 360.0F;
    pan.mouse_delta_x = 20.0F;
    pan.mouse_delta_y = 8.0F;
    pan.mouse_right_down = true;
    director.handle_input(pan);
    assert(genomes::math::lengthSquared(presentation.camera.target - target_before_pan) > 1.0e-8F);
    const auto distance_before_zoom = genomes::math::length(
        presentation.camera.position - presentation.camera.target);
    genomes::input::InputFrame zoom{};
    zoom.viewport_width = 1280;
    zoom.viewport_height = 720;
    zoom.mouse_x = 600.0F;
    zoom.mouse_y = 360.0F;
    zoom.mouse_wheel_y = 1.0F;
    director.handle_input(zoom);
    assert(genomes::math::length(presentation.camera.position - presentation.camera.target) <
           distance_before_zoom);
    genomes::input::InputFrame reset{};
    reset.viewport_width = 1280;
    reset.viewport_height = 720;
    reset.reset_pressed = true;
    director.handle_input(reset);
    assert(genomes::math::lengthSquared(presentation.camera.position - home_camera.position) <
           1.0e-8F);
    genomes::input::InputFrame cancel = orbit;
    cancel.cancel_pressed = true;
    cancel.pointer_cancel = true;
    director.handle_input(cancel);
    assert(genomes::math::lengthSquared(presentation.camera.position - home_camera.position) <
           1.0e-8F);
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

    assert(director.dispatch_ui_action(genomes::foundation::stable_id("unit.camera"), {}) ==
           genomes::ui::UiActionResult::Handled);
    director.frame_update(1.0 / 60.0);
    assert(presentation.skinned_prototypes.front() == stable_prototype);

    assert(director.dispatch_ui_action(genomes::foundation::stable_id("unit.regenerate"), {}) ==
           genomes::ui::UiActionResult::Handled);
    director.frame_update(1.0 / 60.0);
    assert(presentation.skinned_prototypes.front() != stable_prototype);
    assert(presentation.scene_epoch == initial_scene_epoch);
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
    gpu_presentation.simulation_tick = 17U;
    gpu_presentation.clear_scene_payload();
    assert(gpu_presentation.simulation_tick == 17U);
    gpu_scene.build_presentation(gpu_context);
    assert(gpu_presentation.skinned_prototypes.size() == 1U);
    assert(gpu_presentation.skinned_palettes.size() == 1U);
    assert(gpu_presentation.instance_prototypes.empty());
    assert(gpu_presentation.camera.mode == genomes::camera::CameraMode::Orbit);
    assert(gpu_presentation.has_camera_request);
    assert(gpu_presentation.camera_request.preset ==
           genomes::camera::CameraPreset::UnitLab);
    const auto expected_viewport = genomes::runtime::unitLabViewport(1280, 720, 1.0);
    assert(std::abs(gpu_presentation.camera.viewport_left - expected_viewport.left) < 1.0e-6F);
    assert(std::abs(gpu_presentation.camera.viewport_width - expected_viewport.width) < 1.0e-6F);
    assert(gpu_presentation.camera.viewport_left == 0.0F);
    assert(gpu_presentation.camera.viewport_top == 0.0F);
    assert(gpu_presentation.camera.viewport_width == 1.0F);
    assert(gpu_presentation.camera.viewport_height == 1.0F);
    assert(std::abs(gpu_presentation.camera.projection_offset_x -
                    expected_viewport.projection_offset_x) < 1.0e-6F);
    gpu_ui.set_viewport_metrics(
        genomes::ui::UiViewportMetrics{68.0F, 104.0F, 872.0F, 582.0F});
    gpu_ui.clear();
    assert(gpu_ui.viewport_metrics());
    gpu_presentation.clear_scene_payload();
    gpu_scene.build_presentation(gpu_context);
    assert(std::abs(gpu_presentation.camera.viewport_left - 68.0F / 1280.0F) < 1.0e-6F);
    assert(std::abs(gpu_presentation.camera.viewport_top - 104.0F / 720.0F) < 1.0e-6F);
    assert(std::abs(gpu_presentation.camera.viewport_width - 872.0F / 1280.0F) < 1.0e-6F);
    assert(std::abs(gpu_presentation.camera.viewport_height - 582.0F / 720.0F) < 1.0e-6F);
    assert(gpu_presentation.camera.projection_offset_x == 0.0F);
    gpu_ui.set_viewport_metrics(std::nullopt);
    gpu_presentation.clear_scene_payload();
    gpu_scene.build_presentation(gpu_context);
    assert(std::abs(gpu_presentation.camera.viewport_left - 68.0F / 1280.0F) < 1.0e-6F);
    const auto composed = genomes::camera::resolve(gpu_presentation.camera.toRequest(), 1280, 720);
    assert(composed);
    const auto subject = genomes::camera::project(composed.value(),
                                                  gpu_presentation.camera.target);
    assert(subject);
    assert(subject.value().x > 68.0F && subject.value().x < 1280.0F - 340.0F);
    assert(subject.value().y > 104.0F && subject.value().y < 720.0F - 34.0F);
    const auto initial_camera_revision = gpu_presentation.camera.revision;
    for (const auto action : {"unit.wireframe", "unit.skeleton", "unit.bounds",
                              "unit.normals", "unit.weight", "unit.variation",
                              "unit.loadout", "unit.genome-preset"}) {
        assert(gpu_scene.handle_ui_action(gpu_context,
                    genomes::foundation::stable_id(action), {}) ==
               genomes::ui::UiActionResult::Handled);
    }
    gpu_presentation.clear_scene_payload();
    gpu_scene.build_presentation(gpu_context);
    assert(!gpu_presentation.debug_lines.empty());
    assert(gpu_presentation.camera.mode == genomes::camera::CameraMode::Orbit);
    assert(gpu_presentation.camera.revision != 0U);
    assert(gpu_presentation.camera.revision != initial_camera_revision ||
           gpu_presentation.camera.viewport_left == expected_viewport.left);

    const auto before_genome = gpu_presentation.skinned_prototypes.front();
    assert(gpu_scene.handle_ui_action(
        gpu_context, genomes::foundation::stable_id("unit.tab"),
        {{"key", "genome"}, {"value", "genome"}}) == genomes::ui::UiActionResult::Handled);
    assert(gpu_scene.handle_ui_action(
        gpu_context, genomes::foundation::stable_id("unit.genome"),
        {{"key", "height"}, {"value", "0.82"}}) == genomes::ui::UiActionResult::Handled);
    assert(gpu_scene.handle_ui_action(
        gpu_context, genomes::foundation::stable_id("unit.genome"),
        {{"key", "height"}, {"value", "nan"}}) == genomes::ui::UiActionResult::Rejected);
    assert(gpu_scene.handle_ui_action(
        gpu_context, genomes::foundation::stable_id("unit.genome-plus"), {}) ==
        genomes::ui::UiActionResult::Handled);
    gpu_presentation.clear_scene_payload();
    gpu_scene.build_presentation(gpu_context);
    assert(gpu_presentation.skinned_prototypes.front() != before_genome);

    const auto before_equipment = gpu_presentation.skinned_prototypes.front();
    assert(gpu_scene.handle_ui_action(
        gpu_context, genomes::foundation::stable_id("unit.equipment-item"),
        {{"key", "head"}, {"value", "none"}}) == genomes::ui::UiActionResult::Handled);
    assert(gpu_scene.handle_ui_action(
        gpu_context, genomes::foundation::stable_id("unit.wear"),
        {{"value", "0.35"}}) == genomes::ui::UiActionResult::Handled);
    assert(gpu_scene.handle_ui_action(
        gpu_context, genomes::foundation::stable_id("unit.phase"),
        {{"value", "0.5"}}) == genomes::ui::UiActionResult::Handled);
    gpu_scene.frame_update(gpu_context, 0.0);
    const auto* paused = gpu_ui.model().find("animation_paused");
    assert(paused != nullptr && std::get<bool>(*paused));
    assert(gpu_scene.handle_ui_action(
        gpu_context, genomes::foundation::stable_id("unit.locomotion"),
        {{"value", "Crouch Walk"}}) == genomes::ui::UiActionResult::Handled);
    assert(gpu_scene.handle_ui_action(
        gpu_context, genomes::foundation::stable_id("unit.variation"),
        {{"value", "1.75"}}) == genomes::ui::UiActionResult::Handled);
    assert(gpu_scene.handle_ui_action(
        gpu_context, genomes::foundation::stable_id("unit.variation"),
        {{"value", "2.0"}}) == genomes::ui::UiActionResult::Rejected);
    assert(gpu_scene.handle_ui_action(
        gpu_context, genomes::foundation::stable_id("unit.weight"),
        {{"value", "68"}}) == genomes::ui::UiActionResult::Handled);
    assert(gpu_scene.handle_ui_action(
        gpu_context, genomes::foundation::stable_id("unit.genome-clear"),
        {{"key", "height"}}) == genomes::ui::UiActionResult::Handled);
    constexpr const char* exact_seed = "9223372036854775808";
    assert(gpu_scene.handle_ui_action(
        gpu_context, genomes::foundation::stable_id("unit.seed"),
        {{"value", exact_seed}}) == genomes::ui::UiActionResult::Handled);
    gpu_scene.frame_update(gpu_context, 0.0);
    const auto* seed_value = gpu_ui.model().find("seed");
    assert(seed_value != nullptr && std::get<std::string>(*seed_value) == exact_seed);
    assert(gpu_scene.handle_ui_action(
        gpu_context, genomes::foundation::stable_id("unit.equipment-item"), {}) ==
        genomes::ui::UiActionResult::Handled);
    gpu_scene.build_presentation(gpu_context);
    assert(gpu_presentation.skinned_prototypes.front() != before_equipment);

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
    assert(async_director.dispatch_ui_action(
               genomes::foundation::stable_id("unit.regenerate"), {}) ==
           genomes::ui::UiActionResult::Handled);
    assert(async_director.dispatch_ui_action(
               genomes::foundation::stable_id("unit.regenerate"), {}) ==
           genomes::ui::UiActionResult::Handled);
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
