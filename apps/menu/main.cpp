#include <genomes/render/NullRenderer.hpp>
#include <genomes/runtime/BuiltinScenes.hpp>
#include <genomes/runtime/SceneDirector.hpp>
#include <genomes/simulation/FixedStepClock.hpp>
#include <genomes/ui/UiRuntime.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <variant>

int main() {
    genomes::render::NullRenderer renderer;
    genomes::ui::UiRuntime ui;
    genomes::render::PresentationSnapshot presentation;
    genomes::runtime::SceneDirector director(renderer, ui, presentation);

    const auto menu_id = genomes::foundation::scene_id("scene.main-menu");
    genomes::runtime::registerBuiltinScenes(director, false);

    if (!director.start(menu_id)) {
        std::cerr << "Could not start main menu\n";
        return 1;
    }

    genomes::simulation::FixedStepClock clock;
    const auto simulate = [&](double fixed_dt, genomes::foundation::SimulationTick) noexcept {
        director.fixed_update(fixed_dt);
    };
    const auto advance = clock.advanceBy(std::chrono::nanoseconds{16'666'667}, simulate);
    director.set_presentation_timing(advance.first_tick, advance.next_tick,
                                     advance.interpolation_alpha);
    director.frame_update(1.0 / 60.0);
    director.present();

    std::cout << "GENOMES\n\n";
    if (const auto* title = ui.model().find("title"); title != nullptr &&
        std::holds_alternative<std::string>(*title))
        std::cout << std::get<std::string>(*title) << '\n';
    if (const auto* selected = ui.model().find("selected"); selected != nullptr &&
        std::holds_alternative<std::string>(*selected))
        std::cout << "selected: " << std::get<std::string>(*selected) << '\n';
    std::cout << "\nscene instances: " << presentation.instances.size()
              << ", submitted ui nodes: " << renderer.submitted_ui_nodes()
              << ", rendered frames: " << renderer.frames_started() << '\n';

    // The real SDL adapter will produce this same input frame. The scene
    // remains unaware of the platform and only receives semantic actions.
    director.handle_input({.confirm_pressed = true, .events = {}});
    director.frame_update(1.0 / 60.0);
    std::cout << "after confirm: scene "
              << (director.current() ? director.current()->id() : 0) << '\n';
    std::cout << "world config model fields: " << ui.model().size()
              << ", preview instances: " << presentation.instances.size() << '\n';

    // The default world-config selection is Start game. Confirming it carries
    // the seed and generation controls through the application command queue.
    director.handle_input({.confirm_pressed = true, .events = {}});
    director.frame_update(1.0 / 60.0);
    std::cout << "after world start: scene "
              << (director.current() ? director.current()->id() : 0) << '\n';
    return 0;
}
