#include <genomes/game_scenes/WorldConfigScene.hpp>
#include <genomes/game_scenes/ApplicationCommand.hpp>

#include <genomes/foundation/Types.hpp>
#include <genomes/render/ProceduralMeshes.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <charconv>
#include <cstdlib>
#include <random>
#include <utility>

namespace genomes::application {

foundation::Result<proc::Seed, foundation::Error> WorldSeedInput::resolve(
    std::uint64_t auto_entropy) const noexcept {
    if (mode == WorldSeedMode::Explicit) {
        if (explicit_seed == 0U) {
            return foundation::Result<proc::Seed, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument, "explicit world seed must be nonzero"});
        }
        return foundation::Result<proc::Seed, foundation::Error>::success(explicit_seed);
    }
    return foundation::Result<proc::Seed, foundation::Error>::success(
        auto_entropy == 0U ? 1U : auto_entropy);
}

} // namespace genomes::application

namespace genomes::runtime {

namespace {

constexpr std::array<std::uint32_t, 4> map_sizes{{400, 600, 800, 1200}};

template <typename T>
[[nodiscard]] std::string numberText(T value) {
    std::array<char, 64> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return result.ec == std::errc{} ? std::string{buffer.data(), result.ptr} : std::string{};
}

[[nodiscard]] int entryIndex(WorldConfigEntry entry) noexcept {
    return static_cast<int>(entry);
}

[[nodiscard]] std::string percentage(float value) {
    return numberText(static_cast<int>(std::lround(value * 100.0F))) + "%";
}

[[nodiscard]] std::string entryLabel(WorldConfigEntry entry,
                                     const WorldGenerationConfig& config,
                                     const WorldSeedInput& seed_input) {
    switch (entry) {
    case WorldConfigEntry::Seed:
        return seed_input.mode == WorldSeedMode::Auto ? "Seed: auto" :
                                                        "Seed: " + numberText(config.seed);
    case WorldConfigEntry::MapSize:
        return "Map size: " + numberText(config.map_size_m) + " x " +
               numberText(config.map_size_m) + " m";
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

[[nodiscard]] std::uint64_t autoSeedEntropy() {
    std::random_device source;
    return (static_cast<std::uint64_t>(source()) << 32U) ^
           static_cast<std::uint64_t>(source());
}

} // namespace

foundation::SceneId WorldConfigScene::id() const noexcept {
    return foundation::scene_id("scene.world-config");
}

void WorldConfigScene::on_enter(SceneContext& context) {
    state_.config = initial_config_;
    state_.seed_input = state_.config.seed == 0U ? WorldSeedInput::automatic() :
                                                    WorldSeedInput::explicitValue(state_.config.seed);
    context.ui.clear();
}

void WorldConfigScene::handle_input(SceneContext& context, const input::InputFrame& input) {
    const int count = static_cast<int>(WorldConfigEntry::Count);
    int selected = entryIndex(state_.selected);
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
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::ReturnToMainMenu);
    }
}

void WorldConfigScene::fixed_update(SceneContext&, double dt) {
    state_.preview_time += dt;
}

void WorldConfigScene::frame_update(SceneContext& context, double) {
    context.ui.clear();
    auto& model = context.ui.model();
    (void)model.set("title", std::string{"New world"});
    (void)model.set("description", std::string{"A deterministic settlement, roads, parcels and vegetation."});
    ui::UiFieldState seed{};
    seed.value = state_.seed_input.mode == WorldSeedMode::Auto ? std::string{"auto"} :
                                                                 numberText(state_.config.seed);
    seed.commit_policy = ui::UiCommitPolicy::OnChange; seed.minimum = 0.0;
    (void)model.set_field("seed", std::move(seed));
    ui::UiFieldState size{}; size.value = static_cast<std::int64_t>(state_.config.map_size_m);
    size.commit_policy = ui::UiCommitPolicy::OnChange;
    size.options = {{"400", "400m", true}, {"600", "600m", true},
                    {"800", "800m", true}, {"1200", "1200m", true}};
    (void)model.set_field("map_size", std::move(size));
    ui::UiFieldState vegetation{}; vegetation.value = static_cast<double>(state_.config.vegetation);
    vegetation.commit_policy = ui::UiCommitPolicy::Live; vegetation.minimum = 0.0; vegetation.maximum = 1.0; vegetation.step = 0.05;
    (void)model.set_field("vegetation", std::move(vegetation));
    ui::UiFieldState buildings{}; buildings.value = static_cast<double>(state_.config.buildings);
    buildings.commit_policy = ui::UiCommitPolicy::Live; buildings.minimum = 0.0; buildings.maximum = 1.0; buildings.step = 0.05;
    (void)model.set_field("buildings", std::move(buildings));
    ui::UiFieldState fenced{}; fenced.value = static_cast<double>(state_.config.fenced_parcels);
    fenced.commit_policy = ui::UiCommitPolicy::Live; fenced.minimum = 0.0; fenced.maximum = 1.0; fenced.step = 0.05;
    (void)model.set_field("fenced_parcels", std::move(fenced));
    (void)model.set("vegetation_percent", percentage(state_.config.vegetation));
    (void)model.set("buildings_percent", percentage(state_.config.buildings));
    (void)model.set("fenced_parcels_percent", percentage(state_.config.fenced_parcels));
    (void)model.set("selected", entryLabel(state_.selected, state_.config, state_.seed_input));
    (void)model.set("note", std::string{"The same seed and settings produce the same world plan."});
}

ui::UiActionResult WorldConfigScene::handle_ui_action(
    SceneContext& context, ui::UiActionId action, const ui::UiActionArguments& arguments) {
    const auto value_of = [&]() -> std::string_view {
        for (const auto& argument : arguments) if (argument.first == "value") return argument.second;
        return {};
    };
    const auto value = value_of();
    if (action == foundation::stable_id("world.seed")) {
        if (value == "auto") {
            state_.seed_input = WorldSeedInput::automatic();
            return ui::UiActionResult::Handled;
        }
        std::uint64_t seed{};
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), seed);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || seed == 0U) return ui::UiActionResult::Rejected;
        state_.config.seed = seed;
        state_.seed_input = WorldSeedInput::explicitValue(seed);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("world.map-size")) {
        std::uint64_t size{};
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), size);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) return ui::UiActionResult::Rejected;
        if (size != 400U && size != 600U && size != 800U && size != 1200U) return ui::UiActionResult::Rejected;
        state_.config.map_size_m = static_cast<std::uint32_t>(size);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("world.vegetation") ||
        action == foundation::stable_id("world.buildings") ||
        action == foundation::stable_id("world.fenced-parcels")) {
        double parsed = 0.0;
        const auto converted = std::from_chars(value.data(), value.data() + value.size(), parsed,
                                               std::chars_format::general);
        if (converted.ec != std::errc{} || converted.ptr != value.data() + value.size() || !std::isfinite(parsed)) return ui::UiActionResult::Rejected;
        const float clamped = std::clamp(static_cast<float>(parsed), 0.0F, 1.0F);
        if (action == foundation::stable_id("world.vegetation")) state_.config.vegetation = clamped;
        else if (action == foundation::stable_id("world.buildings")) state_.config.buildings = clamped;
        else state_.config.fenced_parcels = clamped;
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("scene.start-battlefield")) {
        const auto resolved_seed = state_.seed_input.resolve(autoSeedEntropy());
        if (!resolved_seed) return ui::UiActionResult::Rejected;
        state_.config.seed = resolved_seed.value();
        state_.seed_input = WorldSeedInput::explicitValue(resolved_seed.value());
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::StartScenario, state_.config);
        return ui::UiActionResult::Handled;
    }
    if (action == foundation::stable_id("scene.return-main-menu")) {
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::ReturnToMainMenu);
        return ui::UiActionResult::Handled;
    }
    return ui::UiActionResult::Unknown;
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
        if (state_.seed_input.mode == WorldSeedMode::Auto) {
            state_.config.seed = 1U;
            state_.seed_input = WorldSeedInput::explicitValue(1U);
        } else if (direction < 0 && state_.config.seed > 1U) {
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
        const auto resolved_seed = state_.seed_input.resolve(autoSeedEntropy());
        if (!resolved_seed) return;
        state_.config.seed = resolved_seed.value();
        state_.seed_input = WorldSeedInput::explicitValue(resolved_seed.value());
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::StartScenario, state_.config);
    } else if (state_.selected == WorldConfigEntry::Back) {
        application::enqueueApplicationCommand(
            context, application::ApplicationCommandKind::ReturnToMainMenu);
    }
}

} // namespace genomes::runtime
