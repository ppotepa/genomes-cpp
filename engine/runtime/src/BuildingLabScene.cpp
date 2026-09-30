#include <genomes/runtime/BuildingLabScene.hpp>

#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/camera/Camera.hpp>
#include <genomes/render/ProceduralMeshes.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace genomes::runtime {

namespace {

[[nodiscard]] foundation::Color part_color(buildings::BuildingPartKind kind,
                                            float integrity) noexcept {
    foundation::Color color{};
    switch (kind) {
    case buildings::BuildingPartKind::Foundation:
        color = {0.25F, 0.22F, 0.20F, 1.0F};
        break;
    case buildings::BuildingPartKind::Floor:
        color = {0.38F, 0.28F, 0.20F, 1.0F};
        break;
    case buildings::BuildingPartKind::Wall:
        color = {0.66F, 0.38F, 0.22F, 1.0F};
        break;
    case buildings::BuildingPartKind::Roof:
        color = {0.18F, 0.16F, 0.15F, 1.0F};
        break;
    case buildings::BuildingPartKind::Door:
        color = {0.22F, 0.11F, 0.05F, 1.0F};
        break;
    }
    const float damage = std::clamp(integrity, 0.0F, 1.0F);
    const float factor = 0.35F + damage * 0.65F;
    return {color.r * factor, color.g * factor, color.b * factor, color.a};
}

[[nodiscard]] const char* part_name(buildings::BuildingPartKind kind) noexcept {
    switch (kind) {
    case buildings::BuildingPartKind::Foundation:
        return "foundation";
    case buildings::BuildingPartKind::Floor:
        return "floor";
    case buildings::BuildingPartKind::Wall:
        return "wall";
    case buildings::BuildingPartKind::Roof:
        return "roof";
    case buildings::BuildingPartKind::Door:
        return "door";
    }
    return "part";
}

} // namespace

foundation::SceneId BuildingLabScene::id() const noexcept {
    return foundation::scene_id("scene.building-lab");
}

void BuildingLabScene::on_enter(SceneContext& context) {
    elapsed_seconds_ = 0.0;
    selected_part_ = 0;
    error_.clear();
    plan_ = {};
    runtime_.reset();
    render_mesh_.reset();
    const buildings::BuildingSpec spec{
        foundation::stable_id("building-lab.preview"), 0xB01D1A9u, {18.0F, 1.0F, 14.0F},
        2, 3.0F, 0.30F, 3};
    const auto generated = buildings::BuildingGenerator::generate(spec);
    if (!generated) {
        error_ = generated.error().message;
    } else {
        plan_ = generated.value();
        runtime_ = std::make_unique<buildings::BuildingRuntime>(plan_);
        rebuild_mesh();
    }
    context.ui.clear();
}

void BuildingLabScene::on_exit(SceneContext&) {
    runtime_.reset();
    render_mesh_.reset();
    plan_ = {};
    error_.clear();
}

void BuildingLabScene::handle_input(SceneContext& context, const input::InputFrame& input) {
    if (input.cancel_pressed || input.confirm_pressed) {
        context.commands.push({ApplicationCommandKind::ReturnToMainMenu});
        return;
    }
    if (plan_.parts.empty()) {
        return;
    }
    if (input.up_pressed) {
        selected_part_ = (selected_part_ + plan_.parts.size() - 1U) % plan_.parts.size();
    } else if (input.down_pressed) {
        selected_part_ = (selected_part_ + 1U) % plan_.parts.size();
    }
    if (input.left_pressed) {
        apply_selected_damage(0.20F);
    } else if (input.right_pressed) {
        apply_selected_damage(0.45F);
    }
}

void BuildingLabScene::fixed_update(SceneContext&, double dt) {
    elapsed_seconds_ += dt;
}

