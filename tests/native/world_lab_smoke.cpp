#include <genomes/render/NullRenderer.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/game_scenes/BuiltinScenes.hpp>
#include <genomes/game_scenes/MainMenuScene.hpp>
#include <genomes/runtime/SceneDirector.hpp>
#include <genomes/game_scenes/WorldLabScene.hpp>
#include <genomes/world/WorldGenerationProfile.hpp>

#include <cassert>
#include <filesystem>
#include <memory>
#include <string>

int main() {
    const auto loaded_world_profile = genomes::world::loadWorldGenerationProfile(
        std::filesystem::path{GENOMES_SOURCE_DIR} /
        "mods/core/profiles/world-generation.json");
    assert(loaded_world_profile);
    const auto world_profile =
        std::make_shared<const genomes::world::FrozenWorldGenerationProfile>(
            loaded_world_profile.value());
    auto active_world_config =
        std::make_shared<genomes::application::WorldGenerationConfig>(
            world_profile->makeDefaultRequest());
    genomes::render::NullRenderer renderer;
    genomes::ui::UiRuntime ui;
    genomes::render::PresentationSnapshot presentation;
    genomes::jobs::JobSystem jobs{2U};
    genomes::runtime::SceneDirector director(renderer, ui, presentation, jobs);
    genomes::application::configureBuiltinSceneRouting(director, active_world_config);
    const auto menu = genomes::foundation::scene_id("scene.main-menu");
    const auto lab = genomes::foundation::scene_id("scene.world-lab");
    director.register_scene(menu, [world_profile] {
        return std::make_unique<genomes::game_scenes::MainMenuScene>(world_profile);
    });
    director.register_scene(lab, [] { return std::make_unique<genomes::game_scenes::WorldLabScene>(); });
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
