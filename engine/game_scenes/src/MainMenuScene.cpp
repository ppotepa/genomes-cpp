#include <genomes/game_scenes/MainMenuScene.hpp>
#include <genomes/game_scenes/ApplicationCommand.hpp>

#include <genomes/foundation/Types.hpp>
#include <genomes/render/ProceduralMeshes.hpp>

#include <array>
#include <cmath>
#include <string_view>

namespace genomes::game_scenes {

namespace {

struct EntryDefinition {
    MainMenuEntry entry;
    std::string_view label;
    bool enabled;
};

constexpr std::array<EntryDefinition, 7> entries{{
    {MainMenuEntry::StartScenario, "Battlefield", true},
    {MainMenuEntry::MassBattle, "Infantry Mass Battle", true},
    {MainMenuEntry::UnitLab, "Unit Laboratory", true},
    {MainMenuEntry::BuildingLab, "Building Laboratory", true},
    {MainMenuEntry::WorldLab, "World Laboratory", true},
    {MainMenuEntry::Settings, "Settings", true},
    {MainMenuEntry::Quit, "Quit", true},
}};

[[nodiscard]] const EntryDefinition& definition_for(MainMenuEntry entry) {
    for (const EntryDefinition& definition : entries) {
        if (definition.entry == entry) {
            return definition;
        }
    }
    return entries.front();
}

[[nodiscard]] MainMenuEntry move_selection(MainMenuEntry current, int direction) {
    const auto count = static_cast<int>(MainMenuEntry::Count);
    int index = static_cast<int>(current);
    for (int attempt = 0; attempt < count; ++attempt) {
        index = (index + direction + count) % count;
        const auto candidate = static_cast<MainMenuEntry>(index);
        if (definition_for(candidate).enabled) {
            return candidate;
        }
    }
    return current;
}

} // namespace

foundation::SceneId MainMenuScene::id() const noexcept {
    return foundation::scene_id("scene.main-menu");
}

void MainMenuScene::on_enter(SceneContext& context) {
    state_.preview_time = 0.0;
    state_.settings_open = false;
    context.ui.clear();
}

void MainMenuScene::fixed_update(SceneContext&, double dt) {
    state_.preview_time += dt;
}

void MainMenuScene::handle_input(SceneContext& context, const input::InputFrame& input) {
    if (input.up_pressed) {
        state_.selected = move_selection(state_.selected, -1);
    } else if (input.down_pressed) {
        state_.selected = move_selection(state_.selected, 1);
    }
    if (input.confirm_pressed && definition_for(state_.selected).enabled) {
        activate(context);
    }
}

void MainMenuScene::frame_update(SceneContext& context, double) {
    context.ui.clear();
    auto& model = context.ui.model();
    (void)model.set("title", std::string{"PROCEDURAL WORLD"});
    (void)model.set("selected", std::string{definition_for(state_.selected).label});
    (void)model.set("version", std::string{"native runtime / scene architecture"});
    (void)model.set("settings_open", state_.settings_open);
    std::vector<ui::UiTableRow> menu_entries;
    menu_entries.reserve(entries.size());
    for (const auto& entry : entries) {
        menu_entries.push_back({{"id", std::string{entry.label}},
                                {"label", std::string{entry.label}},
                                {"enabled", entry.enabled}});
    }
    (void)model.set_list("entries", std::move(menu_entries));
}

void MainMenuScene::build_presentation(SceneContext& context) {
    if (preview_prototypes_.empty()) {
        preview_prototypes_.push_back(render::procedural::make_box(
            foundation::stable_id("mesh.preview.terrain"), {3.0F, 0.10F, 3.0F},
            {0.20F, 0.42F, 0.24F, 1.0F}));
        preview_prototypes_.push_back(render::procedural::make_box(
            foundation::stable_id("mesh.preview.building"), {0.90F, 1.40F, 0.90F},
            {0.60F, 0.36F, 0.20F, 1.0F}));
        preview_prototypes_.push_back(render::procedural::make_box(
            foundation::stable_id("mesh.preview.unit"), {0.30F, 0.80F, 0.30F},
            {0.20F, 0.42F, 0.88F, 1.0F}));
    }
    context.presentation.instance_prototypes = preview_prototypes_;

    const float time = static_cast<float>(state_.preview_time);
    const float camera_motion = std::sin(time * 0.18F) * 0.25F;

    // These are stable semantic render IDs. The future Diligent renderer will
    // resolve them through its resource manager; the menu scene does not own
    // GPU objects.
    context.presentation.instances.push_back({
        foundation::stable_id("menu.preview.terrain"),
        foundation::stable_id("mesh.preview.terrain"),
        foundation::stable_id("material.preview.ground"),
        {0.0F, 0.10F, 0.0F}, {1.0F, 1.0F, 1.0F}, camera_motion, 0,
        render::RenderInstanceFlagPreview});
    context.presentation.instances.push_back({
        foundation::stable_id("menu.preview.building"),
        foundation::stable_id("mesh.preview.building"),
        foundation::stable_id("material.preview.building"),
        {2.0F, 1.40F, -2.0F}, {1.0F, 1.0F, 1.0F}, -0.12F, 0,
        render::RenderInstanceFlagPreview});
    context.presentation.instances.push_back({
        foundation::stable_id("menu.preview.unit"),
        foundation::stable_id("mesh.preview.unit"),
        foundation::stable_id("material.preview.unit"),
        {-1.5F, 0.80F, 1.0F}, {1.0F, 1.0F, 1.0F}, 0.4F, 0,
        render::RenderInstanceFlagPreview});
}

void MainMenuScene::select(MainMenuEntry entry) noexcept {
    state_.selected = entry;
}

void MainMenuScene::activate(SceneContext& context) {
    application::WorldGenerationConfig config = world_config_;
    config.seed = state_.preview_seed;
    switch (state_.selected) {
    case MainMenuEntry::StartScenario:
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::OpenWorldConfig,
            config);
        break;
    case MainMenuEntry::MassBattle:
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::OpenMassBattle,
            config);
        break;
    case MainMenuEntry::UnitLab:
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::OpenUnitLab,
            config);
        break;
    case MainMenuEntry::BuildingLab:
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::OpenBuildingLab,
            config);
        break;
    case MainMenuEntry::WorldLab:
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::OpenWorldLab,
            config);
        break;
    case MainMenuEntry::Settings:
        state_.settings_open = !state_.settings_open;
        break;
    case MainMenuEntry::Quit:
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::Quit);
        break;
    case MainMenuEntry::Count:
        break;
    }
}

} // namespace genomes::game_scenes
