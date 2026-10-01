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
    return {commands_, ui_, presentation_, active_world_config_ ? &*active_world_config_ : nullptr,
            jobs_, renderer_.capabilities(), renderer_.uploadTelemetry(),
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
        return;
    }
    framebuffer_width_ = std::max(1, static_cast<int>(input.viewport_width));
    framebuffer_height_ = std::max(1, static_cast<int>(input.viewport_height));
    if (presentation_.camera.enabled && presentation_.camera.mode != camera::CameraMode::Fixed) {
        camera::CameraRequest request = presentation_.camera.toRequest();
        camera_controller_.setMode(request.mode);
        if (!camera_controller_initialized_) {
            camera_controller_.reset(request);
            camera_controller_initialized_ = true;
        }
        camera::CameraInput camera_input{};
        const float viewport_left = presentation_.camera.viewport_left * framebuffer_width_;
        const float viewport_top = presentation_.camera.viewport_top * framebuffer_height_;
        const float viewport_width = presentation_.camera.viewport_width * framebuffer_width_;
        const float viewport_height = presentation_.camera.viewport_height * framebuffer_height_;
        const bool inside = input.mouse_x >= viewport_left && input.mouse_x < viewport_left + viewport_width &&
                            input.mouse_y >= viewport_top && input.mouse_y < viewport_top + viewport_height;
        bool pointer_down_event = input.mouse_left_pressed;
        for (const auto& event : input.events)
            if (event.type == input::EventType::MouseButtonDown) pointer_down_event = true;
        if (pointer_down_event && inside) camera_pointer_capture_ = true;
        if (input.pointer_cancel || input.focus_lost ||
            (!input.mouse_left_down && !input.mouse_middle_down && !input.mouse_right_down))
            camera_pointer_capture_ = false;
        const float normalized_x = input.mouse_delta_x / std::max(1.0F, viewport_width);
        const float normalized_y = input.mouse_delta_y / std::max(1.0F, viewport_height);
        camera_input.orbit_x = camera_pointer_capture_ && input.mouse_left_down ? normalized_x * 4.0F : 0.0F;
        camera_input.orbit_y = camera_pointer_capture_ && input.mouse_left_down ? normalized_y * 4.0F : 0.0F;
        const bool panning = camera_pointer_capture_ && (input.mouse_middle_down || input.mouse_right_down);
        camera_input.pan_x = panning ? normalized_x : 0.0F;
        camera_input.pan_y = panning ? normalized_y : 0.0F;
        camera_input.move_x = static_cast<float>(input.right_pressed) -
                              static_cast<float>(input.left_pressed);
        camera_input.move_z = static_cast<float>(input.down_pressed) -
                              static_cast<float>(input.up_pressed);
        camera_input.zoom = inside ? -input.mouse_wheel_y * 0.12F : 0.0F;
        camera_input.reset = input.reset_pressed;
        camera_input.cancel = input.cancel_pressed || input.pointer_cancel;
        camera_input.focus_lost = input.focus_lost;
        camera_controller_.update(request, camera_input, 1.0F / 60.0F);
        presentation_.camera.applyRequest(request);
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
    if (application_action_router_) {
        return application_action_router_(action, arguments);
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
    camera_controller_initialized_ = false;

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
    const render::RenderCamera previous_camera = presentation_.camera;
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
        const bool preserve_pose = camera_controller_initialized_ && previous_camera.enabled &&
                                   declared.mode != camera::CameraMode::Fixed &&
                                   declared_revision == previous_camera.revision;
        auto resolved_request = declared;
        if (preserve_pose) {
            // Keep the complete interactive pose, including a panned target.
            // Rebuilding from the declared target would erase right/middle drag
            // panning on the next extraction frame.
            resolved_request.position = previous_camera.position;
            resolved_request.target = previous_camera.target;
            resolved_request.up = declared.up;
            camera_controller_.setMode(declared.mode);
        } else {
            camera_controller_.setMode(declared.mode);
            camera_controller_.reset(resolved_request);
            camera_controller_initialized_ = declared.mode != camera::CameraMode::Fixed;
        }
        presentation_.camera = {};
        presentation_.camera.enabled = true;
        presentation_.camera.applyRequest(resolved_request);
        presentation_.camera.revision = declared_revision != 0U ? declared_revision : previous_camera.revision;
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
        if (application_command_handler_) {
            application_command_handler_(command);
        } else {
            last_error_ = {foundation::ErrorCode::InvalidState,
                           "application command handler is not installed"};
        }
    }
}

} // namespace genomes::runtime
