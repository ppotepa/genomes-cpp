#include <genomes/runtime/WorldConfigScene.hpp>

#include <genomes/foundation/Types.hpp>
#include <genomes/render/ProceduralMeshes.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace genomes::runtime {

namespace {

constexpr std::array<std::uint32_t, 4> map_sizes{{400, 600, 800, 1200}};

[[nodiscard]] int entryIndex(WorldConfigEntry entry) noexcept {
    return static_cast<int>(entry);
}

[[nodiscard]] std::string percentage(float value) {
    return std::to_string(static_cast<int>(std::lround(value * 100.0F))) + "%";
}

[[nodiscard]] std::string entryLabel(WorldConfigEntry entry,
                                     const WorldGenerationConfig& config) {
    switch (entry) {
    case WorldConfigEntry::Seed:
        return "Seed: " + std::to_string(config.seed);
    case WorldConfigEntry::MapSize:
        return "Map size: " + std::to_string(config.map_size_m) + " x " +
               std::to_string(config.map_size_m) + " m";
    case WorldConfigEntry::Preset:
        return "Preset: village with settlement";
    case WorldConfigEntry::Hydrology:
        switch (config.hydrology_mode) {
        case hydrology::HydrologyMode::Off:
            return "Hydrology: off";
        case hydrology::HydrologyMode::SeededOptional:
            return "Hydrology: seeded optional";
        case hydrology::HydrologyMode::Forced:
            return "Hydrology: forced river";
        }
        return "Hydrology: invalid";
    case WorldConfigEntry::Vegetation:
        return "Vegetation: " + percentage(config.vegetation);
    case WorldConfigEntry::Buildings:
        return "Buildings: " + percentage(config.buildings);
    case WorldConfigEntry::FencedParcels:
        return "Fenced parcels: " + percentage(config.fenced_parcels);
    case WorldConfigEntry::Start:
        return "Start game";
    case WorldConfigEntry::Back:
        return "Back";
    case WorldConfigEntry::Count:
        break;
    }
    return {};
}

} // namespace

foundation::SceneId WorldConfigScene::id() const noexcept {
    return foundation::scene_id("scene.world-config");
}

void WorldConfigScene::on_enter(SceneContext& context) {
    if (context.world_config != nullptr) {
        state_.config = *context.world_config;
    }
    context.ui.clear();
}

void WorldConfigScene::handle_input(SceneContext& context, const input::InputFrame& input) {
    const int count = static_cast<int>(WorldConfigEntry::Count);
    int selected = entryIndex(state_.selected);
    if (input.mouse_left_pressed && input.mouse_x >= 78.0F && input.mouse_x <= 678.0F) {
        float row_y = 198.0F; // description follows the title at renderer row 164
        for (int index = 0; index < count; ++index) {
            if (input.mouse_y >= row_y && input.mouse_y <= row_y + 48.0F) {
                state_.selected = static_cast<WorldConfigEntry>(index);
                if (state_.selected == WorldConfigEntry::Start ||
                    state_.selected == WorldConfigEntry::Back) {
                    activate(context);
                }
                return;
            }
            const auto entry = static_cast<WorldConfigEntry>(index);
            row_y += (entry == WorldConfigEntry::Start || entry == WorldConfigEntry::Back)
                          ? 60.0F
                          : 34.0F;
        }
    }
    if (input.up_pressed) {
        selected = (selected + count - 1) % count;
        state_.selected = static_cast<WorldConfigEntry>(selected);
    } else if (input.down_pressed) {
        selected = (selected + 1) % count;
        state_.selected = static_cast<WorldConfigEntry>(selected);
    }

    if (input.left_pressed) {
        adjust(-1);
    } else if (input.right_pressed) {
        adjust(1);
    }

    if (input.confirm_pressed) {
        activate(context);
    } else if (input.cancel_pressed) {
        context.commands.push({ApplicationCommandKind::ReturnToMainMenu});
    }
}

void WorldConfigScene::fixed_update(SceneContext&, double dt) {
    state_.preview_time += dt;
}