void BuildingLabScene::frame_update(SceneContext& context, double) {
    context.ui.clear();
    context.ui.add({foundation::stable_id("building-lab.panel"), ui::UiWidgetType::Panel,
                    "BUILDING LAB", true, false, 620.0F, 650.0F});
    context.ui.add({foundation::stable_id("building-lab.title"), ui::UiWidgetType::Label,
                    "Procedural building plan and damage runtime", true, false, 0.0F, 0.0F});
    if (!error_.empty()) {
        context.ui.add({foundation::stable_id("building-lab.error"), ui::UiWidgetType::Label,
                        "Generation failed: " + error_, true, false, 0.0F, 0.0F});
    } else if (!plan_.parts.empty() && runtime_) {
        const auto& selected = plan_.parts[selected_part_];
        const auto& state = runtime_->parts()[selected_part_];
        context.ui.add({foundation::stable_id("building-lab.selected"), ui::UiWidgetType::Label,
                        "Selected: " + std::string(part_name(selected.kind)) + "  integrity " +
                            std::to_string(static_cast<int>(state.integrity * 100.0F)) + "%",
                        true, false, 0.0F, 0.0F});
        context.ui.add({foundation::stable_id("building-lab.help"), ui::UiWidgetType::Label,
                        "Up/Down select part, Left/Right apply damage", true, false, 0.0F,
                        0.0F});
        context.ui.add({foundation::stable_id("building-lab.parts"), ui::UiWidgetType::Label,
                        "Parts: " + std::to_string(plan_.parts.size()) + "  Rooms: " +
                            std::to_string(plan_.rooms.size()),
                        true, false, 0.0F, 0.0F});
    }
    context.ui.add({foundation::stable_id("building-lab.back"), ui::UiWidgetType::Button,
                    "Back to main menu", true, true, 500.0F, 48.0F});
}

void BuildingLabScene::build_presentation(SceneContext& context) {
    camera::CameraRequest camera_request{};
    camera_request.preset = camera::CameraPreset::BuildingLab;
    camera_request.mode = camera::CameraMode::Orbit;
    camera_request.position = {24.0F, 18.0F, 24.0F};
    camera_request.target = {0.0F, 2.0F, 0.0F};
    camera_request.lens = {0.85F, 0.2F, 250.0F};
    context.publishCameraRequest(camera_request);
    if (!render_mesh_ || render_mesh_->vertices.empty() || render_mesh_->indices.empty()) {
        return;
    }
    context.presentation.instance_prototypes.push_back(render_mesh_);
    context.presentation.instances.push_back({
        foundation::stable_id("building-lab.instance"), render_mesh_->mesh_id,
        foundation::stable_id("material.building-lab"), {0.0F, 0.0F, 0.0F},
        {1.0F, 1.0F, 1.0F}, static_cast<float>(std::sin(elapsed_seconds_ * 0.12)), 0,
        render::RenderInstanceFlagPreview});
}

void BuildingLabScene::rebuild_mesh() {
    if (!runtime_) {
        return;
    }
    auto mesh = std::make_shared<render::RenderMesh>();
    mesh->mesh_id = foundation::stable_id("mesh.building-lab.runtime");
    mesh->revision = render_mesh_ ? render_mesh_->revision + 1U : 1U;
    mesh->vertices.reserve(plan_.parts.size() * 24U);
    mesh->indices.reserve(plan_.parts.size() * 36U);
    const auto& runtime_parts = runtime_->parts();
    for (std::size_t index = 0; index < plan_.parts.size(); ++index) {
        if (index >= runtime_parts.size() || runtime_parts[index].destroyed) {
            continue;
        }
        const auto appended = render::procedural::append_box(
            *mesh, plan_.parts[index].center, plan_.parts[index].extent,
            part_color(plan_.parts[index].kind, runtime_parts[index].integrity));
        if (!appended) {
            error_ = appended.error().message;
            return;
        }
    }
    render_mesh_ = std::move(mesh);
}

void BuildingLabScene::apply_selected_damage(float normalized_damage) {
    if (!runtime_ || selected_part_ >= plan_.parts.size()) {
        return;
    }
    if (runtime_->applyDamage(plan_.parts[selected_part_].id, normalized_damage)) {
        rebuild_mesh();
    }
}

} // namespace genomes::runtime
