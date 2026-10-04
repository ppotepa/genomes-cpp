#include <genomes/runtime/SceneDirector.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace genomes::runtime {

namespace {

[[nodiscard]] const char* loadingPhaseName(SceneLoadingPhase phase) noexcept {
    switch (phase) {
    case SceneLoadingPhase::Starting: return "starting";
    case SceneLoadingPhase::InProgress: return "in-progress";
    case SceneLoadingPhase::Completed: return "completed";
    case SceneLoadingPhase::Failed: return "failed";
    }
    return "failed";
}

} // namespace

SceneDirector::SceneDirector(render::IRenderer& renderer,
                             ui::UiRuntime& ui,
                             render::PresentationSnapshot& presentation,
                             jobs::JobSystem* jobs)
    : renderer_(renderer), ui_(ui), presentation_(presentation),
      jobs_{jobs != nullptr ? jobs : &jobs::processScheduler()},
      scheduler_explicit_{jobs != nullptr} {
    const auto core_module = modules_.registerModule(
        {.id = foundation::stable_id("core"),
         .version = {},
         .required_modules = {},
         .required_capabilities = {},
         .provided_capabilities = {foundation::stable_id("core.scheduler"),
                                   foundation::stable_id("core.scenes")}},
        [](api::ModuleRegistry& registry, api::ModuleContext&) {
            api::ApiOperationDescriptor start_scene{};
            start_scene.id = foundation::stable_id("core.scene.start");
            start_scene.arguments = {api::ValueType::UnsignedInteger};
            auto result = registry.declareCommand(foundation::stable_id("core"), start_scene);
            if (!result) return result;
            api::ApiOperationDescriptor quit{};
            quit.id = foundation::stable_id("core.quit");
            result = registry.declareCommand(foundation::stable_id("core"), quit);
            if (!result) return result;
            api::ApiSystemDescriptor time{};
            time.id = foundation::stable_id("core.time");
            time.lane = jobs::ExecutionLane::Main;
            result = registry.declareSystem(foundation::stable_id("core"), std::move(time));
            if (!result) return result;
            return registry.declareResourceWrite(
                foundation::stable_id("core"), foundation::stable_id("core.session_time"));
        });
    (void)core_module;
    engine_services_.scheduler = jobs_;
    engine_services_.presentation = &presentation_api_;
    engine_services_.core = this;
    engine_services_.modules = &modules_;
    engine_services_.telemetry = &telemetry_;
    engine_services_.cancellation = scene_cancellation_.token();
    engine_services_.scene_epoch = scene_epoch_;
}

SceneContext SceneDirector::make_context() noexcept {
    return {commands_, ui_, presentation_, jobs_, scheduler_explicit_, jobs_->telemetry(),
            renderer_.capabilities(),
            renderer_.uploadTelemetry(),
            telemetry_.simulation_duration,
            telemetry_.presentation_duration,
            telemetry_.gpu_duration,
            telemetry_.rejected_stale_snapshots,
            deterministic_capture_, framebuffer_width_, framebuffer_height_, session_ui_scale_,
            &presentation_.camera_request,
            &presentation_.has_camera_request,
            scene_epoch_,
            &engine_services_};
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
    if (!modules_.frozen()) {
        const auto modules_ready = modules_.finalize();
        if (!modules_ready) {
            last_error_ = modules_ready.error();
            return false;
        }
    }
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
    scene_cancellation_.cancel();
    scene_cancellation_ = jobs::CancelSource{};
    // No scene may observe or publish the previous runtime after the epoch
    // changes. The incoming scene must explicitly bind its own facade.
    engine_services_.simulation = nullptr;
    engine_services_.scene_epoch = scene_epoch_;
    engine_services_.cancellation = scene_cancellation_.token();
    presentation_exchange_.rejectBeforeSceneEpoch(scene_epoch_);
    presentation_api_.reset(scene_epoch_);
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
    const SceneLoadingStatus loading = current_->loading_status();
    (void)ui_.model().set("scene_loading_phase",
                          std::string{loadingPhaseName(loading.phase)});
    (void)ui_.model().set("scene_loading_active",
                          loading.phase == SceneLoadingPhase::Starting ||
                              loading.phase == SceneLoadingPhase::InProgress);
    (void)ui_.model().set("scene_loading_failed",
                          loading.phase == SceneLoadingPhase::Failed);
    (void)ui_.model().set("scene_loading_progress",
                          std::clamp(loading.progress, 0.0, 1.0));
    (void)ui_.model().set("scene_loading_message", loading.message);
    ui_.update(dt);
    const auto extraction_started = std::chrono::steady_clock::now();
    current_->build_presentation(context);
    telemetry_.extraction_duration = std::chrono::duration_cast<foundation::Nanoseconds>(
        std::chrono::steady_clock::now() - extraction_started);
    if (engine_services_.simulation != nullptr) {
        telemetry_.semantic_hash = engine_services_.simulation->snapshotView().semantic_hash;
    }
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
    presentation_.revision = frame_number_ + 1U;
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
        if (const auto published = presentation_exchange_.publish(
                std::move(write.value())); published) {
            presentation_.snapshot_generation = presentation_exchange_.publishedSerial();
        }
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
