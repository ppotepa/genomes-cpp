#pragma once

#include <genomes/render/PresentationSnapshot.hpp>
#include <genomes/render/RenderExtraction.hpp>
#include <genomes/runtime/Scene.hpp>
#include <genomes/foundation/Time.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/camera/Camera.hpp>
#include <genomes/runtime/ViewportController.hpp>
#include <genomes/api/Api.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/jobs/SchedulerClient.hpp>
#include <genomes/proc/GenerationClient.hpp>

#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>

namespace genomes::runtime {

class SceneDirector : public api::CoreControlApi {
public:
    class PresentationBridge final : public api::PresentationFacade {
    public:
        void requestSnapshot(api::SnapshotView snapshot) override {
            if (snapshot.scene_epoch < latest_.scene_epoch ||
                (snapshot.scene_epoch == latest_.scene_epoch &&
                 snapshot.tick < latest_.tick)) {
                ++rejected_stale_;
                return;
            }
            latest_ = snapshot;
        }

        [[nodiscard]] const api::SnapshotView& latest() const noexcept { return latest_; }
        [[nodiscard]] std::uint64_t rejectedStale() const noexcept { return rejected_stale_; }
        void reset(std::uint64_t epoch) noexcept {
            if (latest_.scene_epoch < epoch) latest_ = {};
        }

    private:
        api::SnapshotView latest_{};
        std::uint64_t rejected_stale_{0U};
    };

    class SceneLifetimeAdapter final : public api::SceneLifetime {
    public:
        SceneLifetimeAdapter(jobs::CancelSource& cancellation,
                             const std::uint64_t& epoch) noexcept
            : cancellation_(cancellation), epoch_(epoch) {}
        [[nodiscard]] jobs::CancelToken cancellation() const noexcept override {
            return cancellation_.token();
        }
        [[nodiscard]] std::uint64_t sceneEpoch() const noexcept override { return epoch_; }

    private:
        jobs::CancelSource& cancellation_;
        const std::uint64_t& epoch_;
    };

    using Factory = std::function<std::unique_ptr<Scene>()>;
    using ApplicationActionRouter = std::function<ui::UiActionResult(
        ui::UiActionId, const ui::UiActionArguments&)>;
    using SceneCommandHandler = std::function<void(SceneCommandPtr)>;

    SceneDirector(render::IRenderer& renderer,
                  ui::UiRuntime& ui,
                  render::PresentationSnapshot& presentation,
                  jobs::JobSystem& jobs,
                  proc::ProceduralRuntime* procedural_runtime = nullptr);

    void set_application_action_router(ApplicationActionRouter router) {
        application_action_router_ = std::move(router);
    }
    void set_scene_command_handler(SceneCommandHandler handler) {
        scene_command_handler_ = std::move(handler);
    }
    void enqueue_command(SceneCommandPtr command) { commands_.push(std::move(command)); }
    [[nodiscard]] ui::UiRuntime& ui_runtime() noexcept { return ui_; }
    [[nodiscard]] const ui::UiRuntime& ui_runtime() const noexcept { return ui_; }
    [[nodiscard]] foundation::SceneId current_scene_id() const noexcept {
        return current_ != nullptr ? current_->id() : foundation::SceneId{0};
    }
    [[nodiscard]] double session_ui_scale() const noexcept { return session_ui_scale_; }
    void set_session_ui_scale(double value) noexcept { session_ui_scale_ = value; }
    [[nodiscard]] bool session_show_diagnostics() const noexcept {
        return session_show_diagnostics_;
    }
    void set_session_show_diagnostics(bool value) noexcept {
        session_show_diagnostics_ = value;
    }
    void request_quit() noexcept { quit_requested_ = true; }
    [[nodiscard]] bool startScene(foundation::SceneId scene) override { return start(scene); }
    void requestQuit() noexcept override { request_quit(); }

