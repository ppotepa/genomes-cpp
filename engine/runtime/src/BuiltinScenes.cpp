#include <genomes/game_scenes/BuiltinScenes.hpp>

#include <genomes/foundation/StableHash.hpp>
#include <genomes/runtime/BattlefieldScene.hpp>
#include <genomes/runtime/BuildingLabScene.hpp>
#include <genomes/runtime/MainMenuScene.hpp>
#include <genomes/runtime/Scene.hpp>
#include <genomes/runtime/SceneDirector.hpp>
#if GENOMES_HAS_INFANTRY
#include <genomes/runtime/UnitLabScene.hpp>
#endif
#include <genomes/runtime/WorldConfigScene.hpp>
#include <genomes/runtime/WorldLabScene.hpp>

#include <memory>
#include <array>
#include <charconv>
#include <cmath>
#include <algorithm>
#include <string>
#include <utility>
#include <variant>

namespace genomes::runtime {

namespace {

class PlaceholderScene final : public Scene {
public:
    PlaceholderScene(foundation::SceneId scene_id, std::string title)
        : scene_id_{scene_id}, title_{std::move(title)} {}

    [[nodiscard]] foundation::SceneId id() const noexcept override { return scene_id_; }

    void frame_update(SceneContext& context, double) override {
        context.ui.clear();
        (void)context.ui.model().set("title", title_);
        (void)context.ui.model().set("description",
            std::string{"Scene registered; domain module will provide its content."});
    }

private:
    foundation::SceneId scene_id_;
    std::string title_;
};

[[nodiscard]] std::string scalePercent(double scale) {
    std::array<char, 16> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(),
                                      static_cast<int>(std::lround(scale * 100.0)));
    return result.ec == std::errc{} ? std::string{buffer.data(), result.ptr} + "%"
                                    : std::string{};
}

} // namespace

