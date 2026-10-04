#include <genomes/game_scenes/BattlefieldScene.hpp>
#include <genomes/game_scenes/ApplicationCommand.hpp>
#include <genomes/infantry/InfantryUnitController.hpp>
#include <genomes/render/PresentationSnapshot.hpp>
#include <genomes/ui/UiRuntime.hpp>

#include <cassert>
#include <chrono>
#include <limits>
#include <variant>

int main() {
    using namespace genomes;
    using game_scenes::BattlefieldScene;
    using game_scenes::BattlefieldSceneMode;
    using game_scenes::MassBattleAnimationArchetype;
    using game_scenes::MassBattlePresentationProfile;

    application::WorldGenerationConfig config{};
    BattlefieldScene scene(config, {}, {}, BattlefieldSceneMode::InfantryMassBattle);
    assert(scene.massBattleProfile() == MassBattlePresentationProfile::Balanced);
    assert(scene.liveAnimationBudget() == 96U);
    assert(scene.massBattleUnitCount() == 2000U);
    BattlefieldScene configured(config, {}, {}, BattlefieldSceneMode::InfantryMassBattle, 17U);
    assert(configured.liveAnimationBudget() == 17U);
    BattlefieldScene oversized(config, {}, {}, BattlefieldSceneMode::InfantryMassBattle,
                               std::numeric_limits<std::size_t>::max());
    assert(oversized.liveAnimationBudget() == oversized.massBattleUnitCount());
    assert(scene.rtsControls().max_distance >= 6400.0F);
    assert(scene.rtsControls().min_distance <= 12.0F);
    camera::RtsCameraSettings custom_rts = scene.rtsControls();
    custom_rts.max_distance = 4800.0F;
    custom_rts.zoom_sensitivity = 0.20F;
    BattlefieldScene custom_camera(config, {}, {}, BattlefieldSceneMode::InfantryMassBattle,
                                   96U, custom_rts);
    assert(custom_camera.rtsControls().max_distance == 4800.0F);
    assert(custom_camera.rtsControls().zoom_sensitivity == 0.20F);

    assert(game_scenes::massBattleAnimationArchetype(0U) ==
           MassBattleAnimationArchetype::Idle);
    assert(game_scenes::massBattleAnimationArchetype(1U) ==
           MassBattleAnimationArchetype::Walk);
    assert(game_scenes::massBattleAnimationArchetype(2U) ==
           MassBattleAnimationArchetype::Run);
    assert(game_scenes::massBattleAnimationArchetype(3U) ==
           MassBattleAnimationArchetype::Crouch);
    assert(game_scenes::massBattleAnimationArchetype(4U) ==
           MassBattleAnimationArchetype::CrouchWalk);
    assert(game_scenes::massBattleAnimationArchetype(5U) ==
           MassBattleAnimationArchetype::Prone);
    assert(game_scenes::massBattleAnimationArchetype(6U) ==
           MassBattleAnimationArchetype::ProneMove);
    assert(game_scenes::massBattleAnimationArchetype(7U) ==
           MassBattleAnimationArchetype::WeaponReady);
    assert(game_scenes::massBattleAnimationArchetype(8U) ==
           MassBattleAnimationArchetype::Idle);
    assert(game_scenes::massBattleAnimationArchetype(9U) ==
           MassBattleAnimationArchetype::Walk);
    assert(infantry::infantryUnitActionId(6U) ==
           foundation::stable_id("infantry.action.prone-move"));
    assert(infantry::infantryUnitActionId(7U) ==
           foundation::stable_id("infantry.action.weapon-ready"));

    runtime::SceneCommandQueue commands;
    ui::UiRuntime ui;
    render::PresentationSnapshot presentation;
    runtime::SceneContext context{commands, ui, presentation};
    context.simulation_duration = std::chrono::microseconds{1500};
    context.presentation_duration = std::chrono::microseconds{2250};
    context.gpu_duration = std::chrono::microseconds{3750};
    context.rejected_stale_snapshots = 7U;
    const auto decrease = foundation::stable_id("mass-battle.animation-decrease");
    const auto increase = foundation::stable_id("mass-battle.animation-increase");
    const auto maximum = foundation::stable_id("mass-battle.animation-maximum");
    assert(scene.handle_ui_action(context, decrease, {}) == ui::UiActionResult::Handled);
    assert(scene.liveAnimationBudget() == 64U);
    assert(scene.handle_ui_action(context, increase, {}) == ui::UiActionResult::Handled);
    assert(scene.liveAnimationBudget() == 96U);
    assert(configured.handle_ui_action(context, decrease, {}) == ui::UiActionResult::Handled);
    assert(configured.liveAnimationBudget() == 0U);
    assert(configured.handle_ui_action(context, decrease, {}) == ui::UiActionResult::Handled);
    assert(configured.liveAnimationBudget() == 0U);
    assert(configured.handle_ui_action(context, increase, {}) == ui::UiActionResult::Handled);
    assert(configured.liveAnimationBudget() == 32U);
    configured.frame_update(context, 1.0 / 60.0);
    const auto* budget_field = ui.model().find("live_animation_budget");
    assert(budget_field != nullptr && std::get<std::int64_t>(*budget_field) == 32);
    const auto* limit_field = ui.model().find("live_animation_limit");
    assert(limit_field != nullptr && std::get<std::int64_t>(*limit_field) == 2000);
    assert(scene.handle_ui_action(context, maximum, {}) == ui::UiActionResult::Handled);
    assert(scene.liveAnimationBudget() == scene.massBattleUnitCount());
    assert(scene.handle_ui_action(context, increase, {}) == ui::UiActionResult::Handled);
    assert(scene.liveAnimationBudget() == scene.massBattleUnitCount());
    BattlefieldScene tactical(config, {});
    for (const auto action : {decrease, increase, maximum}) {
        assert(tactical.handle_ui_action(context, action, {}) == ui::UiActionResult::Unknown);
    }
    assert(scene.handle_ui_action(
               context, foundation::stable_id("mass-battle.profile-quality"), {}) ==
           ui::UiActionResult::Handled);
    assert(scene.massBattleProfile() == MassBattlePresentationProfile::Quality);
    assert(scene.handle_ui_action(
               context, foundation::stable_id("mass-battle.profile-balanced"), {}) ==
           ui::UiActionResult::Handled);
    assert(scene.massBattleProfile() == MassBattlePresentationProfile::Balanced);
    assert(scene.handle_ui_action(
               context, foundation::stable_id("mass-battle.profile-stress"), {}) ==
           ui::UiActionResult::Handled);
    assert(scene.massBattleProfile() == MassBattlePresentationProfile::Stress);
    assert(scene.liveAnimationBudget() == scene.massBattleUnitCount());

    assert(scene.handle_ui_action(
               context, foundation::stable_id("mass-battle.restart"), {}) ==
           ui::UiActionResult::Handled);
    assert(!commands.empty());
    auto restart_command = commands.pop();
    const auto* restart =
        dynamic_cast<const application::ApplicationCommand*>(restart_command.get());
    assert(restart != nullptr);
    assert(restart->kind == application::ApplicationCommandKind::OpenMassBattle);
    assert(restart->world_config.seed == config.seed);

    assert(scene.handle_ui_action(
               context, foundation::stable_id("mass-battle.generate-new"), {}) ==
           ui::UiActionResult::Handled);
    auto generate_command = commands.pop();
    const auto* generate =
        dynamic_cast<const application::ApplicationCommand*>(generate_command.get());
    assert(generate != nullptr);
    assert(generate->kind == application::ApplicationCommandKind::OpenMassBattle);
    assert(generate->world_config.seed != 0U);
    assert(generate->world_config.seed != config.seed);

    assert(scene.handle_ui_action(
               context, foundation::stable_id("mass-battle.next-terrain"), {}) ==
           ui::UiActionResult::Handled);
    auto terrain_command = commands.pop();
    const auto* terrain =
        dynamic_cast<const application::ApplicationCommand*>(terrain_command.get());
    assert(terrain != nullptr);
    assert(terrain->world_config.terrain.preset == world::TerrainPreset::Highlands);

    assert(scene.handle_ui_action(
               context, foundation::stable_id("mass-battle.next-terrain-detail"), {}) ==
           ui::UiActionResult::Handled);
    auto terrain_detail_command = commands.pop();
    const auto* terrain_detail =
        dynamic_cast<const application::ApplicationCommand*>(terrain_detail_command.get());
    assert(terrain_detail != nullptr);
    assert(terrain_detail->world_config.terrain.sample_spacing_m == 4U);

    assert(scene.handle_ui_action(
               context, foundation::stable_id("mass-battle.next-hydrology"), {}) ==
           ui::UiActionResult::Handled);
    auto hydrology_command = commands.pop();
    const auto* hydrology_change =
        dynamic_cast<const application::ApplicationCommand*>(hydrology_command.get());
    assert(hydrology_change != nullptr);
    assert(hydrology_change->world_config.hydrology_mode ==
           hydrology::HydrologyMode::SeededOptional);

    assert(scene.handle_ui_action(
               context, foundation::stable_id("mass-battle.frame-terrain"), {}) ==
           ui::UiActionResult::Handled);
    assert(scene.handle_ui_action(
               context, foundation::stable_id("mass-battle.diagnostics-terrain"), {}) ==
           ui::UiActionResult::Handled);
    scene.frame_update(context, 1.0 / 60.0);
    const auto* camera_frame = ui.model().find("camera_frame");
    assert(camera_frame != nullptr && std::get<std::string>(*camera_frame) == "Terrain");
    const auto* terrain_tab = ui.model().find("diag_tab_terrain");
    assert(terrain_tab != nullptr && std::get<bool>(*terrain_tab));
    const auto* terrain_detail_field = ui.model().find("terrain_detail");
    assert(terrain_detail_field != nullptr && std::get<std::string>(*terrain_detail_field) == "8 m");
    const auto* simulation_duration = ui.model().find("diag_sim_ms");
    const auto* presentation_duration = ui.model().find("diag_presentation_ms");
    const auto* gpu_duration = ui.model().find("diag_gpu_ms");
    const auto* stale_snapshots = ui.model().find("diag_stale_snapshots");
    assert(simulation_duration != nullptr &&
           std::get<std::string>(*simulation_duration) == "1.500000 ms");
    assert(presentation_duration != nullptr &&
           std::get<std::string>(*presentation_duration) == "2.250000 ms");
    assert(gpu_duration != nullptr && std::get<std::string>(*gpu_duration) == "3.750000 ms");
    assert(stale_snapshots != nullptr && std::get<std::int64_t>(*stale_snapshots) == 7);
    assert(scene.handle_ui_action(
               context, foundation::stable_id("mass-battle.diagnostics-toggle"), {}) ==
           ui::UiActionResult::Handled);
    scene.frame_update(context, 1.0 / 60.0);
    const auto* diagnostics_open = ui.model().find("diagnostics_panel_open");
    assert(diagnostics_open != nullptr && !std::get<bool>(*diagnostics_open));
    assert(scene.handle_ui_action(context, foundation::stable_id("mass-battle.unknown"), {}) ==
           ui::UiActionResult::Unknown);
    return 0;
}
