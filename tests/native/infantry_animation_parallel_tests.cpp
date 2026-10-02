#include <genomes/infantry/AnimationSystem.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/RigBuilder.hpp>

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <utility>

namespace {

using namespace genomes::infantry;

struct Fixture final {
    LocomotionController locomotion;
    LocomotionState locomotion_state{};
    AnimationTransitionRuntime transition_runtime{};
    FaceAnimator face;
    SkeletonData rig;

    static Fixture make(const PhenotypeArtifact& phenotype, std::uint32_t seed) {
        const auto locomotion = LocomotionController::create(phenotype.body);
        const auto face = FaceAnimator::create(seed, phenotype.face);
        const auto rig = RigBuilder::build(phenotype.body, phenotype.face);
        assert(locomotion && face && rig);
        Fixture result{locomotion.value(), {}, {}, face.value(), rig.value()};
        return result;
    }
};

AnimationEntity entityFor(Fixture& fixture, genomes::foundation::StableId id) {
    AnimationEntity result{};
    result.semantic_id = id;
    result.skeleton = &fixture.rig;
    result.locomotion = &fixture.locomotion;
    result.locomotion_state = &fixture.locomotion_state;
    result.transition_runtime = &fixture.transition_runtime;
    result.face = &fixture.face;
    result.root_position = {0.0F, 0.0F, 0.0F};
    return result;
}

void assertPoseEqual(const AnimationPose& left, const AnimationPose& right) {
    assert(left.valid() && right.valid());
    assert(left.semantic_id == right.semantic_id);
    assert(std::abs(left.locomotion_phase - right.locomotion_phase) < 1.0e-6F);
    assert(std::abs(left.transition_progress - right.transition_progress) < 1.0e-6F);
    assert(std::abs(left.transition_stage_progress - right.transition_stage_progress) < 1.0e-6F);
    for (std::size_t bone = 0U; bone < kRigBoneCount; ++bone) {
        const auto compare = [](const RigTransform& a, const RigTransform& b) {
            assert(std::abs(a.translation.x - b.translation.x) < 1.0e-6F);
            assert(std::abs(a.translation.y - b.translation.y) < 1.0e-6F);
            assert(std::abs(a.translation.z - b.translation.z) < 1.0e-6F);
            assert(std::abs(a.rotation.x - b.rotation.x) < 1.0e-6F);
            assert(std::abs(a.rotation.y - b.rotation.y) < 1.0e-6F);
            assert(std::abs(a.rotation.z - b.rotation.z) < 1.0e-6F);
            assert(std::abs(a.rotation.w - b.rotation.w) < 1.0e-6F);
        };
        compare(left.target_bones[bone], right.target_bones[bone]);
        compare(left.damped_bones[bone], right.damped_bones[bone]);
        compare(left.bones[bone], right.bones[bone]);
    }
}

} // namespace