void configureBuiltinSceneRouting(SceneDirector& director) {
    const auto menu_id = foundation::scene_id("scene.main-menu");
    const auto world_config_id = foundation::scene_id("scene.world-config");
    const auto battlefield_id = foundation::scene_id("scene.battlefield");
    const auto unit_lab_id = foundation::scene_id("scene.unit-lab");
    const auto building_lab_id = foundation::scene_id("scene.building-lab");
    const auto world_lab_id = foundation::scene_id("scene.world-lab");
    const auto settings_id = foundation::scene_id("scene.settings");
    const auto pause_id = foundation::scene_id("scene.pause");

    director.set_application_action_router([&director](ui::UiActionId action,
                                                       const ui::UiActionArguments& arguments) {
        auto& ui = director.ui_runtime();
        const auto push = [&director](ApplicationCommandKind kind) {
            director.enqueue_command({kind, {}});
            return ui::UiActionResult::Handled;
        };
        if (action == foundation::stable_id("scene.start-battlefield"))
            return push(ApplicationCommandKind::StartScenario);
        if (action == foundation::stable_id("scene.open-unit-lab"))
            return push(ApplicationCommandKind::OpenUnitLab);
        if (action == foundation::stable_id("scene.open-building-lab"))
            return push(ApplicationCommandKind::OpenBuildingLab);
        if (action == foundation::stable_id("scene.open-world-config"))
            return push(ApplicationCommandKind::OpenWorldConfig);
        if (action == foundation::stable_id("scene.open-world-lab"))
            return push(ApplicationCommandKind::OpenWorldLab);
        if (action == foundation::stable_id("scene.open-settings")) {
            if (ui.routes().top() != nullptr && ui.routes().top()->overlay)
                return ui::UiActionResult::Rejected;
            (void)ui.model().set("ui_scale", director.session_ui_scale());
            (void)ui.model().set("ui_scale_percent", scalePercent(director.session_ui_scale()));
            (void)ui.model().set("show_diagnostics", director.session_show_diagnostics());
            ui.routes().push({foundation::scene_id("scene.settings"), {}, {}, {}, true});
            return ui::UiActionResult::Handled;
        }
        if (action == foundation::stable_id("scene.open-pause")) {
            if (director.current_scene_id() != foundation::scene_id("scene.battlefield") ||
                (ui.routes().top() != nullptr && ui.routes().top()->overlay))
                return ui::UiActionResult::Rejected;
            ui.routes().push({foundation::scene_id("scene.pause"), {}, {}, {}, true});
            return ui::UiActionResult::Handled;
        }
        if (action == foundation::stable_id("scene.return-main-menu")) {
            if (ui.routes().top() != nullptr &&
                ui.routes().top()->scene == foundation::scene_id("scene.pause")) {
                return push(ApplicationCommandKind::ReturnToMainMenu);
            }
            if (ui.routes().top() != nullptr && ui.routes().top()->overlay) {
                ui.routes().pop();
                return ui::UiActionResult::Handled;
            }
            return push(ApplicationCommandKind::ReturnToMainMenu);
        }
        if (action == foundation::stable_id("scene.resume")) {
            if (ui.routes().top() != nullptr &&
                ui.routes().top()->scene == foundation::scene_id("scene.pause")) {
                ui.routes().pop();
                return ui::UiActionResult::Handled;
            }
            return ui::UiActionResult::Rejected;
        }
        if (action == foundation::stable_id("scene.close-overlay")) {
            if (ui.routes().top() == nullptr || !ui.routes().top()->overlay)
                return ui::UiActionResult::Rejected;
            ui.routes().pop();
            return ui::UiActionResult::Handled;
        }
        if (action == foundation::stable_id("settings.defaults")) {
            (void)ui.model().set("ui_scale", 1.0);
            (void)ui.model().set("ui_scale_percent", std::string{"100%"});
            (void)ui.model().set("show_diagnostics", true);
            return ui::UiActionResult::Handled;
        }
        if (action == foundation::stable_id("settings.ui-scale")) {
            for (const auto& argument : arguments) if (argument.first == "value") {
                double value = 0.0;
                const auto converted = std::from_chars(
                    argument.second.data(), argument.second.data() + argument.second.size(),
                    value, std::chars_format::general);
                if (converted.ec != std::errc{} ||
                    converted.ptr != argument.second.data() + argument.second.size() ||
                    !std::isfinite(value)) return ui::UiActionResult::Rejected;
                const double clamped = std::clamp(value, 0.75, 1.50);
                (void)ui.model().set("ui_scale", clamped);
                (void)ui.model().set("ui_scale_percent", scalePercent(clamped));
                return ui::UiActionResult::Handled;
            }
            return ui::UiActionResult::Rejected;
        }
        if (action == foundation::stable_id("settings.show-diagnostics")) {
            for (const auto& argument : arguments) if (argument.first == "value") {
                (void)ui.model().set("show_diagnostics",
                                     argument.second == "true" || argument.second == "1");
                return ui::UiActionResult::Handled;
            }
            return ui::UiActionResult::Rejected;
        }
        if (action == foundation::stable_id("settings.cancel")) {
            if (ui.routes().top() != nullptr && ui.routes().top()->overlay) {
                (void)ui.model().set("ui_scale", director.session_ui_scale());
                (void)ui.model().set("ui_scale_percent", scalePercent(director.session_ui_scale()));
                (void)ui.model().set("show_diagnostics", director.session_show_diagnostics());
                ui.routes().pop();
                return ui::UiActionResult::Handled;
            }
            return ui::UiActionResult::Rejected;
        }
        if (action == foundation::stable_id("settings.apply")) {
            if (const auto* value = ui.model().find("ui_scale"); value != nullptr &&
                std::holds_alternative<double>(*value))
                director.set_session_ui_scale(std::get<double>(*value));
            if (const auto* value = ui.model().find("show_diagnostics"); value != nullptr &&
                std::holds_alternative<bool>(*value))
                director.set_session_show_diagnostics(std::get<bool>(*value));
            if (ui.routes().top() != nullptr && ui.routes().top()->overlay) ui.routes().pop();
            return ui::UiActionResult::Handled;
        }
        if (action == foundation::stable_id("application.quit"))
            return push(ApplicationCommandKind::Quit);
        return ui::UiActionResult::Unknown;
    });

    director.set_application_command_handler([&director, menu_id, world_config_id,
                                              battlefield_id, unit_lab_id, building_lab_id,
                                              world_lab_id](const ApplicationCommand& command) {
        switch (command.kind) {
        case ApplicationCommandKind::StartScenario:
            director.set_active_world_config(command.world_config);
            (void)director.start(battlefield_id);
            break;
        case ApplicationCommandKind::OpenWorldConfig:
            director.set_active_world_config(command.world_config);
            (void)director.start(world_config_id);
            break;
        case ApplicationCommandKind::OpenUnitLab: (void)director.start(unit_lab_id); break;
        case ApplicationCommandKind::OpenBuildingLab: (void)director.start(building_lab_id); break;
        case ApplicationCommandKind::OpenWorldLab: (void)director.start(world_lab_id); break;
        case ApplicationCommandKind::ReturnToMainMenu: (void)director.start(menu_id); break;
        case ApplicationCommandKind::OpenSettings:
        case ApplicationCommandKind::OpenPause:
            break;
        case ApplicationCommandKind::Quit: director.request_quit(); break;
        }
    });

}

