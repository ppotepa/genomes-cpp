#pragma once

#include <genomes/game_scenes/WorldConfig.hpp>
#include <genomes/render/RenderTypes.hpp>
#include <genomes/runtime/Scene.hpp>
#include <genomes/world/WorldGenerationProfile.hpp>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace genomes::game_scenes {

using runtime::Scene;
using runtime::SceneContext;

// Namespace aliases preserve the historical scene API while the actual
// configuration and seed contract remain owned by genomes::application.
using WorldGenerationConfig = application::WorldGenerationConfig;
using WorldSeedMode = application::WorldSeedMode;
using WorldSeedInput = application::WorldSeedInput;

enum class WorldConfigEntry {
    Seed,
    MapSize,
    Preset,
    Hydrology,
    Vegetation,
    Buildings,
    FencedParcels,
    Start,
    Back,
    Count
};

struct WorldConfigState final {
    WorldConfigEntry selected{WorldConfigEntry::Start};
    application::WorldGenerationConfig config{};
    application::WorldSeedInput seed_input{};
    double preview_time{0.0};
};

class WorldConfigScene final : public Scene {
public:
    WorldConfigScene(
        std::shared_ptr<const world::FrozenWorldGenerationProfile> world_profile,
        application::WorldGenerationConfig config)
        : world_profile_{std::move(world_profile)}, initial_config_{std::move(config)} {
        if (world_profile_ == nullptr || !world_profile_->frozen() || !initial_config_.valid()) {
            throw std::invalid_argument{
                "world configuration scene requires a frozen profile and resolved request"};
        }
    }

    [[nodiscard]] foundation::SceneId id() const noexcept override;

    void on_enter(SceneContext&) override;
    void handle_input(SceneContext&, const input::InputFrame&) override;
    ui::UiActionResult handle_ui_action(
        SceneContext&, ui::UiActionId, const ui::UiActionArguments&) override;
    void fixed_update(SceneContext&, double) override;
    void frame_update(SceneContext&, double) override;
    void build_presentation(SceneContext&) override;

    [[nodiscard]] const WorldConfigState& state() const noexcept {
        return state_;
    }

private:
    void adjust(int direction) noexcept;
    void activate(SceneContext&);

    std::shared_ptr<const world::FrozenWorldGenerationProfile> world_profile_;
    application::WorldGenerationConfig initial_config_{};
    WorldConfigState state_{};
    std::vector<std::shared_ptr<const render::RenderMesh>> preview_prototypes_;
};

} // namespace genomes::game_scenes
