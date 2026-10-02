#include <genomes/game_scenes/BattlefieldScene.hpp>
#include <genomes/render/PresentationSnapshot.hpp>
#include <genomes/ui/UiRuntime.hpp>

#include <cassert>

int main() {
    using namespace genomes;
    using game_scenes::BattlefieldScene;
    using game_scenes::BattlefieldSceneMode;
    using game_scenes::MassBattleAnimationArchetype;
    using game_scenes::MassBattlePresentationProfile;

    application::WorldGenerationConfig config{};
    BattlefieldScene scene(config, {}, {}, BattlefieldSceneMode::InfantryMassBattle);
    assert(scene.massBattleProfile() == MassBattlePresentationProfile::Balanced);

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
    assert(game_scenes::massBattleAnimationArchetype(9U) ==
           MassBattleAnimationArchetype::CrouchWalk);

    runtime::SceneCommandQueue commands;
    ui::UiRuntime ui;
    render::PresentationSnapshot presentation;
    runtime::SceneContext context{commands, ui, presentation};
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
    assert(scene.handle_ui_action(context, foundation::stable_id("mass-battle.unknown"), {}) ==
           ui::UiActionResult::Unknown);
    return 0;
}
