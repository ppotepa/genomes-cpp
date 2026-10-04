#include <genomes/game_scenes/BuildingLabScene.hpp>
#include <genomes/game_scenes/ApplicationCommand.hpp>
#include <genomes/buildings/BuildingModel.hpp>

#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/camera/Camera.hpp>
#include <genomes/render/ProceduralMeshes.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>

namespace genomes::game_scenes {

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
    if (generation_ticket_.valid()) {
        generation_ticket_.cancel();
        generation_ticket_ = {};
    }
    generation_channel_ = proc::GenerationChannel{};
    elapsed_seconds_ = 0.0;
    selected_part_ = 0;
    error_.clear();
    plan_ = {};
    runtime_.reset();
    render_mesh_.reset();
    if (building_profile_ == nullptr || !building_profile_->frozen()) {
        error_ = "building profile is unavailable";
        context.ui.clear();
        return;
    }
    const buildings::BuildingSpec spec = building_profile_->labPreview().instantiate(seed_);
    shared_procedural_runtime_ = context.engine_services != nullptr
        ? context.engine_services->procedural_runtime : nullptr;
    if (shared_procedural_runtime_ != nullptr) {
        proc::GenerationRequest<buildings::BuildingSpec, buildings::BuildingPlan> request;
        request.generator = proc::generatorId("buildings.plan");
        request.input = std::make_shared<const buildings::BuildingSpec>(spec);
        request.seed_path = proc::SeedPath(spec.seed);
        request.options.input_hash = foundation::stableHashCombine(spec.building_id, spec.seed);
        request.options.retained_bytes = sizeof(buildings::BuildingPlan);
        if (context.engine_services != nullptr && context.engine_services->generation != nullptr) {
            request.options.cancellation = context.engine_services->generation->cancellation();
        }
        generation_ticket_ = shared_procedural_runtime_->request(
            std::move(request), &generation_channel_);
        context.ui.clear();
        return;
    }
    error_ = "shared procedural runtime is unavailable";
    context.ui.clear();
}

void BuildingLabScene::on_exit(SceneContext&) {
    if (generation_ticket_.valid()) {
        generation_ticket_.cancel();
        generation_ticket_ = {};
    }
    shared_procedural_runtime_ = nullptr;
    runtime_.reset();
    render_mesh_.reset();
    plan_ = {};
    error_.clear();
}

void BuildingLabScene::handle_input(SceneContext& context, const input::InputFrame& input) {
    if (input.cancel_pressed || input.confirm_pressed) {
        application::enqueueApplicationCommand(context,
                                                application::ApplicationCommandKind::ReturnToMainMenu);
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
    if (input.left_pressed) damage_amount_ = std::clamp(damage_amount_ - 0.05F, 0.0F, 1.0F);
    else if (input.right_pressed) damage_amount_ = std::clamp(damage_amount_ + 0.05F, 0.0F, 1.0F);
}

void BuildingLabScene::fixed_update(SceneContext&, double dt) {
    elapsed_seconds_ += dt;
}

void BuildingLabScene::frame_update(SceneContext& context, double) {
    if (generation_ticket_.valid() && generation_ticket_.complete()) {
        if (generation_ticket_.status() == proc::GenerationStatus::Completed) {
            if (const auto generated = generation_ticket_.artifact(); generated) {
                plan_ = *generated;
                runtime_ = std::make_unique<buildings::BuildingRuntime>(plan_);
                rebuild_mesh();
            }
        } else {
            error_ = std::string(generation_ticket_.error().message);
        }
        generation_ticket_ = {};
    }
    context.ui.clear();
    auto& model = context.ui.model();
    (void)model.set("title", std::string{"Procedural building plan and damage runtime"});
    (void)model.set("seed", static_cast<std::int64_t>(seed_));
    (void)model.set("error", std::string{});
    ui::UiFieldState damage{};
    damage.value = static_cast<double>(damage_amount_);
    // Selecting an amount updates the scene-side draft on change; the
    // destructive operation itself remains explicit behind Apply Damage.
    damage.commit_policy = ui::UiCommitPolicy::OnChange;
    damage.minimum = 0.0; damage.maximum = 1.0; damage.step = 0.05;
    (void)model.set_field("damage_amount", std::move(damage));
    if (!error_.empty()) {
        (void)model.set("error", "Generation failed: " + error_);
    } else if (!plan_.parts.empty() && runtime_) {
        const auto& selected = plan_.parts[selected_part_];
        const auto& state = runtime_->parts()[selected_part_];
        (void)model.set("selected", "Selected: " + std::string(part_name(selected.kind)));
        (void)model.set("selected_part", static_cast<std::int64_t>(selected_part_));
        (void)model.set("integrity", static_cast<double>(state.integrity));
        (void)model.set("parts", static_cast<std::int64_t>(plan_.parts.size()));
        (void)model.set("rooms", static_cast<std::int64_t>(plan_.rooms.size()));
        std::vector<ui::UiTableRow> options;
        options.reserve(plan_.parts.size());
        for (std::size_t index = 0; index < plan_.parts.size(); ++index) {
            const auto& part = plan_.parts[index];
            options.push_back({{"id", std::to_string(index)},
                               {"label", std::string(part_name(part.kind))},
                               {"value", std::to_string(index)}});
        }
        (void)model.set_list("parts_options", std::move(options));
    }
}

ui::UiActionResult BuildingLabScene::handle_ui_action(
    SceneContext& context, ui::UiActionId action, const ui::UiActionArguments& arguments) {
    if (action == foundation::stable_id("building.regenerate")) {
        ++seed_;
        on_enter(context);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("building.select-next")) {
        if (!plan_.parts.empty()) selected_part_ = (selected_part_ + 1U) % plan_.parts.size();
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("building.select")) {
        for (const auto& argument : arguments) if (argument.first == "value") {
            std::size_t selected = 0;
            const auto parsed = std::from_chars(argument.second.data(),
                                                argument.second.data() + argument.second.size(), selected);
            if (parsed.ec != std::errc{} || parsed.ptr != argument.second.data() + argument.second.size() ||
                selected >= plan_.parts.size()) return ui::UiActionResult::Rejected;
            selected_part_ = selected;
            return ui::UiActionResult::Handled;
        }
        return ui::UiActionResult::Rejected;
    }
    if (action == foundation::stable_id("building.apply-damage")) {
        if (const auto* field = context.ui.model().find_field("damage_amount"); field != nullptr) {
            if (const auto* value = std::get_if<double>(&field->value); value != nullptr)
                damage_amount_ = std::clamp(static_cast<float>(*value), 0.0F, 1.0F);
        }
        apply_selected_damage(damage_amount_);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("building.damage")) {
        for (const auto& argument : arguments) {
            if (argument.first != "value") continue;
            double parsed = 0.0;
            const auto converted = std::from_chars(argument.second.data(),
                                                   argument.second.data() + argument.second.size(),
                                                   parsed, std::chars_format::general);
            if (converted.ec != std::errc{} || converted.ptr != argument.second.data() + argument.second.size() || !std::isfinite(parsed)) return ui::UiActionResult::Rejected;
            damage_amount_ = std::clamp(static_cast<float>(parsed), 0.0F, 1.0F);
            return ui::UiActionResult::Handled;
        }
        return ui::UiActionResult::Rejected;
    }
    if (action == foundation::stable_id("scene.return-main-menu")) {
        application::enqueueApplicationCommand(context,
                                                application::ApplicationCommandKind::ReturnToMainMenu);
        return ui::UiActionResult::Handled;
    }
    return ui::UiActionResult::Unknown;
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

} // namespace genomes::game_scenes