void WorldConfigScene::frame_update(SceneContext& context, double) {
    context.ui.clear();
    context.ui.add({foundation::stable_id("world-config.panel"), ui::UiWidgetType::Panel,
                    "WORLD-GENERATION-1", true, false, 720.0F, 760.0F});
    context.ui.add({foundation::stable_id("world-config.title"), ui::UiWidgetType::Label,
                    "New world", true, false, 0.0F, 0.0F});
    context.ui.add({foundation::stable_id("world-config.description"), ui::UiWidgetType::Label,
                    "A deterministic settlement, roads, parcels and vegetation.", true, false,
                    0.0F, 0.0F});

    for (int index = 0; index < static_cast<int>(WorldConfigEntry::Count); ++index) {
        const auto entry = static_cast<WorldConfigEntry>(index);
        context.ui.add({foundation::stable_id("world-config." + std::to_string(index)),
                        entry == WorldConfigEntry::Start || entry == WorldConfigEntry::Back
                            ? ui::UiWidgetType::Button
                            : ui::UiWidgetType::Label,
                        entryLabel(entry, state_.config), true, entry == state_.selected,
                        600.0F, 48.0F});
    }

    context.ui.add({foundation::stable_id("world-config.note"), ui::UiWidgetType::Label,
                    "The same seed and settings produce the same world plan.", true, false,
                    0.0F, 0.0F});
}

void WorldConfigScene::build_presentation(SceneContext& context) {
    if (preview_prototypes_.empty()) {
        preview_prototypes_.push_back(render::procedural::make_box(
            foundation::stable_id("mesh.preview.terrain"), {3.0F, 0.10F, 3.0F},
            {0.20F, 0.42F, 0.24F, 1.0F}));
        preview_prototypes_.push_back(render::procedural::make_box(
            foundation::stable_id("mesh.preview.building"), {0.90F, 1.40F, 0.90F},
            {0.60F, 0.36F, 0.20F, 1.0F}));
    }
    context.presentation.instance_prototypes = preview_prototypes_;
    const float time = static_cast<float>(state_.preview_time);
    const float motion = std::sin(time * 0.18F) * 0.25F;
    const float scale = static_cast<float>(state_.config.map_size_m) / 600.0F;
    context.presentation.instances.push_back({
        foundation::stable_id("world-config.preview.terrain"),
        foundation::stable_id("mesh.preview.terrain"),
        foundation::stable_id("material.preview.ground"),
        {0.0F, 0.10F, 0.0F}, {scale, 1.0F, scale}, motion, 0,
        render::RenderInstanceFlagPreview});
    context.presentation.instances.push_back({
        foundation::stable_id("world-config.preview.building"),
        foundation::stable_id("mesh.preview.building"),
        foundation::stable_id("material.preview.building"),
        {2.0F, 1.40F, -2.0F}, {1.0F, 1.0F, 1.0F}, -0.12F, 0,
        render::RenderInstanceFlagPreview});
}

void WorldConfigScene::adjust(int direction) noexcept {
    switch (state_.selected) {
    case WorldConfigEntry::Seed:
        if (direction < 0 && state_.config.seed > 0) {
            --state_.config.seed;
        } else if (direction > 0) {
            ++state_.config.seed;
        }
        break;
    case WorldConfigEntry::MapSize: {
        int index = 0;
        for (int i = 0; i < static_cast<int>(map_sizes.size()); ++i) {
            if (map_sizes[static_cast<std::size_t>(i)] == state_.config.map_size_m) {
                index = i;
                break;
            }
        }
        index = std::clamp(index + direction, 0, static_cast<int>(map_sizes.size()) - 1);
        state_.config.map_size_m = map_sizes[static_cast<std::size_t>(index)];
        break;
    }
    case WorldConfigEntry::Hydrology: {
        constexpr int mode_count = 3;
        int mode = static_cast<int>(state_.config.hydrology_mode);
        mode = (mode + direction + mode_count) % mode_count;
        state_.config.hydrology_mode = static_cast<hydrology::HydrologyMode>(mode);
        break;
    }
    case WorldConfigEntry::Preset:
    case WorldConfigEntry::Start:
    case WorldConfigEntry::Back:
    case WorldConfigEntry::Count:
        break;
    case WorldConfigEntry::Vegetation:
        state_.config.vegetation = std::clamp(state_.config.vegetation + direction * 0.05F,
                                              0.0F, 1.0F);
        break;
    case WorldConfigEntry::Buildings:
        state_.config.buildings = std::clamp(state_.config.buildings + direction * 0.05F,
                                             0.0F, 1.0F);
        break;
    case WorldConfigEntry::FencedParcels:
        state_.config.fenced_parcels =
            std::clamp(state_.config.fenced_parcels + direction * 0.05F, 0.0F, 1.0F);
        break;
    }
}

void WorldConfigScene::activate(SceneContext& context) {
    if (state_.selected == WorldConfigEntry::Start) {
        context.commands.push({ApplicationCommandKind::StartScenario,
                               state_.config});
    } else if (state_.selected == WorldConfigEntry::Back) {
        context.commands.push({ApplicationCommandKind::ReturnToMainMenu});
    }
}

} // namespace genomes::runtime
