#pragma once

#include <genomes/render/PresentationSnapshot.hpp>
#include <genomes/render/RenderExtraction.hpp>
#include <genomes/runtime/Scene.hpp>
#include <genomes/foundation/Time.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

namespace genomes::runtime {

class SceneDirector {
public:
    using Factory = std::function<std::unique_ptr<Scene>()>;

    SceneDirector(render::IRenderer& renderer,
                  ui::UiDocument& ui,
                  render::PresentationSnapshot& presentation,
                  jobs::JobSystem* jobs = nullptr);

    void register_scene(foundation::SceneId id, Factory factory);
    bool start(foundation::SceneId id);
    void handle_input(const input::InputFrame&);
    void fixed_update(double dt);
    void frame_update(double dt);
    void present();

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

private:
    [[nodiscard]] SceneContext make_context() noexcept;
    void process_commands();
    bool change_to(foundation::SceneId id);

    render::IRenderer& renderer_;
    ui::UiDocument& ui_;
    render::PresentationSnapshot& presentation_;
    SceneCommandQueue commands_;
    std::unordered_map<foundation::SceneId, Factory> factories_;
    std::unique_ptr<Scene> current_;
    std::optional<WorldGenerationConfig> active_world_config_;
    jobs::JobSystem* jobs_{nullptr};
    bool quit_requested_{false};
    std::uint64_t frame_number_{0};
    foundation::SimulationTick previous_presentation_tick_{};
    foundation::SimulationTick next_presentation_tick_{};
    double interpolation_alpha_{0.0};
    render::SnapshotExchange presentation_exchange_{3};
};

} // namespace genomes::runtime
