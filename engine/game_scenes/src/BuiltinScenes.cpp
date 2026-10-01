#include <genomes/game_scenes/BuiltinScenes.hpp>
#include <genomes/game_scenes/ApplicationCommand.hpp>

#include <genomes/foundation/StableHash.hpp>
#include <genomes/game_scenes/BattlefieldScene.hpp>
#include <genomes/game_scenes/BuildingLabScene.hpp>
#include <genomes/game_scenes/MainMenuScene.hpp>
#include <genomes/runtime/Scene.hpp>
#include <genomes/runtime/SceneDirector.hpp>
#if GENOMES_HAS_INFANTRY
#include <genomes/game_scenes/UnitLabScene.hpp>
#endif
#include <genomes/game_scenes/WorldConfigScene.hpp>
#include <genomes/game_scenes/WorldLabScene.hpp>

#include <memory>
#include <array>
#include <charconv>
#include <cmath>
#include <algorithm>
#include <string>
#include <stdexcept>
#include <utility>
#include <variant>

namespace genomes::application {

using runtime::Scene;
using runtime::SceneContext;
using runtime::SceneDirector;

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

void configureBuiltinSceneRouting(
    SceneDirector& director,
    std::shared_ptr<WorldGenerationConfig> active_world_config) {
    director.set_application_action_router([&director](ui::UiActionId action,
                                                       const ui::UiActionArguments& arguments) {
        auto& ui = director.ui_runtime();
        const auto push = [&director](application::ApplicationCommandKind kind) {
            director.enqueue_command(application::makeApplicationCommand(kind));
            return ui::UiActionResult::Handled;
        };
        if (action == foundation::stable_id("scene.start-battlefield"))
            return push(application::ApplicationCommandKind::StartScenario);
        if (action == foundation::stable_id("scene.open-unit-lab"))
            return push(application::ApplicationCommandKind::OpenUnitLab);
        if (action == foundation::stable_id("scene.open-building-lab"))
            return push(application::ApplicationCommandKind::OpenBuildingLab);
        if (action == foundation::stable_id("scene.open-world-config"))
            return push(application::ApplicationCommandKind::OpenWorldConfig);
        if (action == foundation::stable_id("scene.open-world-lab"))
            return push(application::ApplicationCommandKind::OpenWorldLab);
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
                return push(application::ApplicationCommandKind::ReturnToMainMenu);
            }
            if (ui.routes().top() != nullptr && ui.routes().top()->overlay) {
                ui.routes().pop();
                return ui::UiActionResult::Handled;
            }
            return push(application::ApplicationCommandKind::ReturnToMainMenu);
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
            return push(application::ApplicationCommandKind::Quit);
        return ui::UiActionResult::Unknown;
    });

    director.set_scene_command_handler(
        [&director, active_world_config](runtime::SceneCommandPtr command) {
        const auto menu_id = foundation::scene_id("scene.main-menu");
        const auto world_config_id = foundation::scene_id("scene.world-config");
        const auto battlefield_id = foundation::scene_id("scene.battlefield");
        const auto unit_lab_id = foundation::scene_id("scene.unit-lab");
        const auto building_lab_id = foundation::scene_id("scene.building-lab");
        const auto world_lab_id = foundation::scene_id("scene.world-lab");
        const auto* application_command =
            dynamic_cast<const application::ApplicationCommand*>(command.get());
        if (application_command == nullptr) {
            return;
        }
        switch (application_command->kind) {
        case application::ApplicationCommandKind::StartScenario:
            if (active_world_config != nullptr)
                *active_world_config = application_command->world_config;
            (void)director.start(battlefield_id);
            break;
        case application::ApplicationCommandKind::OpenWorldConfig:
            if (active_world_config != nullptr)
                *active_world_config = application_command->world_config;
            (void)director.start(world_config_id);
            break;
        case application::ApplicationCommandKind::OpenUnitLab:
            (void)director.start(unit_lab_id); break;
        case application::ApplicationCommandKind::OpenBuildingLab:
            (void)director.start(building_lab_id); break;
        case application::ApplicationCommandKind::OpenWorldLab:
            (void)director.start(world_lab_id); break;
        case application::ApplicationCommandKind::ReturnToMainMenu:
            (void)director.start(menu_id); break;
        case application::ApplicationCommandKind::OpenSettings:
        case application::ApplicationCommandKind::OpenPause:
            break;
        case application::ApplicationCommandKind::Quit: director.request_quit(); break;
        }
        });

}

