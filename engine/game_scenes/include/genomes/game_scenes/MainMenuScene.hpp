#pragma once

#include <genomes/game_scenes/WorldConfig.hpp>
#include <genomes/render/RenderTypes.hpp>
#include <genomes/runtime/Scene.hpp>
#include <genomes/world/WorldGenerationProfile.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace genomes::game_scenes {

using runtime::Scene;
using runtime::SceneContext;

enum class MainMenuEntry {
    StartScenario,
    UnitLab,
    BuildingLab,
    WorldLab,
    Settings,
    Quit,
    Count
};

struct MainMenuState {
    MainMenuEntry selected{MainMenuEntry::StartScenario};
    std::uint64_t preview_seed{0U};
    double preview_time{0.0};
    bool settings_open{false};
};

class MainMenuScene final : public Scene {
public:
    explicit MainMenuScene(
        std::shared_ptr<const world::FrozenWorldGenerationProfile> world_profile)
        : world_profile_{std::move(world_profile)},
          world_config_{requireWorldProfile(world_profile_)} {
        state_.preview_seed = world_config_.seed;
    }

    [[nodiscard]] foundation::SceneId id() const noexcept override;

    void on_enter(SceneContext&) override;
    void handle_input(SceneContext&, const input::InputFrame&) override;
    void fixed_update(SceneContext&, double) override;
    void frame_update(SceneContext&, double) override;
    void build_presentation(SceneContext&) override;

    void select(MainMenuEntry entry) noexcept;
    void activate(SceneContext&);

    [[nodiscard]] const MainMenuState& state() const noexcept {
        return state_;
    }

private:
    [[nodiscard]] static application::WorldGenerationConfig requireWorldProfile(
        const std::shared_ptr<const world::FrozenWorldGenerationProfile>& profile) {
        if (profile == nullptr || !profile->frozen()) {
            throw std::invalid_argument{"main menu requires a frozen world profile"};
        }
        return profile->makeDefaultRequest();
    }

    std::shared_ptr<const world::FrozenWorldGenerationProfile> world_profile_;
    application::WorldGenerationConfig world_config_{};
    MainMenuState state_{};
    std::vector<std::shared_ptr<const render::RenderMesh>> preview_prototypes_;
};

} // namespace genomes::game_scenes
