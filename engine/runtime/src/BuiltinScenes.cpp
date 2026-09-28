#include <genomes/runtime/BuiltinScenes.hpp>

#include <genomes/foundation/StableHash.hpp>
#include <genomes/runtime/BattlefieldScene.hpp>
#include <genomes/runtime/BuildingLabScene.hpp>
#include <genomes/runtime/MainMenuScene.hpp>
#include <genomes/runtime/Scene.hpp>
#include <genomes/runtime/SceneDirector.hpp>
#include <genomes/runtime/UnitLabScene.hpp>
#include <genomes/runtime/WorldConfigScene.hpp>

#include <memory>
#include <string>
#include <utility>

namespace genomes::runtime {

namespace {

class PlaceholderScene final : public Scene {
public:
    PlaceholderScene(foundation::SceneId scene_id, std::string title)
        : scene_id_{scene_id}, title_{std::move(title)} {}

    [[nodiscard]] foundation::SceneId id() const noexcept override { return scene_id_; }

    void frame_update(SceneContext& context, double) override {
        context.ui.clear();
        context.ui.add({foundation::stable_id("placeholder.panel"), ui::UiNodeType::Panel,
                        title_, true, false, 720.0F, 480.0F});
        context.ui.add({foundation::stable_id("placeholder.description"),
                        ui::UiNodeType::Label,
                        "Scene registered; domain module will provide its content.", true, false,
                        0.0F, 0.0F});
    }

private:
    foundation::SceneId scene_id_;
    std::string title_;
};

} // namespace

void registerBuiltinScenes(SceneDirector& director, bool real_battlefield) {
    const auto menu_id = foundation::scene_id("scene.main-menu");
    const auto world_config_id = foundation::scene_id("scene.world-config");
    const auto battlefield_id = foundation::scene_id("scene.battlefield");
    const auto unit_lab_id = foundation::scene_id("scene.unit-lab");
    const auto building_lab_id = foundation::scene_id("scene.building-lab");
    const auto world_lab_id = foundation::scene_id("scene.world-lab");

    director.register_scene(menu_id, [] { return std::make_unique<MainMenuScene>(); });
    director.register_scene(world_config_id,
                            [] { return std::make_unique<WorldConfigScene>(); });
    if (real_battlefield) {
        director.register_scene(battlefield_id,
                                [] { return std::make_unique<BattlefieldScene>(); });
    } else {
        director.register_scene(battlefield_id, [battlefield_id] {
            return std::make_unique<PlaceholderScene>(battlefield_id,
                                                      "Battlefield loading boundary");
        });
    }
    director.register_scene(unit_lab_id, [] { return std::make_unique<UnitLabScene>(); });
    director.register_scene(building_lab_id,
                            [] { return std::make_unique<BuildingLabScene>(); });
    director.register_scene(world_lab_id, [world_lab_id] {
        return std::make_unique<PlaceholderScene>(world_lab_id, "World generator");
    });
}

} // namespace genomes::runtime