int main() {
    const auto genome = InfantryGenome::generate(0xC0FFEEU, 1.0F);
    assert(genome);
    const auto phenotype = PhenotypeResolver::resolve(genome.value());
    assert(phenotype);

    Fixture sequential_a = Fixture::make(phenotype.value(), 0x42U);
    Fixture sequential_b = Fixture::make(phenotype.value(), 0x43U);
    Fixture parallel_a = Fixture::make(phenotype.value(), 0x42U);
    Fixture parallel_b = Fixture::make(phenotype.value(), 0x43U);

    std::array<AnimationEntity, 2U> sequential{
        entityFor(sequential_a, 17U), entityFor(sequential_b, 18U)};
    std::array<AnimationEntity, 2U> parallel{
        entityFor(parallel_a, 17U), entityFor(parallel_b, 18U)};
    sequential[0].lod.setTier(AnimationLOD::Near);
    sequential[1].lod.setTier(AnimationLOD::Far);
    parallel[0].lod.setTier(AnimationLOD::Near);
    parallel[1].lod.setTier(AnimationLOD::Far);

    auto sequential_system_result = AnimationSystem::create(64U);
    auto parallel_system_result = AnimationSystem::create(1U);
    assert(sequential_system_result && parallel_system_result);
    AnimationSystem sequential_system = std::move(sequential_system_result.value());
    AnimationSystem parallel_system = std::move(parallel_system_result.value());
    genomes::jobs::JobSystem jobs(2U, 0U);

    // Establish the initial snap before issuing a non-immediate request.
    assert(sequential_system.evaluate(sequential, 0U, 1.0F / 60.0F));
    assert(parallel_system.evaluate(parallel, 0U, 1.0F / 60.0F, &jobs));

    assert(sequential_a.locomotion.setState(
        sequential_a.locomotion_state, AnimationState::PRONE));
    assert(sequential_b.locomotion.setState(
        sequential_b.locomotion_state, AnimationState::PRONE));
    assert(parallel_a.locomotion.setState(
        parallel_a.locomotion_state, AnimationState::PRONE));
    assert(parallel_b.locomotion.setState(
        parallel_b.locomotion_state, AnimationState::PRONE));

    for (std::uint64_t tick = 1U; tick <= 12U; ++tick) {
        assert(sequential_system.evaluate(sequential, tick, 1.0F / 60.0F));
        assert(parallel_system.evaluate(parallel, tick, 1.0F / 60.0F, &jobs));
        for (std::size_t index = 0U; index < sequential.size(); ++index) {
            assertPoseEqual(sequential_system.currentSnapshot().poses[index],
                            parallel_system.currentSnapshot().poses[index]);
            assert(std::abs(sequential[index].transition_runtime->total_elapsed -
                            parallel[index].transition_runtime->total_elapsed) < 1.0e-6F);
        }
    }
    assert(sequential[0].lod.evaluationCount() == 13U);
    assert(sequential[1].lod.evaluationCount() == 4U);
    assert(parallel[0].lod.evaluationCount() == sequential[0].lod.evaluationCount());
    assert(parallel[1].lod.evaluationCount() == sequential[1].lod.evaluationCount());
    assert(sequential_system.lastStats().chunk_count == 1U);
    assert(parallel_system.lastStats().chunk_count == 2U);

    // Far LOD does not publish a snapshot on this tick, but the owned runtime
    // still advances. Waking the unit publishes the already advanced stage.
    const float far_elapsed = parallel[1].transition_runtime->total_elapsed;
    assert(parallel_system.evaluate(parallel, 13U, 1.0F / 60.0F, &jobs));
    assert(parallel[1].transition_runtime->total_elapsed > far_elapsed);
    assert(parallel[1].lod.evaluationCount() == 4U);
    parallel[1].lod.setTier(AnimationLOD::Near);
    assert(parallel_system.evaluate(parallel, 14U, 1.0F / 60.0F, &jobs));
    assert(parallel[1].lod.evaluationCount() == 5U);
    assert(parallel_system.currentSnapshot().poses[1].transition_progress ==
           parallel[1].locomotion_state->transition_progress);

    // A new request only changes the first unit; the second unit's runtime
    // and stage remain independent.
    const auto untouched_revision = parallel[1].transition_runtime->handled_request_revision;
    const float untouched_elapsed = parallel[1].transition_runtime->stage_elapsed;
    assert(parallel_a.locomotion.setState(
        parallel_a.locomotion_state, AnimationState::WALK));
    assert(parallel_system.evaluate(parallel, 15U, 1.0F / 60.0F, &jobs));
    assert(parallel[0].transition_runtime->active);
    assert(parallel[1].transition_runtime->handled_request_revision == untouched_revision);
    assert(parallel[1].transition_runtime->stage_elapsed > untouched_elapsed);
    assert(parallel[1].locomotion_state->requested_state == AnimationState::PRONE);
    assert(parallel[0].transition_runtime != parallel[1].transition_runtime);

    // Runtime ownership is exclusive even when callers provide otherwise
    // individually valid entities; accepting this would create a worker race.
    std::array<AnimationEntity, 2U> shared{parallel[0], parallel[1]};
    shared[1].transition_runtime = shared[0].transition_runtime;
    assert(!parallel_system.evaluate(shared, 16U, 1.0F / 60.0F, &jobs));

    const auto interpolated = AnimationSystem::interpolate(
        sequential_system.previousSnapshot().poses.front(),
        sequential_system.currentSnapshot().poses.front(), 0.5F);
    assert(interpolated.valid());
    return 0;
}
