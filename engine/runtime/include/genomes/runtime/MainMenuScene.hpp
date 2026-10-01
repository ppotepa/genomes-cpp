#pragma once

#include <genomes/render/RenderTypes.hpp>
#include <genomes/runtime/Scene.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace genomes::runtime {

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
    std::uint64_t preview_seed{0x5EED2026ull};
    double preview_time{0.0};
    bool settings_open{false};
};

class MainMenuScene final : public Scene {
public:
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
    MainMenuState state_{};
    std::vector<std::shared_ptr<const render::RenderMesh>> preview_prototypes_;
};

} // namespace genomes::runtime
