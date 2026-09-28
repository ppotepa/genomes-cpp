#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/input/InputFrame.hpp>
#include <genomes/render/PresentationSnapshot.hpp>
#include <genomes/render/RenderTypes.hpp>
#include <genomes/runtime/ApplicationCommand.hpp>
#include <genomes/ui/UiDocument.hpp>

#include <cstdint>
#include <deque>

namespace genomes::runtime {

class SceneCommandQueue {
public:
    void push(ApplicationCommand command) {
        commands_.push_back(command);
    }

    [[nodiscard]] bool empty() const noexcept {
        return commands_.empty();
    }

    ApplicationCommand pop() {
        const ApplicationCommand command = commands_.front();
        commands_.pop_front();
        return command;
    }

private:
    std::deque<ApplicationCommand> commands_;
};

struct SceneContext {
    SceneCommandQueue& commands;
    ui::UiDocument& ui;
    render::PresentationSnapshot& presentation;
    const WorldGenerationConfig* world_config{nullptr};
    jobs::JobSystem* jobs{nullptr};
    render::RenderCapabilities render_capabilities{};
};

class Scene {
public:
    virtual ~Scene() = default;

    [[nodiscard]] virtual foundation::SceneId id() const noexcept = 0;
    virtual void on_enter(SceneContext&) {}
    virtual void on_exit(SceneContext&) {}
    virtual void handle_input(SceneContext&, const input::InputFrame&) {}
    virtual void fixed_update(SceneContext&, double) {}
    virtual void frame_update(SceneContext&, double) {}
    virtual void build_presentation(SceneContext&) {}
};

} // namespace genomes::runtime