BuiltinSceneCatalog::BuiltinSceneCatalog(BuiltinSceneConfig config) {
    const auto world_profile = config.world_generation_profile;
    if (world_profile == nullptr || !world_profile->frozen() ||
        config.building_profile == nullptr || !config.building_profile->frozen()) {
        throw std::invalid_argument{"built-in scenes require frozen world and building profiles"};
    }
    active_world_config_ = std::make_shared<WorldGenerationConfig>(
        world_profile->makeDefaultRequest());
    const auto menu_id = foundation::scene_id("scene.main-menu");
    const auto world_config_id = foundation::scene_id("scene.world-config");
    const auto battlefield_id = foundation::scene_id("scene.battlefield");
    const auto unit_lab_id = foundation::scene_id("scene.unit-lab");
    const auto building_lab_id = foundation::scene_id("scene.building-lab");
    const auto world_lab_id = foundation::scene_id("scene.world-lab");
    const auto settings_id = foundation::scene_id("scene.settings");
    const auto pause_id = foundation::scene_id("scene.pause");

    entries_.push_back({menu_id, [world_profile] {
        return std::make_unique<game_scenes::MainMenuScene>(world_profile);
    }});
    const auto active_world_config = active_world_config_;
    entries_.push_back({world_config_id,
                        [active_world_config, world_profile] {
                            return std::make_unique<game_scenes::WorldConfigScene>(
                                world_profile, *active_world_config);
                        }});
    const auto building_profile = config.building_profile;
    if (config.real_battlefield) {
#if GENOMES_HAS_INFANTRY
        const auto tactical_ai_profile =
            config.tactical_ai_profile.value_or(combat::TacticalAIProfile{});
        entries_.push_back({battlefield_id, [active_world_config, tactical_ai_profile,
                                             building_profile] {
            return std::make_unique<game_scenes::BattlefieldScene>(*active_world_config,
                                                      building_profile,
                                                      tactical_ai_profile);
        }});
#else
        entries_.push_back({battlefield_id, [active_world_config, building_profile] {
            return std::make_unique<game_scenes::BattlefieldScene>(*active_world_config,
                                                               building_profile);
        }});
#endif
    } else {
        entries_.push_back({battlefield_id, [battlefield_id] {
            return std::make_unique<PlaceholderScene>(battlefield_id,
                                                      "Battlefield loading boundary");
        }});
    }
#if GENOMES_HAS_INFANTRY
    const auto appearance_catalog = config.appearance_catalog;
    entries_.push_back({unit_lab_id, [appearance_catalog] {
        return std::make_unique<game_scenes::UnitLabScene>(appearance_catalog);
    }});
#else
    (void)unit_lab_id;
#endif
    entries_.push_back({building_lab_id, [building_profile] {
        return std::make_unique<game_scenes::BuildingLabScene>(building_profile);
    }});
    entries_.push_back({world_lab_id, [] { return std::make_unique<game_scenes::WorldLabScene>(); }});
    entries_.push_back({settings_id, [settings_id] {
        return std::make_unique<PlaceholderScene>(settings_id, "Settings");
    }});
    entries_.push_back({pause_id, [pause_id] {
        return std::make_unique<PlaceholderScene>(pause_id, "Pause");
    }});
}

void BuiltinSceneCatalog::install(runtime::SceneDirector& director) const {
    configureBuiltinSceneRouting(director, active_world_config_);
    for (const auto& entry : entries_) director.register_scene(entry.id, entry.factory);
#if !GENOMES_HAS_INFANTRY
    director.register_unavailable_scene(
        foundation::scene_id("scene.unit-lab"),
        {foundation::ErrorCode::UnavailableFeature,
         "unit laboratory requires the infantry module"});
#endif
}

void registerBuiltinScenes(runtime::SceneDirector& director, BuiltinSceneConfig config) {
    BuiltinSceneCatalog{std::move(config)}.install(director);
}

} // namespace genomes::application
