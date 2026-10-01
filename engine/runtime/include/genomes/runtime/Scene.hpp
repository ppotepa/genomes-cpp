#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/input/InputFrame.hpp>
#include <genomes/render/PresentationSnapshot.hpp>
#include <genomes/render/RenderTypes.hpp>
#include <genomes/camera/Camera.hpp>
#include <genomes/simulation/SessionSimulationClock.hpp>
#include <genomes/ui/UiRuntime.hpp>

#include <cstdint>
#include <deque>
#include <memory>

namespace genomes::runtime {

// The neutral runtime transports scene commands without knowing which
// product/application owns their payload. Product layers derive their typed
// command from this marker and install a handler at the composition root.
class SceneCommand {
public:
    virtual ~SceneCommand() = default;
};

using SceneCommandPtr = std::unique_ptr<SceneCommand>;

class SceneCommandQueue {
public:
    void push(SceneCommandPtr command) {
        if (command != nullptr) {
            commands_.push_back(std::move(command));
        }
    }

    [[nodiscard]] bool empty() const noexcept {
        return commands_.empty();
    }

    SceneCommandPtr pop() {
        SceneCommandPtr command = std::move(commands_.front());
        commands_.pop_front();
        return command;
    }

private:
    std::deque<SceneCommandPtr> commands_;
};

struct SceneContext {
    SceneCommandQueue& commands;
    ui::UiRuntime& ui;
    render::PresentationSnapshot& presentation;
    jobs::JobSystem* jobs{nullptr};
    render::RenderCapabilities render_capabilities{};
    // Previous completed renderer frame. This is diagnostic/presentation data,
    // never a simulation input.
    render::RenderUploadTelemetry render_telemetry{};
    // Evidence/capture mode disables worker timing as an input to generated
    // presentation state. Simulation still advances through fixed_update.
    bool deterministic_capture{false};
    int framebuffer_width{1280};
    int framebuffer_height{720};
    double ui_scale{1.0};
    camera::CameraRequest* camera_request{nullptr};
    bool* camera_request_published{nullptr};

    void publishCameraRequest(const camera::CameraRequest& request) noexcept {
        if (camera_request != nullptr && camera_request_published != nullptr) {
            *camera_request = request;
            *camera_request_published = true;
        }
    }
};

class Scene {
public:
    virtual ~Scene() = default;

    [[nodiscard]] virtual foundation::SceneId id() const noexcept = 0;
    virtual void on_enter(SceneContext&) {}
    virtual void on_exit(SceneContext&) {}
    virtual void handle_input(SceneContext&, const input::InputFrame&) {}
    virtual ui::UiActionResult handle_ui_action(
        SceneContext&, ui::UiActionId, const ui::UiActionArguments&) {
        return ui::UiActionResult::Unknown;
    }
    // The session clock is the sole owner of simulation tick, fixed dt and
    // frequency.  The double overload remains as a compatibility hook for
    // scenes that only need a duration; new simulation owners override the
    // TickContext overload.
    virtual void fixed_update(SceneContext& context,
                              const simulation::TickContext& tick) {
        fixed_update(context, tick.fixed_dt_seconds);
    }
    virtual void fixed_update(SceneContext&, double) {}
    virtual void frame_update(SceneContext&, double) {}
    virtual void build_presentation(SceneContext&) {}
};

} // namespace genomes::runtime
