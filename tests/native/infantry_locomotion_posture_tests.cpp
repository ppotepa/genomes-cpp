#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/LocomotionController.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>

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

    const auto zero = locomotion.setRequested(state, {{0.0F}, {0.0F}});
    assert(zero && state.requested_crouch == 0.0F && state.requested_speed_mps == 0.0F);
    const auto nan = locomotion.setRequested(
        state, {{std::numeric_limits<float>::quiet_NaN()}, std::nullopt});
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

    assert(locomotion.setRequested(state, {{1.0F}, {100.0F}}));
    for (int index = 0; index < 120; ++index) {
        assert(locomotion.step(state, 1.0F / 60.0F));
    }
    assert(state.actual_crouch <= locomotion.limits().max_crouch + 1.0e-4F);
    assert(state.actual_speed_mps <= locomotion.limits().sprint_speed_mps);
    const auto posture = locomotion.posture(state);
    assert(posture.hip_height < 1.0F && posture.knee_bend > 0.0F);

    assert(locomotion.setFamily(state, LocomotionFamily::Prone));
    assert(locomotion.step(state, 1.0F / 60.0F));
    assert(state.target_speed_mps == 0.0F);
    assert(state.limit_reason == LocomotionLimitReason::FamilyDoesNotMove);
    return 0;
}