void registerBuiltinScenes(SceneDirector& director, BuiltinSceneConfig config) {
    configureBuiltinSceneRouting(director);
    const auto menu_id = foundation::scene_id("scene.main-menu");
    const auto world_config_id = foundation::scene_id("scene.world-config");
    const auto battlefield_id = foundation::scene_id("scene.battlefield");
    const auto unit_lab_id = foundation::scene_id("scene.unit-lab");
    const auto building_lab_id = foundation::scene_id("scene.building-lab");
    const auto world_lab_id = foundation::scene_id("scene.world-lab");
    const auto settings_id = foundation::scene_id("scene.settings");
    const auto pause_id = foundation::scene_id("scene.pause");

    director.register_scene(menu_id, [] { return std::make_unique<MainMenuScene>(); });
    director.register_scene(world_config_id,
                            [] { return std::make_unique<WorldConfigScene>(); });
    if (config.real_battlefield) {
#if GENOMES_HAS_INFANTRY
        const auto tactical_ai_profile = config.tactical_ai_profile.value_or(combat::TacticalAIProfile{});
        director.register_scene(battlefield_id, [tactical_ai_profile] {
            return std::make_unique<BattlefieldScene>(tactical_ai_profile);
        });
#else
        director.register_scene(battlefield_id,
                                [] { return std::make_unique<BattlefieldScene>(); });
#endif
    } else {
        director.register_scene(battlefield_id, [battlefield_id] {
            return std::make_unique<PlaceholderScene>(battlefield_id,
                                                      "Battlefield loading boundary");
        });
    }
#if GENOMES_HAS_INFANTRY
    director.register_scene(unit_lab_id, [] { return std::make_unique<UnitLabScene>(); });
#else
    director.register_unavailable_scene(
        unit_lab_id, {foundation::ErrorCode::UnavailableFeature,
                      "unit laboratory requires the infantry module"});
#endif
    director.register_scene(building_lab_id,
                            [] { return std::make_unique<BuildingLabScene>(); });
    director.register_scene(world_lab_id, [] { return std::make_unique<WorldLabScene>(); });
    director.register_scene(settings_id, [settings_id] {
        return std::make_unique<PlaceholderScene>(settings_id, "Settings");
    });
    director.register_scene(pause_id, [pause_id] {
        return std::make_unique<PlaceholderScene>(pause_id, "Pause");
    });
}

void registerBuiltinScenes(SceneDirector& director, bool real_battlefield) {
    registerBuiltinScenes(director, BuiltinSceneConfig{.real_battlefield = real_battlefield});
}

} // namespace genomes::runtime
