#include <genomes/infantry/AnimationSystem.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/RigBuilder.hpp>
#include <genomes/weapons/WeaponHandlingSystem.hpp>

#include <cassert>
#include <cmath>
#include <optional>
#include <span>

int main() {
    using namespace genomes;
    using namespace genomes::infantry;
    const auto genome = InfantryGenome::generate(0xBADC0DEU, 1.0F);
    assert(genome);
    const auto phenotype = PhenotypeResolver::resolve(genome.value());
    assert(phenotype);
    const auto locomotion_result = LocomotionController::create(phenotype.value().body);
    const auto face_result = FaceAnimator::create(0x1234U, phenotype.value().face);
    const auto rig_result = RigBuilder::build(phenotype.value().body, phenotype.value().face);
    assert(locomotion_result && face_result && rig_result);
    auto locomotion = locomotion_result.value();
    auto face = face_result.value();
    const auto rig = rig_result.value();
    auto state = locomotion.initialState();
    assert(locomotion.setState(state, AnimationState::IDLE, true));

    AnimationWeaponOverlay overlay{};
    overlay.weapon_id = 17U;
    overlay.primary = {AnimationHandOwner::Primary, {0.28F, 0.95F, 0.42F},
                       1.0F, 1.0F, true};
    overlay.support = {};
    overlay.aim_direction = {0.2F, 0.1F, 0.97F};
    overlay.readiness = 0.8F;
    overlay.recoil = 0.0F;
    assert(overlay.valid());

    AnimationTransitionRuntime transition_runtime{};
    AnimationEntity entity{31U, &rig, &locomotion, &state, &transition_runtime, &face,
                           {0.0F, 0.0F, 0.0F}, std::nullopt, std::nullopt, {},
                           nullptr, nullptr, &overlay};
    entity.lod.setTier(AnimationLOD::Near);
    auto animation_result = AnimationSystem::create(1U);
    assert(animation_result);
    auto animation = animation_result.value();
    assert(animation.evaluate(std::span<AnimationEntity>(&entity, 1U), 0U,
                              1.0F / 60.0F));
    const auto first = animation.currentSnapshot().poses.front();
    assert(first.valid());
    assert(first.hand_owners[0] == AnimationHandOwner::Free);
    assert(first.hand_owners[1] == AnimationHandOwner::Primary);
    assert(std::abs(first.weapon_readiness - 0.8F) < 1.0e-6F);
    assert(std::abs(first.weapon_aim_direction.x - 0.2F) < 1.0e-6F);

    overlay.support = {AnimationHandOwner::Support, {-0.28F, 0.93F, 0.40F},
                       1.0F, 1.0F, true};
    overlay.readiness = 1.0F;
    overlay.recoil = 0.75F;
    overlay.aim_direction = {1.0F, 0.0F, 0.0F};
    assert(animation.evaluate(std::span<AnimationEntity>(&entity, 1U), 1U,
                              1.0F / 60.0F));
    const auto second = animation.currentSnapshot().poses.front();
    assert(second.valid());
    assert(second.hand_owners[0] == AnimationHandOwner::Support);
    assert(second.hand_owners[1] == AnimationHandOwner::Primary);
    assert(std::abs(second.weapon_readiness - 1.0F) < 1.0e-6F);
    assert(std::abs(second.weapon_recoil - 0.75F) < 1.0e-6F);
    assert(std::abs(second.bones[boneIndex(BoneId::Chest)].rotation.y -
                    first.bones[boneIndex(BoneId::Chest)].rotation.y) > 1.0e-4F ||
           std::abs(second.bones[boneIndex(BoneId::SpineUpper)].rotation.y -
                    first.bones[boneIndex(BoneId::SpineUpper)].rotation.y) > 1.0e-4F);

    // Aim is a bounded body contribution as well as a diagnostic pose field;
    // an elevated cardinal direction must reach the spine/chest overlay.
    overlay.aim_direction = {0.0F, 0.75F, 0.25F};
    overlay.recoil = 0.0F;
    assert(animation.evaluate(std::span<AnimationEntity>(&entity, 1U), 2U,
                              1.0F / 60.0F));
    const auto elevated = animation.currentSnapshot().poses.front();
    assert(std::abs(elevated.bones[boneIndex(BoneId::Chest)].rotation.x -
                    second.bones[boneIndex(BoneId::Chest)].rotation.x) > 1.0e-4F ||
           std::abs(elevated.bones[boneIndex(BoneId::SpineUpper)].rotation.x -
                    second.bones[boneIndex(BoneId::SpineUpper)].rotation.x) > 1.0e-4F);

    // Body transitions do not become a second transport for either overlay.
    // The weapon values remain current and face output is read directly from
    // the face animator while the body is moving toward SITTING.
    assert(locomotion.setState(state, AnimationState::SITTING));
    assert(animation.evaluate(std::span<AnimationEntity>(&entity, 1U), 3U,
                              1.0F / 60.0F));
    const auto during_body_transition = animation.currentSnapshot().poses.front();
    assert(during_body_transition.transition_stage != AnimationTransitionStage::None);
    assert(std::abs(during_body_transition.weapon_readiness - overlay.readiness) < 1.0e-6F);
    assert(std::abs(during_body_transition.weapon_recoil - overlay.recoil) < 1.0e-6F);
    const auto face_output = face.output();
    assert(std::abs(during_body_transition.face.head_yaw - face_output.head_yaw) < 1.0e-6F);
    assert(std::abs(during_body_transition.face.head_pitch - face_output.head_pitch) < 1.0e-6F);

    // A one-handed holster releases both persistent ownership slots. This
    // also covers the snapshot reuse path used by LOD evaluation.
    overlay.primary = {};
    overlay.support = {};
    overlay.readiness = 0.0F;
    overlay.recoil = 0.0F;
    assert(animation.evaluate(std::span<AnimationEntity>(&entity, 1U), 4U,
                              1.0F / 60.0F));
    const auto holstered = animation.currentSnapshot().poses.front();
    assert(holstered.valid());
    assert(holstered.hand_owners[0] == AnimationHandOwner::Free);
    assert(holstered.hand_owners[1] == AnimationHandOwner::Free);
    assert(holstered.weapon_readiness == 0.0F);
    assert(holstered.weapon_recoil == 0.0F);

    // The weapon bridge emits one authoritative intent for one request, and
    // the same output pose carries the recoil generated by that shot.
    const auto* carbine = weapons::WeaponCatalog::find("carbine");
    assert(carbine != nullptr);
    const auto weapon_artifact = weapons::WeaponGeometryGenerator::build(*carbine);
    assert(weapon_artifact);
    weapons::WeaponRuntimeState weapon_state{};
    weapon_state.selected_weapon = carbine->id;
    weapon_state.active_weapon = carbine->id;
    weapon_state.handling = weapons::WeaponHandlingState::Held;
    weapon_state.readiness = 1.0F;
    weapon_state.requested_readiness = 1.0F;
    const weapons::WeaponHandlingInput handling_input{
        77U, carbine, &weapon_artifact.value(), {0.0F, 1.0F, 0.0F},
        foundation::Vec3{0.0F, 1.0F, 5.0F}, {}, true};
    weapons::WeaponHandlingSystem weapon_handling;
    weapons::WeaponStepOutput weapon_output{};
    assert(weapon_handling.step(weapon_state, handling_input, {10U}, 1.0F / 60.0F,
                                weapon_output));
    assert(weapon_output.fire.has_value());
    assert(weapon_output.fire->entity == 77U);
    assert(weapon_output.fire->shot_sequence == 1U);
    assert(weapon_output.pose.recoil > 0.0F);
    weapons::WeaponStepOutput duplicate_output{};
    assert(weapon_handling.step(weapon_state, handling_input, {10U}, 1.0F / 60.0F,
                                duplicate_output));
    assert(!duplicate_output.fire.has_value());
    return 0;
}
