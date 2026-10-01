#include "../Pr16Measurement.hpp"

#include <genomes/foundation/StableHash.hpp>
#include <genomes/infantry/AnimationSystem.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/RigBuilder.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] std::uint64_t semanticHash(
    const genomes::infantry::AnimationSnapshot& snapshot) noexcept {
    auto hash = genomes::foundation::stableHashU64(snapshot.simulation_tick);
    hash = genomes::foundation::stableHashCombine(hash, snapshot.pose_revision);
    hash = genomes::foundation::stableHashCombine(hash, snapshot.poses.size());
    for (const auto& pose : snapshot.poses) {
        hash = genomes::foundation::stableHashCombine(hash, pose.semantic_id);
        hash = genomes::foundation::stableHashCombine(hash, pose.revision);
        hash = genomes::foundation::stableHashCombine(hash, pose.evaluated ? 1U : 0U);
    }
    return hash;
}

[[nodiscard]] std::pair<double, std::uint64_t> run(
    const genomes::benchmark::Pr16MeasurementPlan& plan) {
    using namespace genomes::infantry;
    const auto genome = InfantryGenome::generate(plan.seed, 1.0F);
    if (!genome) {
        return {-1.0, 0U};
    }
    const auto phenotype = PhenotypeResolver::resolve(genome.value());
    if (!phenotype) {
        return {-1.0, 0U};
    }
    const auto rig = RigBuilder::build(phenotype.value().body, phenotype.value().face);
    if (!rig) {
        return {-1.0, 0U};
    }
    const auto locomotion = LocomotionController::create(phenotype.value().body);
    const auto face_seed = FaceAnimator::create(plan.seed, phenotype.value().face);
    if (!locomotion || !face_seed) {
        return {-1.0, 0U};
    }

    std::vector<LocomotionState> states(plan.entity_count);
    std::vector<FaceAnimator> faces;
    faces.reserve(plan.entity_count);
    for (std::size_t index = 0U; index < plan.entity_count; ++index) {
        faces.push_back(face_seed.value());
    }
    std::vector<AnimationEntity> entities;
    entities.reserve(plan.entity_count);
    for (std::size_t index = 0U; index < plan.entity_count; ++index) {
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
    const auto ticks = static_cast<std::uint64_t>(plan.duration_seconds * 60.0);
    const auto begin = std::chrono::steady_clock::now();
    for (std::uint64_t tick = 0U; tick < ticks; ++tick) {
        const auto result = system.evaluate(entities, tick, 1.0F / 60.0F, &jobs);
        if (!result) {
            return {-1.0, 0U};
        }
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - begin);
    return {static_cast<double>(elapsed.count()), semanticHash(system.currentSnapshot())};
}

} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path fixture = argc > 1
                                              ? argv[1]
                                              : std::filesystem::path{GENOMES_SOURCE_DIR} /
                                                    "config/performance/pr16_measurement_inputs.json";
    const auto loaded = genomes::benchmark::loadPr16MeasurementPlan(
        fixture, "infantry.animation");
    if (!loaded) {
        std::cerr << "status=INVALID_MEASUREMENT_FIXTURE error=" << loaded.error << '\n';
        return 2;
    }
    const auto& plan = *loaded.plan;
    for (std::size_t index = 0U; index < plan.warmup_runs; ++index) {
        if (run(plan).first < 0.0) {
            return 1;
        }
    }
    std::vector<double> samples;
    samples.reserve(plan.measured_runs);
    std::uint64_t semantic_hash = 0U;
    bool semantic_hash_stable = true;
    for (std::size_t index = 0U; index < plan.measured_runs; ++index) {
        const auto result = run(plan);
        if (result.first < 0.0) {
            return 1;
        }
        samples.push_back(result.first);
        if (index == 0U) {
            semantic_hash = result.second;
        } else {
            semantic_hash_stable = semantic_hash_stable && semantic_hash == result.second;
        }
    }
    const auto raw_samples = samples;
    const auto statistics = genomes::benchmark::sortedPr16Samples(std::move(samples));
    std::cout << "status=" << (plan.requiresBaseline() ? "BASELINE_REQUIRED" : plan.status)
              << " baseline_status=" << plan.baseline_status
              << " workload=" << plan.workload_id << " fixture=" << fixture.generic_string()
              << " entry_point=" << plan.entry_point << " seed=" << plan.seed
              << " entity_count=" << plan.entity_count
              << " duration_seconds=" << plan.duration_seconds
              << " warmup_runs=" << plan.warmup_runs
              << " warmups_discarded=true"
              << " measured_runs=" << plan.measured_runs
              << " required_metrics=" << genomes::benchmark::pr16MetricList(plan)
              << " median_us=" << statistics.percentile(0.50)
              << " p95_us=" << statistics.percentile(0.95)
              << " p99_us=" << statistics.percentile(0.99)
              << " max_us=" << statistics.maximum() << " semantic_hash=0x" << std::hex
              << semantic_hash << std::dec << " semantic_hash_scope=pose_summary"
              << " semantic_hash_stable=" << (semantic_hash_stable ? "true" : "false")
              << " allocations=NOT_INSTRUMENTED bytes=NOT_INSTRUMENTED samples_us=";
    for (std::size_t index = 0U; index < raw_samples.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        std::cout << raw_samples[index];
    }
    std::cout << '\n';
    return 0;
}
