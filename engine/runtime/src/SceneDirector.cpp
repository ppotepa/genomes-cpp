#include <genomes/runtime/SceneDirector.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace genomes::runtime {

SceneDirector::SceneDirector(render::IRenderer& renderer,
                             ui::UiRuntime& ui,
                             render::PresentationSnapshot& presentation,
                             jobs::JobSystem* jobs)
    : renderer_(renderer), ui_(ui), presentation_(presentation), jobs_{jobs} {}

SceneContext SceneDirector::make_context() noexcept {
    return {commands_, ui_, presentation_, jobs_, renderer_.capabilities(),
            renderer_.uploadTelemetry(),
            deterministic_capture_, framebuffer_width_, framebuffer_height_, session_ui_scale_,
            &presentation_.camera_request,
            &presentation_.has_camera_request};
}

foundation::Result<void, foundation::Error>
SceneDirector::register_scene(foundation::SceneId id, Factory factory) {
    if (scene_registry_frozen_) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "scene registry is frozen"});
    }
    if (factories_.contains(id) || unavailable_scenes_.contains(id)) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "scene ID already registered"});
    }
    factories_.emplace(id, std::move(factory));
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error>
SceneDirector::register_unavailable_scene(foundation::SceneId id, foundation::Error error) {
    if (scene_registry_frozen_) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "scene registry is frozen"});
    }
    if (error.code != foundation::ErrorCode::UnavailableFeature || factories_.contains(id) ||
        unavailable_scenes_.contains(id)) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid unavailable scene registration"});
    }
    unavailable_scenes_.emplace(id, error);
    return foundation::Result<void, foundation::Error>::success();
}

bool SceneDirector::start(foundation::SceneId id) {
    scene_registry_frozen_ = true;
    return change_to(id);
}

void SceneDirector::handle_input(const input::InputFrame& input) {
    if (!current_) {
        return;
    }
    if (ui_.process_input(input)) {
        viewport_controller_.cancelGesture();
        return;
    }
    framebuffer_width_ = std::max(1, static_cast<int>(input.viewport_width));
    framebuffer_height_ = std::max(1, static_cast<int>(input.viewport_height));
    if (presentation_.camera.enabled && presentation_.camera.mode != camera::CameraMode::Fixed) {
        viewport_controller_.configure(presentation_.camera.toRequest(), presentation_.camera.revision);
        viewport_controller_.handleInput(input);
    } else {
        viewport_controller_.clear();
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
    if (application_action_router_) {
        const auto result = application_action_router_(action, arguments);
        if (result != ui::UiActionResult::Unknown) process_commands();
        return result;
    }
    return ui::UiActionResult::Unknown;
}

bool SceneDirector::change_to(foundation::SceneId id) {
    if (const auto unavailable = unavailable_scenes_.find(id);
        unavailable != unavailable_scenes_.end()) {
        last_error_ = unavailable->second;
        return false;
    }
    const auto factory = factories_.find(id);
    if (factory == factories_.end()) {
        last_error_ = {foundation::ErrorCode::NotFound, "scene ID is not registered"};
        return false;
    }

    ++scene_epoch_;
    viewport_controller_.clear();

    SceneContext context = make_context();
    if (current_) {
        current_->on_exit(context);
    }
    ui_.routes().replace({id, {}, {}, {}, false});
    ui_.reset_model();
    current_ = factory->second();
    current_->on_enter(context);
    last_error_ = {};
    return true;
}

void SceneDirector::fixed_update(const simulation::TickContext& tick_context) {
    if (!current_) {
        return;
    }
    SceneContext scene_context = make_context();
    current_->fixed_update(scene_context, tick_context);
}

void SceneDirector::fixed_update(double dt) {
    if (!current_) {
        return;
    }
    simulation::TickContext context{};
    context.tick = compatibility_tick_;
    context.tick.increment();
    context.fixed_dt_seconds = dt;
    context.tick_rate_hz = simulation::SessionSimulationTickRateHz;
    compatibility_tick_ = context.tick;
    fixed_update(context);
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
        const camera::CameraRequest declared = presentation_.camera_request;
        const std::uint64_t declared_revision = presentation_.camera.revision;
        viewport_controller_.configure(declared, declared_revision);
        const auto resolved_request = viewport_controller_.update(static_cast<float>(dt));
        presentation_.camera = {};
        presentation_.camera.enabled = true;
        presentation_.camera.applyRequest(resolved_request);
        presentation_.camera.revision = declared_revision;
    } else if (presentation_.camera.enabled) {
        viewport_controller_.configure(presentation_.camera.toRequest(), presentation_.camera.revision);
        presentation_.camera.applyRequest(viewport_controller_.update(static_cast<float>(dt)));
    } else {
        viewport_controller_.clear();
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
        SceneCommandPtr command = commands_.pop();
        if (scene_command_handler_) {
            scene_command_handler_(std::move(command));
        } else {
            last_error_ = {foundation::ErrorCode::InvalidState,
                           "scene command handler is not installed"};
        }
    }
}

} // namespace genomes::runtime
