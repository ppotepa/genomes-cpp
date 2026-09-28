#include <genomes/infantry/AnimationSystem.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/RigBuilder.hpp>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace {

struct Fixture final {
    genomes::infantry::LocomotionController locomotion;
    genomes::infantry::LocomotionState locomotion_state{};
    genomes::infantry::FaceAnimator face;
    genomes::infantry::SkeletonData rig;

    static Fixture make(const genomes::infantry::PhenotypeArtifact& phenotype,
                        std::uint32_t seed) {
        const auto locomotion =
            genomes::infantry::LocomotionController::create(phenotype.body);
        const auto face = genomes::infantry::FaceAnimator::create(seed, phenotype.face);
        const auto rig = genomes::infantry::RigBuilder::build(phenotype.body, phenotype.face);
        assert(locomotion && face && rig);
        Fixture result{locomotion.value(), {}, face.value(), rig.value()};
        assert(result.locomotion.setPreset(result.locomotion_state,
                                           genomes::infantry::BipedPreset::Walk));
        return result;
    }
};

} // namespace

int main() {
    using namespace genomes::infantry;
    const auto genome = InfantryGenome::generate(0xC0FFEEU, 1.0F);
    assert(genome);
    const auto phenotype = PhenotypeResolver::resolve(genome.value());
    assert(phenotype);

    Fixture sequential_fixture = Fixture::make(phenotype.value(), 0x42U);
    Fixture parallel_fixture = Fixture::make(phenotype.value(), 0x42U);
    AnimationEntity sequential_entity{17U,
                                      &sequential_fixture.rig,
                                      &sequential_fixture.locomotion,
                                      &sequential_fixture.locomotion_state,
                                      &sequential_fixture.face,
                                      {0.0F, 0.0F, 0.0F},
                                      genomes::foundation::Vec3{1.0F, 0.2F, 4.0F}};
    AnimationEntity parallel_entity{17U,
                                    &parallel_fixture.rig,
                                    &parallel_fixture.locomotion,
                                    &parallel_fixture.locomotion_state,
                                    &parallel_fixture.face,
                                    {0.0F, 0.0F, 0.0F},
                                    genomes::foundation::Vec3{1.0F, 0.2F, 4.0F}};
    sequential_entity.lod.setTier(AnimationLOD::Mid);
    parallel_entity.lod.setTier(AnimationLOD::Mid);
    std::vector<AnimationEntity> sequential{sequential_entity};
    std::vector<AnimationEntity> parallel{parallel_entity};

    auto sequential_system_result = AnimationSystem::create(64U);
    auto parallel_system_result = AnimationSystem::create(1U);
    assert(sequential_system_result && parallel_system_result);
    AnimationSystem sequential_system = std::move(sequential_system_result.value());
    AnimationSystem parallel_system = std::move(parallel_system_result.value());
    genomes::jobs::JobSystem jobs(2U, 0U);

    for (std::uint64_t tick = 0U; tick < 12U; ++tick) {
        assert(sequential_system.evaluate(sequential, tick, 1.0F / 60.0F));
        assert(parallel_system.evaluate(parallel, tick, 1.0F / 60.0F, &jobs));
        const auto& left = sequential_system.currentSnapshot().poses.front();
        const auto& right = parallel_system.currentSnapshot().poses.front();
        assert(left.valid() && right.valid());
        assert(left.semantic_id == right.semantic_id);
        assert(std::abs(left.locomotion_phase - right.locomotion_phase) < 1.0e-6F);
        assert(std::abs(left.face.head_yaw - right.face.head_yaw) < 1.0e-6F);
        assert(std::abs(left.face.eyelids_close - right.face.eyelids_close) < 1.0e-6F);
    }
    assert(sequential[0].lod.evaluationCount() == 6U);
    assert(parallel[0].lod.evaluationCount() == sequential[0].lod.evaluationCount());
    assert(sequential_system.lastStats().chunk_count == 1U);
    assert(parallel_system.lastStats().chunk_count == 1U);

    const auto interpolated = AnimationSystem::interpolate(
        sequential_system.previousSnapshot().poses.front(),
        sequential_system.currentSnapshot().poses.front(), 0.5F);
    assert(interpolated.valid());

    sequential[0].lod.setTier(AnimationLOD::Near);
    assert(sequential_system.evaluate(sequential, 12U, 1.0F / 60.0F));
    assert(sequential[0].lod.evaluationCount() == 7U);
    return 0;
}
