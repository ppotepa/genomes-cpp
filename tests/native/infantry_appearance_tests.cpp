#include <genomes/infantry/AppearanceArtifact.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/RigBuilder.hpp>

#include <cassert>
#include <cmath>

int main() {
    const auto genome = genomes::infantry::InfantryGenome::generate(0xBEEFU, 1.0F);
    assert(genome);
    const auto phenotype = genomes::infantry::PhenotypeResolver::resolve(genome.value());
    assert(phenotype);
    const auto rig = genomes::infantry::RigBuilder::build(phenotype.value().body,
                                                           phenotype.value().face);
    assert(rig);

    genomes::infantry::AppearanceOptions options{};
    options.seed = 17U;
    options.hair_style = genomes::infantry::HairStyle::Long;
    const auto artifact = genomes::infantry::AppearanceCompiler::build(
        phenotype.value(), rig.value(), options);
    assert(artifact);
    assert(artifact.value().valid(rig.value()));
    assert(artifact.value().has_eye_openings);
    assert(artifact.value().has_mouth_opening);
    assert(artifact.value().body.vertices.size() > 32U);
    assert(!artifact.value().body.indices.empty());
    assert(artifact.value().hair.vertices.size() > 0U);
    assert(artifact.value().morphs.size() == 4U);

    for (const auto& vertex : artifact.value().body.vertices) {
        assert(vertex.influence_count <= 4U);
        float total = 0.0F;
        for (std::size_t index = 0U; index < vertex.influence_count; ++index) {
            assert(vertex.influences[index].bone_index < rig.value().bones().size());
            assert(vertex.influences[index].weight > 0.0F);
            total += vertex.influences[index].weight;
        }
        assert(std::abs(total - 1.0F) < 1.0e-4F);
    }

    genomes::infantry::AppearanceOptions bald = options;
    bald.hair_style = genomes::infantry::HairStyle::Bald;
    const auto bald_artifact = genomes::infantry::AppearanceCompiler::build(
        phenotype.value(), rig.value(), bald);
    assert(bald_artifact && bald_artifact.value().hair.vertices.empty());

    genomes::infantry::AppearanceCache cache;
    const auto first = cache.acquire(phenotype.value(), rig.value(), options);
    const auto second = cache.acquire(phenotype.value(), rig.value(), options);
    assert(first && second && first->cache_key == second->cache_key);
    assert(cache.size() == 1U);
    assert(cache.hits() >= 1U && cache.misses() >= 1U);
    return 0;
}
