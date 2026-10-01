#include <genomes/render/NullRenderer.hpp>
#include <genomes/runtime/MainMenuScene.hpp>
#include <genomes/runtime/SceneDirector.hpp>
#include <genomes/runtime/WorldLabScene.hpp>

#include <cassert>
#include <memory>
#include <string>

int main() {
    genomes::render::NullRenderer renderer;
    genomes::ui::UiRuntime ui;
    genomes::render::PresentationSnapshot presentation;
    genomes::runtime::SceneDirector director(renderer, ui, presentation);
    const auto menu = genomes::foundation::scene_id("scene.main-menu");
    const auto lab = genomes::foundation::scene_id("scene.world-lab");
    director.register_scene(menu, [] { return std::make_unique<genomes::runtime::MainMenuScene>(); });
    director.register_scene(lab, [] { return std::make_unique<genomes::runtime::WorldLabScene>(); });
    assert(director.start(menu));
    assert(director.dispatch_ui_action(genomes::foundation::stable_id("scene.open-world-lab"), {}) ==
           genomes::ui::UiActionResult::Handled);
    assert(director.current() != nullptr && director.current()->id() == lab);
    director.frame_update(1.0 / 60.0);
    const auto* status = ui.model().find("prototype_status");
    assert(status != nullptr && std::get<std::string>(*status).find("not applied") != std::string::npos);
    assert(director.dispatch_ui_action(genomes::foundation::stable_id("worldlab.category"),
                                       {{"value", "Flora"}}) == genomes::ui::UiActionResult::Handled);
    director.frame_update(1.0 / 60.0);
    assert(std::get<std::string>(*ui.model().find("category")) == "Flora");
    assert(director.dispatch_ui_action(genomes::foundation::stable_id("scene.return-main-menu"), {}) ==
           genomes::ui::UiActionResult::Handled);
    assert(director.current() != nullptr && director.current()->id() == menu);
    return 0;
}
