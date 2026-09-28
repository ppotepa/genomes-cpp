#include <genomes/infantry/AppearanceArtifact.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/RigBuilder.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>

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
    constexpr std::uint32_t builds = 8U;
    const auto build_begin = std::chrono::steady_clock::now();
    std::size_t vertices = 0U;
    for (std::uint32_t index = 0U; index < builds; ++index) {
        options.seed = 19U + index;
        const auto artifact = genomes::infantry::AppearanceCompiler::build(
            phenotype.value(), rig.value(), options);
        if (!artifact) {
            return 1;
        }
        vertices += artifact.value().body.vertices.size();
    }
    const auto build_end = std::chrono::steady_clock::now();
    const auto cache_begin = std::chrono::steady_clock::now();
    options.seed = 19U;
    for (std::uint32_t index = 0U; index < builds; ++index) {
        if (!cache.acquire(phenotype.value(), rig.value(), options)) {
            return 1;
        }
    }
    const auto cache_end = std::chrono::steady_clock::now();
    const auto build_us = std::chrono::duration_cast<std::chrono::microseconds>(
        build_end - build_begin).count();
    const auto cache_us = std::chrono::duration_cast<std::chrono::microseconds>(
        cache_end - cache_begin).count();
    std::cout << "infantry_appearance builds=" << builds << " vertices=" << vertices
              << " build_us=" << build_us << " cache_us=" << cache_us
              << " cache_hits=" << cache.hits() << " cache_misses=" << cache.misses() << '\n';
    return 0;
}
