#pragma once

#include <genomes/buildings/BuildingModel.hpp>
#include <genomes/buildings/BuildingProfile.hpp>
#include <genomes/render/RenderTypes.hpp>
#include <genomes/runtime/Scene.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

namespace genomes::runtime {

class BuildingLabScene final : public Scene {
public:
    explicit BuildingLabScene(
        std::shared_ptr<const buildings::FrozenBuildingProfile> building_profile) noexcept
        : building_profile_{std::move(building_profile)} {}

    [[nodiscard]] foundation::SceneId id() const noexcept override;

    void on_enter(SceneContext&) override;
    void on_exit(SceneContext&) override;
    void handle_input(SceneContext&, const input::InputFrame&) override;
    ui::UiActionResult handle_ui_action(
        SceneContext&, ui::UiActionId, const ui::UiActionArguments&) override;
    void fixed_update(SceneContext&, double) override;
    void frame_update(SceneContext&, double) override;
    void build_presentation(SceneContext&) override;

private:
    void rebuild_mesh();
    void apply_selected_damage(float normalized_damage);

    std::shared_ptr<const buildings::FrozenBuildingProfile> building_profile_;
    buildings::BuildingPlan plan_{};
    std::unique_ptr<buildings::BuildingRuntime> runtime_;
    std::shared_ptr<render::RenderMesh> render_mesh_;
    std::size_t selected_part_{0};
    std::uint64_t seed_{0xB01D1A9u};
    float damage_amount_{0.20F};
    double elapsed_seconds_{0.0};
    std::string error_;
};

} // namespace genomes::runtime
