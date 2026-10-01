#pragma once

#include <genomes/render/PresentationSnapshot.hpp>
#include <genomes/render/RenderExtraction.hpp>
#include <genomes/runtime/Scene.hpp>
#include <genomes/foundation/Time.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/camera/Camera.hpp>
#include <genomes/camera/CameraController.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>

namespace genomes::runtime {

class SceneDirector {
public:
    using Factory = std::function<std::unique_ptr<Scene>()>;
    using ApplicationActionRouter = std::function<ui::UiActionResult(
        ui::UiActionId, const ui::UiActionArguments&)>;
    using ApplicationCommandHandler = std::function<void(const ApplicationCommand&)>;

    SceneDirector(render::IRenderer& renderer,
                  ui::UiRuntime& ui,
                  render::PresentationSnapshot& presentation,
                  jobs::JobSystem* jobs = nullptr);

    void set_application_action_router(ApplicationActionRouter router) {
        application_action_router_ = std::move(router);
    }
    void set_application_command_handler(ApplicationCommandHandler handler) {
        application_command_handler_ = std::move(handler);
    }
    void enqueue_command(ApplicationCommand command) { commands_.push(std::move(command)); }
    [[nodiscard]] ui::UiRuntime& ui_runtime() noexcept { return ui_; }
    [[nodiscard]] const ui::UiRuntime& ui_runtime() const noexcept { return ui_; }
    [[nodiscard]] foundation::SceneId current_scene_id() const noexcept {
        return current_ != nullptr ? current_->id() : foundation::SceneId{0};
    }
    void set_active_world_config(WorldGenerationConfig config) {
        active_world_config_ = std::move(config);
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

    foundation::Result<void, foundation::Error>
    register_scene(foundation::SceneId id, Factory factory);
    foundation::Result<void, foundation::Error>
    register_unavailable_scene(foundation::SceneId id, foundation::Error error);
    bool start(foundation::SceneId id);
    void handle_input(const input::InputFrame&);
    [[nodiscard]] ui::UiActionResult dispatch_ui_action(
        ui::UiActionId action, const ui::UiActionArguments& arguments);
    void fixed_update(double dt);
    void frame_update(double dt);
    void present();

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

    [[nodiscard]] const WorldGenerationConfig* active_world_config() const noexcept {
        return active_world_config_ ? &*active_world_config_ : nullptr;
    }

    [[nodiscard]] const foundation::Error& last_error() const noexcept {
        return last_error_;
    }

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
    std::optional<WorldGenerationConfig> active_world_config_;
    jobs::JobSystem* jobs_{nullptr};
    bool quit_requested_{false};
    foundation::Error last_error_{};
    bool deterministic_capture_{false};
    std::uint64_t frame_number_{0};
    foundation::SimulationTick previous_presentation_tick_{};
    foundation::SimulationTick next_presentation_tick_{};
    double interpolation_alpha_{0.0};
    int framebuffer_width_{1280};
    int framebuffer_height_{720};
    camera::CameraController camera_controller_{};
    bool camera_controller_initialized_{false};
    bool camera_pointer_capture_{false};
    std::uint64_t scene_epoch_{0};
    double session_ui_scale_{1.0};
    bool session_show_diagnostics_{true};
    ApplicationActionRouter application_action_router_{};
    ApplicationCommandHandler application_command_handler_{};
    render::SnapshotExchange presentation_exchange_{3};
};

} // namespace genomes::runtime
