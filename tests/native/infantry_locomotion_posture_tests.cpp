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
