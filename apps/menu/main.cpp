#include <genomes/render/NullRenderer.hpp>
#include <genomes/runtime/BuiltinScenes.hpp>
#include <genomes/runtime/SceneDirector.hpp>
#include <genomes/simulation/FixedStepClock.hpp>
#include <genomes/ui/UiRuntime.hpp>

#include <chrono>
#include <iostream>
#include <string>

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
    for (const auto& command : ui.frame().commands) {
        if (!command.text.empty()) {
            std::cout << (command.primitive == genomes::ui::UiDrawPrimitive::Quad ? "[ ] " : "    ")
                      << command.text << (command.enabled ? "" : " (coming soon)") << '\n';
        }
    }
    std::cout << "\nscene instances: " << presentation.instances.size()
              << ", submitted ui nodes: " << renderer.submitted_ui_nodes()
              << ", rendered frames: " << renderer.frames_started() << '\n';

    // The real SDL adapter will produce this same input frame. The scene
    // remains unaware of the platform and only receives semantic actions.
    director.handle_input({.confirm_pressed = true, .events = {}});
    director.frame_update(1.0 / 60.0);
    std::cout << "after confirm: scene "
              << (director.current() ? director.current()->id() : 0) << '\n';
    std::cout << "world config ui nodes: " << ui.frame().commands.size()
              << ", preview instances: " << presentation.instances.size() << '\n';

    // The default world-config selection is Start game. Confirming it carries
    // the seed and generation controls through the application command queue.
    director.handle_input({.confirm_pressed = true, .events = {}});
    director.frame_update(1.0 / 60.0);
    std::cout << "after world start: scene "
              << (director.current() ? director.current()->id() : 0) << '\n';
    return 0;
}
