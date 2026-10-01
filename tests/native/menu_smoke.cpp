#include <genomes/render/NullRenderer.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/game_scenes/BuiltinScenes.hpp>
#include <genomes/runtime/BattlefieldScene.hpp>
#include <genomes/runtime/MainMenuScene.hpp>
#include <genomes/runtime/SceneDirector.hpp>
#include <genomes/runtime/WorldConfigScene.hpp>

#include <cassert>
#include <memory>
#include <thread>
#include <string>

namespace {

class DummyScene final : public genomes::runtime::Scene {
public:
    explicit DummyScene(genomes::foundation::SceneId scene_id) : scene_id_(scene_id) {}

    [[nodiscard]] genomes::foundation::SceneId id() const noexcept override {
        return scene_id_;
    }

    void frame_update(genomes::runtime::SceneContext& context, double) override {
        (void)context.ui.model().set("title", std::string{"test scene"});
    }

private:
    genomes::foundation::SceneId scene_id_;
};

} // namespace

int main() {
    const auto automatic_seed = genomes::runtime::WorldSeedInput::automatic().resolve(0U);
    assert(automatic_seed && automatic_seed.value() != 0U);
    const auto invalid_explicit_seed =
        genomes::runtime::WorldSeedInput::explicitValue(0U).resolve(123U);
    assert(!invalid_explicit_seed);
    {
        genomes::render::NullRenderer unavailable_renderer;
        genomes::ui::UiRuntime unavailable_ui;
        genomes::render::PresentationSnapshot unavailable_presentation;
        genomes::runtime::SceneDirector unavailable_director(
            unavailable_renderer, unavailable_ui, unavailable_presentation);
        const auto unavailable_id = genomes::foundation::scene_id("scene.optional-feature");
        assert(unavailable_director.register_unavailable_scene(
            unavailable_id, {genomes::foundation::ErrorCode::UnavailableFeature,
                             "optional feature is disabled"}));
        assert(!unavailable_director.start(unavailable_id));
        assert(unavailable_director.current() == nullptr);
        assert(unavailable_director.last_error().code ==
               genomes::foundation::ErrorCode::UnavailableFeature);
    }
    {
        genomes::render::NullRenderer catalog_renderer;
        genomes::ui::UiRuntime catalog_ui;
        genomes::render::PresentationSnapshot catalog_presentation;
        genomes::runtime::SceneDirector catalog_director(
            catalog_renderer, catalog_ui, catalog_presentation);
        genomes::runtime::BuiltinSceneCatalog catalog{
            genomes::runtime::BuiltinSceneConfig{.real_battlefield = false}};
        assert(!catalog.entries().empty());
        catalog.install(catalog_director);
        assert(catalog_director.start(genomes::foundation::scene_id("scene.main-menu")));
    }
    genomes::jobs::JobSystem jobs{2};
    genomes::render::NullRenderer renderer;
    genomes::ui::UiRuntime ui;
    genomes::render::PresentationSnapshot presentation;
    genomes::runtime::SceneDirector director(renderer, ui, presentation, &jobs);
    genomes::runtime::configureBuiltinSceneRouting(director);

    const auto menu_id = genomes::foundation::scene_id("scene.main-menu");
    const auto unit_lab_id = genomes::foundation::scene_id("scene.unit-lab");
    const auto world_config_id = genomes::foundation::scene_id("scene.world-config");
    const auto battlefield_id = genomes::foundation::scene_id("scene.battlefield");
    director.register_scene(menu_id, [] {
        return std::make_unique<genomes::runtime::MainMenuScene>();
    });
    const auto duplicate_menu = director.register_scene(menu_id, [] {
        return std::make_unique<DummyScene>(genomes::foundation::scene_id("scene.main-menu"));
    });
    assert(!duplicate_menu);
    assert(duplicate_menu.error().code == genomes::foundation::ErrorCode::InvalidState);
    director.register_scene(unit_lab_id, [unit_lab_id] {
        return std::make_unique<DummyScene>(unit_lab_id);
    });
    director.register_scene(world_config_id, [] {
        return std::make_unique<genomes::runtime::WorldConfigScene>();
    });
    director.register_scene(battlefield_id, [] {
        return std::make_unique<genomes::runtime::BattlefieldScene>();
    });

    assert(director.start(menu_id));
    const auto late_scene = director.register_scene(
        genomes::foundation::scene_id("scene.late-registration"), [] {
            return std::make_unique<DummyScene>(
                genomes::foundation::scene_id("scene.late-registration"));
        });
    assert(!late_scene);
    assert(late_scene.error().code == genomes::foundation::ErrorCode::InvalidState);
    director.fixed_update(1.0 / 60.0);
    director.frame_update(1.0 / 60.0);
    director.present();

    assert(ui.model().find("title") != nullptr);
    assert(presentation.instances.size() == 3);
    assert(renderer.submitted_ui_nodes() == 0);

    director.handle_input({.down_pressed = true, .confirm_pressed = true, .events = {}});
    assert(director.current() != nullptr);
    assert(director.current()->id() == unit_lab_id);

    director.frame_update(1.0 / 60.0);
    director.present();
    assert(ui.model().find("title") != nullptr);
    assert(presentation.instances.empty());
    assert(renderer.frames_started() == 2);
    assert(renderer.submitted_instances() == 3);
    assert(renderer.submitted_ui_nodes() == 0);

    assert(director.start(menu_id));
    director.handle_input({.confirm_pressed = true, .events = {}});
    assert(director.current() != nullptr);
    assert(director.current()->id() == world_config_id);
    director.frame_update(1.0 / 60.0);
    assert(ui.model().find("title") != nullptr);
    assert(presentation.instances.size() == 2);
    director.handle_input({.confirm_pressed = true, .events = {}});
    assert(director.current() != nullptr);
    assert(director.current()->id() == battlefield_id);
    assert(director.active_world_config() != nullptr);
    assert(director.active_world_config()->seed == 0x5EED2026ull);
    assert(director.active_world_config()->map_size_m == 600);
    const auto* battlefield = dynamic_cast<const genomes::runtime::BattlefieldScene*>(
        director.current());
    assert(battlefield != nullptr);
    for (int attempt = 0; attempt < 1000 && battlefield->plan() == nullptr; ++attempt) {
        director.frame_update(1.0 / 60.0);
        std::this_thread::yield();
        battlefield = dynamic_cast<const genomes::runtime::BattlefieldScene*>(
            director.current());
    }
    assert(battlefield != nullptr);
    assert(battlefield->plan() != nullptr);
    assert(!battlefield->plan()->features.empty());
    return 0;
}
