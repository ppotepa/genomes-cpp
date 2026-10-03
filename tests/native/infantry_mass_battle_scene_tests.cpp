#include <genomes/game_scenes/BattlefieldScene.hpp>
#include <genomes/infantry/InfantryUnitController.hpp>
#include <genomes/render/PresentationSnapshot.hpp>
#include <genomes/ui/UiRuntime.hpp>

#include <cassert>
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
    assert(scene.handle_ui_action(context, foundation::stable_id("mass-battle.unknown"), {}) ==
           ui::UiActionResult::Unknown);
    return 0;
}
