#include "../Pr16Measurement.hpp"

#include <genomes/foundation/StableHash.hpp>
#include <genomes/infantry/AppearanceArtifact.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/RigBuilder.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <utility>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using Microseconds = std::chrono::microseconds;

[[nodiscard]] std::uint64_t semanticHash(
    const genomes::infantry::AppearanceArtifact& artifact) noexcept {
    auto hash = genomes::foundation::stableHashU64(artifact.version);
    hash = genomes::foundation::stableHashCombine(hash, artifact.cache_key);
    hash = genomes::foundation::stableHashCombine(hash, artifact.body.vertices.size());
    hash = genomes::foundation::stableHashCombine(hash, artifact.body.indices.size());
    hash = genomes::foundation::stableHashCombine(hash, artifact.hair.vertices.size());
    hash = genomes::foundation::stableHashCombine(hash, artifact.hair.indices.size());
    for (const auto& morph : artifact.morphs) {
        hash = genomes::foundation::stableHashCombine(hash, morph.position_deltas.size());
        hash = genomes::foundation::stableHashCombine(hash, morph.normal_deltas.size());
    }
    return hash;
}

} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path fixture = argc > 1
                                              ? argv[1]
                                              : std::filesystem::path{GENOMES_SOURCE_DIR} /
                                                    "config/performance/pr16_measurement_inputs.json";
    const auto loaded = genomes::benchmark::loadPr16MeasurementPlan(
        fixture, "infantry.appearance_generation");
    if (!loaded) {
        std::cerr << "status=INVALID_MEASUREMENT_FIXTURE error=" << loaded.error << '\n';
        return 2;
    }
    const auto& plan = *loaded.plan;
    const auto genome = genomes::infantry::InfantryGenome::generate(
        static_cast<std::uint32_t>(plan.seed), 1.0F);
    if (!genome) {
        return 1;
    }
    const auto phenotype = genomes::infantry::PhenotypeResolver::resolve(genome.value());
    if (!phenotype) {
        return 1;
    }
    const auto rig = genomes::infantry::RigBuilder::build(phenotype.value().body,
                                                           phenotype.value().face);
    if (!rig) {
        return 1;
    }
    genomes::infantry::AppearanceOptions options{};
    options.detail_level = plan.detail == "far" ? 0U : (plan.detail == "world" ? 1U : 2U);
    options.hair_style = genomes::infantry::HairStyle::Buzz;
    const auto measure = [&]() -> std::pair<double, std::uint64_t> {
        const auto begin = Clock::now();
        std::uint64_t result_hash = 0U;
        for (std::size_t index = 0U; index < plan.entity_count; ++index) {
            options.seed = plan.seed + static_cast<std::uint64_t>(index);
            const auto artifact = genomes::infantry::AppearanceCompiler::build(
                phenotype.value(), rig.value(), options);
            if (!artifact) {
                return {-1.0, 0U};
            }
            result_hash = genomes::foundation::stableHashCombine(
                result_hash, semanticHash(artifact.value()));
        }
        return {Microseconds(Clock::now() - begin).count(), result_hash};
    };
    for (std::size_t index = 0U; index < plan.warmup_runs; ++index) {
        if (measure().first < 0.0) {
            return 1;
        }
    }
    std::vector<double> samples;
    samples.reserve(plan.measured_runs);
    std::uint64_t semantic_hash = 0U;
    bool semantic_hash_stable = true;
    for (std::size_t index = 0U; index < plan.measured_runs; ++index) {
        const auto result = measure();
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
              << semantic_hash << std::dec << " semantic_hash_scope=artifact_summary"
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