    foundation::Result<void, foundation::Error>
    register_scene(foundation::SceneId id, Factory factory);
    foundation::Result<void, foundation::Error>
    register_unavailable_scene(foundation::SceneId id, foundation::Error error);
    [[nodiscard]] foundation::Result<void, foundation::Error> register_module(
        api::ModuleDescriptor descriptor, api::ModuleHost::Registration registration) {
        return modules_.registerModule(std::move(descriptor), std::move(registration));
    }
    [[nodiscard]] foundation::Result<void, foundation::Error> finalize_modules() {
        return modules_.finalize();
    }
    bool start(foundation::SceneId id);
    void handle_input(const input::InputFrame&);
    [[nodiscard]] ui::UiActionResult dispatch_ui_action(
        ui::UiActionId action, const ui::UiActionArguments& arguments);
    void fixed_update(const simulation::TickContext& context);
    // Compatibility entry point for legacy callers that have only a fixed
    // duration. Production application code must pass the session context.
    void fixed_update(double dt);
    void frame_update(double dt);
    void present();
    [[nodiscard]] bool rendererHealthy() const noexcept { return renderer_.healthy(); }
    [[nodiscard]] foundation::Error rendererLastError() const noexcept {
        return renderer_.last_error();
    }

    void set_deterministic_capture(bool enabled) noexcept {
        deterministic_capture_ = enabled;
    }

    void set_presentation_timing(foundation::SimulationTick previous_tick,
                                 foundation::SimulationTick next_tick,
                                 double interpolation_alpha) noexcept;

    [[nodiscard]] Scene* current() noexcept {
        return current_.get();
    }

    [[nodiscard]] bool quit_requested() const noexcept {
        return quit_requested_;
    }

    [[nodiscard]] const foundation::Error& last_error() const noexcept {
        return last_error_;
    }
    [[nodiscard]] api::ModuleHost& module_host() noexcept { return modules_; }
    [[nodiscard]] const api::ModuleHost& module_host() const noexcept { return modules_; }
    [[nodiscard]] const PresentationBridge& presentation_api() const noexcept {
        return presentation_api_;
    }
    [[nodiscard]] api::EngineTelemetry& telemetry() noexcept { return telemetry_; }

private:
    [[nodiscard]] SceneContext make_context() noexcept;
    void process_commands();
    bool change_to(foundation::SceneId id);

    render::IRenderer& renderer_;
    ui::UiRuntime& ui_;
    render::PresentationSnapshot& presentation_;
    SceneCommandQueue commands_;
    std::unordered_map<foundation::SceneId, Factory> factories_;
    std::unordered_map<foundation::SceneId, foundation::Error> unavailable_scenes_;
    bool scene_registry_frozen_{false};
    std::unique_ptr<Scene> current_;
    jobs::JobSystem& jobs_;
    jobs::SchedulerClient execution_client_;
    proc::ProceduralRuntime* procedural_runtime_{nullptr};
    proc::GenerationClient generation_client_;
    bool quit_requested_{false};
    foundation::Error last_error_{};
    bool deterministic_capture_{false};
    std::uint64_t frame_number_{0};
    foundation::SimulationTick previous_presentation_tick_{};
    foundation::SimulationTick next_presentation_tick_{};
    double interpolation_alpha_{0.0};
    int framebuffer_width_{1280};
    int framebuffer_height_{720};
    ViewportController viewport_controller_{};
    std::uint64_t scene_epoch_{0};
    double session_ui_scale_{1.0};
    bool session_show_diagnostics_{true};
    foundation::SimulationTick compatibility_tick_{};
    ApplicationActionRouter application_action_router_{};
    SceneCommandHandler scene_command_handler_{};
    render::SnapshotExchange presentation_exchange_{3};
    api::ModuleHost modules_{};
    jobs::CancelSource scene_cancellation_{};
    SceneLifetimeAdapter scene_lifetime_;
    PresentationBridge presentation_api_{};
    api::EngineTelemetry telemetry_{};
    api::EngineServices engine_services_{};
};

} // namespace genomes::runtime
