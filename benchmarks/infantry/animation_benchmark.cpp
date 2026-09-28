#include <genomes/infantry/AnimationSystem.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/RigBuilder.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

void run(std::size_t count) {
    using namespace genomes::infantry;
    const auto genome = InfantryGenome::generate(0xBEEFU, 1.0F);
    const auto phenotype = PhenotypeResolver::resolve(genome.value());
    const auto rig = RigBuilder::build(phenotype.value().body, phenotype.value().face);
    const auto locomotion = LocomotionController::create(phenotype.value().body);
    const auto face_seed = FaceAnimator::create(0x1234U, phenotype.value().face);

    std::vector<LocomotionState> states(count);
    std::vector<FaceAnimator> faces;
    faces.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        faces.push_back(face_seed.value());
    }
    std::vector<AnimationEntity> entities;
    entities.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        (void)locomotion.value().setPreset(states[index], BipedPreset::Walk);
        entities.push_back({static_cast<genomes::foundation::StableId>(index + 1U),
                            &rig.value(),
                            &locomotion.value(),
                            &states[index],
                            &faces[index],
                            {static_cast<float>(index % 100U) * 0.1F, 0.0F, 0.0F},
                            std::nullopt,
                            {}});
        if ((index % 4U) == 1U) {
            entities.back().lod.setTier(AnimationLOD::Mid);
        } else if ((index % 4U) == 2U) {
            entities.back().lod.setTier(AnimationLOD::Far);
        } else if ((index % 4U) == 3U) {
            entities.back().lod.setTier(AnimationLOD::Offscreen);
        }
    }

    auto system_result = AnimationSystem::create(128U);
    AnimationSystem system = std::move(system_result.value());
    genomes::jobs::JobSystem jobs(4U, 0U);
    constexpr std::uint64_t ticks = 60U;
    const auto begin = std::chrono::steady_clock::now();
    for (std::uint64_t tick = 0U; tick < ticks; ++tick) {
        (void)system.evaluate(entities, tick, 1.0F / 60.0F, &jobs);
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - begin);
    std::cout << "infantry_animation count=" << count
              << " avg_us=" << (static_cast<double>(elapsed.count()) / ticks)
              << " evaluations=" << system.lastStats().evaluated_count << '\n';
}

} // namespace

int main() {
    run(100U);
    run(1000U);
    run(10000U);
    return 0;
}
