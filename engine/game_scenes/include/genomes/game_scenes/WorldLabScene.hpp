#pragma once

#include <genomes/runtime/Scene.hpp>

#include <cstdint>
#include <string>

namespace genomes::runtime {

// The native Environment generator is not part of the current runtime. This
// scene intentionally exposes a session-only prototype so the eventual
// contract can be reviewed without pretending to mutate the simulated world.
class WorldLabScene final : public Scene {
public:
    [[nodiscard]] foundation::SceneId id() const noexcept override;
    void on_enter(SceneContext&) override;
    ui::UiActionResult handle_ui_action(
        SceneContext&, ui::UiActionId, const ui::UiActionArguments&) override;
    void frame_update(SceneContext&, double) override;

private:
    std::string category_{"Terrain"};
    std::string species_{"Oak"};
    std::string season_{"Summer"};
    std::string detail_{"World"};
    std::uint64_t seed_{0};
    double age_{50.0};
    double genome_{0.5};
    bool show_vegetation_{true};
    bool show_buildings_{true};
    std::string imported_path_{};
};

} // namespace genomes::runtime
