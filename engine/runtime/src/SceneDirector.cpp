#include <genomes/runtime/SceneDirector.hpp>

#include <genomes/runtime/MainMenuScene.hpp>

#include <algorithm>
#include <string>
#include <utility>

namespace genomes::runtime {

namespace {

class PlaceholderScene final : public Scene {
public:
    explicit PlaceholderScene(foundation::SceneId scene_id, const char* title)
        : scene_id_(scene_id), title_(title) {}

    [[nodiscard]] foundation::SceneId id() const noexcept override {
        return scene_id_;
    }

    void on_enter(SceneContext& context) override {
        context.ui.clear();
    }

    void frame_update(SceneContext& context, double) override {
        context.ui.clear();
        context.ui.add({foundation::stable_id("placeholder.panel"), ui::UiWidgetType::Panel,
                        title_, true, false, 720.0F, 480.0F});
        context.ui.add({foundation::stable_id("placeholder.description"), ui::UiWidgetType::Label,
                        "Scene registered; domain module will provide its content.",
                        true, false, 0.0F, 0.0F});
    }

private:
    foundation::SceneId scene_id_;
    const char* title_;
};

} // namespace

SceneDirector::SceneDirector(render::IRenderer& renderer,
                             ui::UiRuntime& ui,
                             render::PresentationSnapshot& presentation,
                             jobs::JobSystem* jobs)
    : renderer_(renderer), ui_(ui), presentation_(presentation), jobs_{jobs} {}

SceneContext SceneDirector::make_context() noexcept {
    return {commands_, ui_, presentation_, active_world_config_ ? &*active_world_config_ : nullptr,
            jobs_, renderer_.capabilities(), renderer_.uploadTelemetry(),
            deterministic_capture_, &presentation_.camera_request,
            &presentation_.has_camera_request};
}

void SceneDirector::register_scene(foundation::SceneId id, Factory factory) {
    factories_[id] = std::move(factory);
}

bool SceneDirector::start(foundation::SceneId id) {
    return change_to(id);
}

void SceneDirector::handle_input(const input::InputFrame& input) {
    if (!current_) {
        return;
    }
    if (ui_.process_input(input)) {
        return;
    }
    framebuffer_width_ = std::max(1, static_cast<int>(input.viewport_width));
    framebuffer_height_ = std::max(1, static_cast<int>(input.viewport_height));
    if (presentation_.camera.enabled && presentation_.camera.interactive_orbit) {
        camera::CameraRequest request = presentation_.camera.toRequest();
        if (!camera_controller_initialized_) {
            camera_controller_.reset(request);
            camera_controller_initialized_ = true;
        }
        camera::CameraInput camera_input{};
        camera_input.orbit_x = input.mouse_left_down ? input.mouse_delta_x * 0.01F : 0.0F;
        camera_input.orbit_y = input.mouse_left_down ? input.mouse_delta_y * 0.01F : 0.0F;
        camera_input.zoom = -input.mouse_wheel_y * 0.05F;
        camera_input.cancel = input.cancel_pressed || input.pointer_cancel;
        camera_input.focus_lost = input.focus_lost;
        camera_controller_.update(request, camera_input, 1.0F / 60.0F);
        presentation_.camera.applyRequest(request);
        ++presentation_.camera.revision;
    } else {
        camera_controller_initialized_ = false;
    }
    SceneContext context = make_context();
    current_->handle_input(context, input);
    process_commands();
}

ui::UiActionResult SceneDirector::dispatch_ui_action(
    ui::UiActionId action, const ui::UiActionArguments& arguments) {
    if (current_) {
        SceneContext context = make_context();
        const auto local = current_->handle_ui_action(context, action, arguments);
        if (local != ui::UiActionResult::Unknown) {
            process_commands();
            return local;
        }
    }
    const auto push = [this](ApplicationCommandKind kind) {
        commands_.push({kind, {}});
        return ui::UiActionResult::Handled;
    };
    if (action == foundation::stable_id("scene.start-battlefield"))
        return push(ApplicationCommandKind::StartScenario);
    if (action == foundation::stable_id("scene.open-unit-lab"))
        return push(ApplicationCommandKind::OpenUnitLab);
    if (action == foundation::stable_id("scene.open-building-lab"))
        return push(ApplicationCommandKind::OpenBuildingLab);
    if (action == foundation::stable_id("scene.open-world-config"))
        return push(ApplicationCommandKind::OpenWorldConfig);
    if (action == foundation::stable_id("scene.open-settings")) {
        if (ui_.routes().top() != nullptr && ui_.routes().top()->overlay) return ui::UiActionResult::Rejected;
        ui_.routes().push({foundation::scene_id("scene.settings"), {}, "builtin.settings", "scene.settings", true});
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("scene.open-pause")) {
        if (current_ == nullptr || current_->id() != foundation::scene_id("scene.battlefield") ||
            (ui_.routes().top() != nullptr && ui_.routes().top()->overlay)) return ui::UiActionResult::Rejected;
        ui_.routes().push({foundation::scene_id("scene.pause"), {}, "builtin.pause", "scene.pause", true});
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("scene.return-main-menu")) {
        if (ui_.routes().top() != nullptr && ui_.routes().top()->scene == foundation::scene_id("scene.pause")) {
            return push(ApplicationCommandKind::ReturnToMainMenu);
        } else if (ui_.routes().top() != nullptr && ui_.routes().top()->overlay) {
            ui_.routes().pop();
            return ui::UiActionResult::Handled;
        }
        return push(ApplicationCommandKind::ReturnToMainMenu);
    }
    if (action == foundation::stable_id("scene.resume")) {
        if (ui_.routes().top() != nullptr && ui_.routes().top()->scene == foundation::scene_id("scene.pause")) {
            ui_.routes().pop();
            return ui::UiActionResult::Handled;
        }
        return ui::UiActionResult::Rejected;
    }
    if (action == foundation::stable_id("scene.close-overlay")) {
        if (ui_.routes().top() == nullptr || !ui_.routes().top()->overlay) return ui::UiActionResult::Rejected;
        ui_.routes().pop();
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("application.quit"))
        return push(ApplicationCommandKind::Quit);
    return ui::UiActionResult::Unknown;
}

bool SceneDirector::change_to(foundation::SceneId id) {
    const auto factory = factories_.find(id);
    if (factory == factories_.end()) {
        return false;
    }

    ++scene_epoch_;
    camera_controller_initialized_ = false;

    SceneContext context = make_context();
    if (current_) {
        current_->on_exit(context);
    }
    ui_.routes().replace({id, {}, {}, {}, false});
    current_ = factory->second();
    current_->on_enter(context);
    return true;
}

void SceneDirector::fixed_update(double dt) {
    if (!current_) {
        return;
    }
    SceneContext context = make_context();
    current_->fixed_update(context, dt);
}

void SceneDirector::frame_update(double dt) {
    if (!current_) {
        return;
    }
    SceneContext context = make_context();
    ui_.clear();
    // A presentation is an extraction for this frame. Clearing it here keeps
    // a scene that has no visual entities from inheriting the previous scene's
    // instances after a transition.
    presentation_.clear_scene_payload();
    current_->frame_update(context, dt);
    ui_.update(dt);
    current_->build_presentation(context);
    if (presentation_.has_camera_request) {
        const bool interactive_orbit = presentation_.camera.interactive_orbit;
        const std::uint64_t revision = presentation_.camera.revision;
        presentation_.camera = {};
        presentation_.camera.enabled = true;
        presentation_.camera.applyRequest(presentation_.camera_request);
        presentation_.camera.interactive_orbit = interactive_orbit;
        presentation_.camera.revision = revision;
    }
    presentation_.has_resolved_camera = false;
    if (presentation_.camera.enabled) {
        const camera::CameraRequest request = presentation_.camera.toRequest();
        const auto resolved = camera::resolve(request, framebuffer_width_, framebuffer_height_);
        if (resolved) {
            presentation_.resolved_camera = resolved.value();
            presentation_.has_resolved_camera = true;
        }
    }
    presentation_.scene_epoch = scene_epoch_;
    presentation_.previous_simulation_tick = previous_presentation_tick_.value;
    presentation_.simulation_tick = next_presentation_tick_.value;
    presentation_.interpolation_alpha = interpolation_alpha_;
    process_commands();

    // Publish an immutable copy into the bounded exchange. The public
    // presentation reference remains a compatibility mirror for headless
    // tools; renderer submission uses the lease below so simulation code can
    // continue producing the next snapshot without racing the backend.
    auto write = presentation_exchange_.acquireWrite();
    if (write) {
        write.value().snapshot() = presentation_;
        write.value().snapshot().frame_number = frame_number_ + 1;
        (void)presentation_exchange_.publish(std::move(write.value()));
    }
}

void SceneDirector::set_presentation_timing(foundation::SimulationTick previous_tick,
                                            foundation::SimulationTick next_tick,
                                            double interpolation_alpha) noexcept {
    previous_presentation_tick_ = previous_tick;
    next_presentation_tick_ = next_tick;
    interpolation_alpha_ = std::clamp(interpolation_alpha, 0.0, 1.0);
}

void SceneDirector::present() {
    presentation_.frame_number = ++frame_number_;
    renderer_.begin_frame();
    auto read = presentation_exchange_.acquireLatestRead();
    if (read) {
        renderer_.submit(read.value().snapshot(), ui_.frame());
    } else {
        // A full exchange is a normal non-blocking backpressure outcome. The
        // compatibility mirror still lets single-threaded/headless callers
        // present the current frame while the next slot becomes available.
        renderer_.submit(presentation_, ui_.frame());
    }
    renderer_.end_frame();
}

void SceneDirector::process_commands() {
    while (!commands_.empty()) {
        const ApplicationCommand command = commands_.pop();
        switch (command.kind) {
        case ApplicationCommandKind::StartScenario:
            active_world_config_ = command.world_config;
            change_to(foundation::scene_id("scene.battlefield"));
            break;
        case ApplicationCommandKind::OpenWorldConfig:
            active_world_config_ = command.world_config;
            change_to(foundation::scene_id("scene.world-config"));
            break;
        case ApplicationCommandKind::OpenUnitLab:
            change_to(foundation::scene_id("scene.unit-lab"));
            break;
        case ApplicationCommandKind::OpenBuildingLab:
            change_to(foundation::scene_id("scene.building-lab"));
            break;
        case ApplicationCommandKind::OpenWorldLab:
            change_to(foundation::scene_id("scene.world-lab"));
            break;
        case ApplicationCommandKind::ReturnToMainMenu:
            change_to(foundation::scene_id("scene.main-menu"));
            break;
        case ApplicationCommandKind::OpenSettings:
            change_to(foundation::scene_id("scene.settings"));
            break;
        case ApplicationCommandKind::OpenPause:
            ui_.routes().push({foundation::scene_id("scene.pause"), {}, "builtin.pause", "scene.pause", true});
            break;
        case ApplicationCommandKind::Quit:
            quit_requested_ = true;
            break;
        }
    }
}

} // namespace genomes::runtime
