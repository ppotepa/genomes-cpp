#include <genomes/infantry/AppearanceArtifact.hpp>
#include <genomes/infantry/EquipmentCatalog.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/InfantryModelCompiler.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/RigBuilder.hpp>

#include <chrono>
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using Microseconds = std::chrono::microseconds;

[[nodiscard]] std::int64_t percentile(std::vector<std::int64_t> values, double fraction) {
    if (values.empty()) {
        return 0;
    }
    std::sort(values.begin(), values.end());
    const std::size_t index = static_cast<std::size_t>(
        fraction * static_cast<double>(values.size() - 1U));
    return values[index];
}

} // namespace

int main() {
    const auto genome = genomes::infantry::InfantryGenome::generate(0xCAFEU, 1.0F);
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
    options.seed = 19U;
    genomes::infantry::AppearanceCache cache;
    constexpr std::uint32_t builds = 100U;
    std::vector<std::int64_t> unique_samples;
    unique_samples.reserve(builds);
    std::size_t vertices = 0U;
    for (std::uint32_t index = 0U; index < builds; ++index) {
        options.seed = 19U + index;
        const auto begin = Clock::now();
        const auto artifact = genomes::infantry::AppearanceCompiler::build(
            phenotype.value(), rig.value(), options);
        if (!artifact) {
            return 1;
        }
        unique_samples.push_back(
            std::chrono::duration_cast<Microseconds>(Clock::now() - begin).count());
        vertices += artifact.value().body.vertices.size();
    }
    std::vector<std::int64_t> cache_samples;
    cache_samples.reserve(builds);
    for (std::uint32_t index = 0U; index < builds; ++index) {
        options.seed = 19U + index;
        const auto artifact = genomes::infantry::AppearanceCompiler::build(
            phenotype.value(), rig.value(), options);
        if (!artifact) {
            return 1;
        }
        if (!cache.acquire(phenotype.value(), rig.value(), options)) {
            return 1;
        }
    }
    for (std::uint32_t index = 0U; index < builds; ++index) {
        options.seed = 19U + index;
        const auto begin = Clock::now();
        if (!cache.acquire(phenotype.value(), rig.value(), options)) {
            return 1;
        }
        cache_samples.push_back(
            std::chrono::duration_cast<Microseconds>(Clock::now() - begin).count());
    }
    std::array<std::int64_t, 7U> style_samples{};
    for (std::size_t style = 0U; style < style_samples.size(); ++style) {
        options.seed = 101U + static_cast<std::uint64_t>(style);
        options.hair_style = static_cast<genomes::infantry::HairStyle>(style);
        const auto begin = Clock::now();
        if (!genomes::infantry::AppearanceCompiler::build(
                phenotype.value(), rig.value(), options)) {
            return 1;
        }
        style_samples[style] =
            std::chrono::duration_cast<Microseconds>(Clock::now() - begin).count();
    }
    genomes::infantry::InfantryModelCompiler model_compiler;
    std::vector<std::int64_t> equipment_samples;
    const auto loadouts = genomes::infantry::infantryLoadouts();
    equipment_samples.reserve(loadouts.size());
    for (const auto& loadout : loadouts) {
        genomes::infantry::InfantryModelRequest request{};
        request.seed = 0x5EED2026U;
        request.loadout_id = loadout.id;
        const auto begin = Clock::now();
        if (!model_compiler.compile(request)) {
            return 1;
        }
        equipment_samples.push_back(
            std::chrono::duration_cast<Microseconds>(Clock::now() - begin).count());
    }
    std::cout << "infantry_appearance builds=" << builds << " vertices=" << vertices
              << " unique_avg_us=" << (unique_samples.empty() ? 0 :
                    std::accumulate(unique_samples.begin(), unique_samples.end(), std::int64_t{0}) /
                    static_cast<std::int64_t>(unique_samples.size()))
              << " unique_p50_us=" << percentile(unique_samples, 0.50)
              << " unique_p95_us=" << percentile(unique_samples, 0.95)
              << " cache_p50_us=" << percentile(cache_samples, 0.50)
              << " cache_p95_us=" << percentile(cache_samples, 0.95)
              << " styles_p95_us=" << percentile(
                    std::vector<std::int64_t>(style_samples.begin(), style_samples.end()), 0.95)
              << " equipment_p95_us=" << percentile(equipment_samples, 0.95)
              << " equipment_loadouts=" << loadouts.size()
              << " cache_hits=" << cache.hits() << " cache_misses=" << cache.misses() << '\n';
    return 0;
}
