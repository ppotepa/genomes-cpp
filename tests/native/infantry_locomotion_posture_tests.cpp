#include <genomes/infantry/AnimationSystem.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/LocomotionController.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/RigBuilder.hpp>

#include <array>
#include <cassert>
#include <cmath>
#include <limits>

int main() {
    using namespace genomes::infantry;
    const auto genome = InfantryGenome::generate(77U, 1.0F);
    assert(genome);
    const auto phenotype = PhenotypeResolver::resolve(genome.value());
    assert(phenotype);
    const auto controller = LocomotionController::create(phenotype.value().body);
    assert(controller && controller.value().limits().valid());
    LocomotionController locomotion = controller.value();
    LocomotionState state = locomotion.initialState();
    assert(state.valid());

    const auto& body = phenotype.value().body;
    assert(std::abs(PostureProfile::curve(0.0F, 1) - .985F) < 1.0e-7F);
    assert(std::abs(PostureProfile::curve(.5F, 3) - .385F) < 1.0e-7F);
    assert(std::abs(PostureProfile::curve(1.0F, 6) - .028F) < 1.0e-7F);
    assert(PostureProfile::depthAtSpeed(0.0F, body) == 1.0F);
    assert(PostureProfile::depthAtSpeed(body.run_speed, body) == 0.0F);
    const auto gait = PostureProfile::gait(0.0F, body.walk_speed, body);
    assert(gait.cycle_m > 0.0F && gait.duty > 0.0F && gait.duty < 1.0F);
    assert(std::abs(gait.cadence - body.walk_speed / gait.cycle_m) < 1.0e-6F);
    const auto contact = PostureProfile::sampleLowContact(0.0F, gait.duty,
                                                          gait.cycle_m, gait.lift_m);
    assert(contact.stance && contact.lift == 0.0F);
    assert(std::abs(contact.z - gait.cycle_m * gait.duty * .5F) < 1.0e-6F);
    const auto swing = PostureProfile::sampleLowContact(
        gait.duty + (1.0F-gait.duty)*.5F, gait.duty, gait.cycle_m, gait.lift_m);
    assert(!swing.stance && swing.lift > 0.0F && swing.plant == 0.0F);

    const auto zero = locomotion.setRequested(state, {{0.0F}, {0.0F}, std::nullopt});
    assert(zero && state.requested_crouch == 0.0F && state.requested_speed_mps == 0.0F);
    const auto nan = locomotion.setRequested(
        state, {{std::numeric_limits<float>::quiet_NaN()}, std::nullopt, std::nullopt});
    assert(!nan);
    assert(locomotion.setPreset(state, BipedPreset::Run));
    const auto before_phase = state.phase;
    for (int index = 0; index < 30; ++index) {
        assert(locomotion.step(state, 1.0F / 60.0F));
    }
    assert(state.actual_speed_mps > 0.0F);
    assert(state.phase != before_phase);
    const float phase_before_switch = state.phase;
    assert(locomotion.setPreset(state, BipedPreset::Walk));
    assert(std::abs(state.phase - phase_before_switch) < 1.0e-6F);
    assert(locomotion.step(state, 1.0F / 60.0F));
    assert(state.phase >= 0.0F && state.phase < 1.0F);

    for (const double initial_phase : {0.12, 0.45, 0.88}) {
        LocomotionState stopped = locomotion.initialState();
        stopped.phase = initial_phase;
        assert(locomotion.setState(stopped, AnimationState::IDLE, true));
        bool saw_settling = false;
        for (int index = 0; index < 30; ++index) {
            assert(locomotion.step(stopped, 1.0F / 60.0F));
            saw_settling = saw_settling || stopped.settling;
        }
        assert(saw_settling);
        const double settled_phase = stopped.phase;
        assert((settled_phase > 0.49 && settled_phase < 0.54) || settled_phase < 0.04);
        assert(locomotion.step(stopped, 1.0F / 60.0F));
        assert(std::abs(stopped.phase - settled_phase) < 1.0e-6);
    }

    assert(locomotion.setRequested(state, {{1.0F}, {100.0F}, std::nullopt}));
    for (int index = 0; index < 120; ++index) {
        assert(locomotion.step(state, 1.0F / 60.0F));
    }
    assert(state.actual_crouch <= locomotion.limits().max_crouch + 1.0e-4F);
    assert(state.actual_speed_mps <= locomotion.limits().sprint_speed_mps);
    const auto posture = locomotion.posture(state);
    assert(posture.hip_height < 1.0F && posture.knee_bend > 0.0F);

    // Direction and angular velocity are presentation inputs as well as
    // simulation telemetry: they must steer the lower body and anticipate a
    // turn without changing the authored gait phase.
    const auto rig_result = RigBuilder::build(phenotype.value().body, phenotype.value().face);
    assert(rig_result);
    auto rig = rig_result.value();
    AnimationTransitionRuntime transition_runtime{};
    AnimationEntity entity{};
    entity.semantic_id = 77U;
    entity.skeleton = &rig;
    entity.locomotion = &locomotion;
    entity.locomotion_state = &state;
    entity.transition_runtime = &transition_runtime;
    entity.lod.setTier(AnimationLOD::Near);
    auto animation_result = AnimationSystem::create(1U);
    assert(animation_result);
    auto animation = animation_result.value();
    std::array<AnimationEntity, 1U> entities{entity};
    constexpr float fixed_dt = 1.0F / 60.0F;
    const double phase_before_pose = state.phase;
    state.move_angle = 0.0F;
    state.turn_rate = 0.0F;
    assert(animation.evaluate(entities, 0U, fixed_dt));
    const auto baseline = animation.currentSnapshot().poses.front();
    state.move_angle = 1.5707963267948966F;
    assert(animation.evaluate(entities, 1U, fixed_dt));
    const auto strafe = animation.currentSnapshot().poses.front();
    assert(std::abs(strafe.foot_targets[0].x - baseline.foot_targets[0].x) > 1.0e-4F ||
           std::abs(strafe.foot_targets[0].z - baseline.foot_targets[0].z) > 1.0e-4F);
    assert(std::abs(strafe.target_bones[boneIndex(BoneId::Hips)].rotation.y -
                    baseline.target_bones[boneIndex(BoneId::Hips)].rotation.y) > 1.0e-4F);
    state.move_angle = 0.0F;
    state.turn_rate = 1.0F;
    assert(animation.evaluate(entities, 2U, fixed_dt));
    const auto turn = animation.currentSnapshot().poses.front();
    assert(std::abs(turn.target_bones[boneIndex(BoneId::Hips)].rotation.y -
                    baseline.target_bones[boneIndex(BoneId::Hips)].rotation.y) > 1.0e-4F);
    assert(std::abs(turn.foot_yaw[0] - baseline.foot_yaw[0]) > 1.0e-4F);
    assert(state.phase == phase_before_pose);

    // A seated preview may be anchored to a moving world-space seat. The
    // anchor shifts hips and both leg targets, and re-entry follows the new
    // point without leaking the previous seated offset.
    assert(locomotion.setState(state, AnimationState::SITTING, true));
    entities[0].seat_anchor.reset();
    assert(animation.evaluate(entities, 3U, fixed_dt));
    const auto free_seat = animation.currentSnapshot().poses.front();
    entities[0].seat_anchor = genomes::foundation::Vec3{0.0F, 1.0F, 1.5F};
    assert(animation.evaluate(entities, 4U, fixed_dt));
    const auto anchored_seat = animation.currentSnapshot().poses.front();
    assert(std::abs(anchored_seat.bones[boneIndex(BoneId::Hips)].translation.z -
                    free_seat.bones[boneIndex(BoneId::Hips)].translation.z) > 1.0e-3F);
    assert(std::abs(anchored_seat.foot_targets[0].z - free_seat.foot_targets[0].z) >
           1.0e-3F);
    entities[0].seat_anchor = genomes::foundation::Vec3{0.0F, 1.0F, 2.0F};
    assert(animation.evaluate(entities, 5U, fixed_dt));
    const auto moved_seat = animation.currentSnapshot().poses.front();
    assert(std::abs(moved_seat.bones[boneIndex(BoneId::Hips)].translation.z -
                    anchored_seat.bones[boneIndex(BoneId::Hips)].translation.z) > 1.0e-3F);
    entities[0].seat_anchor.reset();

    assert(locomotion.setFamily(state, LocomotionFamily::Prone));
    assert(locomotion.step(state, 1.0F / 60.0F));
    assert(state.target_speed_mps == 0.0F);
    assert(state.limit_reason == LocomotionLimitReason::FamilyDoesNotMove);
    return 0;
}
