#include <genomes/runtime/MainMenuScene.hpp>

#include <genomes/foundation/Types.hpp>
#include <genomes/render/ProceduralMeshes.hpp>

#include <array>
#include <cmath>
#include <string_view>

namespace genomes::runtime {

namespace {

struct EntryDefinition {
    MainMenuEntry entry;
    std::string_view label;
    bool enabled;
};

constexpr std::array<EntryDefinition, 6> entries{{
    {MainMenuEntry::StartScenario, "Battlefield", true},
    {MainMenuEntry::UnitLab, "Unit laboratory", true},
    {MainMenuEntry::BuildingLab, "Building laboratory", true},
    {MainMenuEntry::WorldConfig, "World configuration", true},
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
    if (input.mouse_left_pressed && input.mouse_x >= 78.0F && input.mouse_x <= 438.0F) {
        constexpr float first_button_y = 184.0F;
        constexpr float button_step = 60.0F;
        const float relative_y = input.mouse_y - first_button_y;
        const int index = static_cast<int>(relative_y / button_step);
        const float local_y = relative_y - static_cast<float>(index) * button_step;
        if (index >= 0 && index < static_cast<int>(entries.size()) && local_y >= 0.0F &&
            local_y <= 48.0F) {
            state_.selected = entries[static_cast<std::size_t>(index)].entry;
            if (entries[static_cast<std::size_t>(index)].enabled) {
                activate(context);
            }
            return;
        }
    }
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

    context.ui.add({foundation::stable_id("menu.panel"), ui::UiWidgetType::Panel,
                    "GENOMES", true, false, 420.0F, 640.0F});
    context.ui.add({foundation::stable_id("menu.title"), ui::UiWidgetType::Label,
                    "PROCEDURAL WORLD", true, false, 0.0F, 0.0F});

    for (const EntryDefinition& definition : entries) {
        context.ui.add({foundation::stable_id(definition.label), ui::UiWidgetType::Button,
                        std::string(definition.label), definition.enabled,
                        definition.entry == state_.selected, 360.0F, 48.0F});
    }

    context.ui.add({foundation::stable_id("menu.separator"), ui::UiWidgetType::Separator,
                    {}, true, false, 360.0F, 1.0F});
    context.ui.add({foundation::stable_id("menu.version"), ui::UiWidgetType::Label,
                    "native runtime / scene architecture", true, false, 0.0F, 0.0F});

    if (state_.settings_open) {
        context.ui.add({foundation::stable_id("menu.settings"), ui::UiWidgetType::Panel,
                        "Settings", true, false, 360.0F, 160.0F});
    }
}

void MainMenuScene::build_presentation(SceneContext& context) {
    context.presentation.clear_scene_payload();

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
    switch (state_.selected) {
    case MainMenuEntry::StartScenario:
        context.commands.push({ApplicationCommandKind::OpenWorldConfig,
                               WorldGenerationConfig{state_.preview_seed}});
        break;
    case MainMenuEntry::UnitLab:
        context.commands.push({ApplicationCommandKind::OpenUnitLab,
                               WorldGenerationConfig{state_.preview_seed}});
        break;
    case MainMenuEntry::BuildingLab:
        context.commands.push({ApplicationCommandKind::OpenBuildingLab,
                               WorldGenerationConfig{state_.preview_seed}});
        break;
    case MainMenuEntry::WorldConfig:
        context.commands.push({ApplicationCommandKind::OpenWorldConfig,
                               WorldGenerationConfig{state_.preview_seed}});
        break;
    case MainMenuEntry::Settings:
        state_.settings_open = !state_.settings_open;
        break;
    case MainMenuEntry::Quit:
        context.commands.push({ApplicationCommandKind::Quit, {}});
        break;
    case MainMenuEntry::Count:
        break;
    }
}

} // namespace genomes::runtime
