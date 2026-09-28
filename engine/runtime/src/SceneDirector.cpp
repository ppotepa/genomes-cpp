#include <genomes/runtime/SceneDirector.hpp>

#include <genomes/runtime/MainMenuScene.hpp>

#include <algorithm>
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
        context.ui.add({foundation::stable_id("placeholder.panel"), ui::UiNodeType::Panel,
                        title_, true, false, 720.0F, 480.0F});
        context.ui.add({foundation::stable_id("placeholder.description"), ui::UiNodeType::Label,
                        "Scene registered; domain module will provide its content.",
                        true, false, 0.0F, 0.0F});
    }

private:
    foundation::SceneId scene_id_;
    const char* title_;
};

} // namespace

SceneDirector::SceneDirector(render::IRenderer& renderer,
                             ui::UiDocument& ui,
                             render::PresentationSnapshot& presentation,
                             jobs::JobSystem* jobs)
    : renderer_(renderer), ui_(ui), presentation_(presentation), jobs_{jobs} {}

SceneContext SceneDirector::make_context() noexcept {
    return {commands_, ui_, presentation_, active_world_config_ ? &*active_world_config_ : nullptr,
            jobs_, renderer_.capabilities()};
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
    SceneContext context = make_context();
    current_->handle_input(context, input);
    process_commands();
}

bool SceneDirector::change_to(foundation::SceneId id) {
    const auto factory = factories_.find(id);
    if (factory == factories_.end()) {
        return false;
    }

    SceneContext context = make_context();
    if (current_) {
        current_->on_exit(context);
    }
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
    presentation_.clear();
    presentation_.previous_simulation_tick = previous_presentation_tick_.value;
    presentation_.simulation_tick = next_presentation_tick_.value;
    presentation_.interpolation_alpha = interpolation_alpha_;
    current_->frame_update(context, dt);
    current_->build_presentation(context);
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
        renderer_.submit(read.value().snapshot(), ui_);
    } else {
        // A full exchange is a normal non-blocking backpressure outcome. The
        // compatibility mirror still lets single-threaded/headless callers
        // present the current frame while the next slot becomes available.
        renderer_.submit(presentation_, ui_);
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
            break;
        case ApplicationCommandKind::Quit:
            quit_requested_ = true;
            break;
        }
    }
}

} // namespace genomes::runtime
