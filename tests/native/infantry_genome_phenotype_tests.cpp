#include <genomes/infantry/PhenotypeResolver.hpp>

#include <cassert>
#include <cmath>
#include <limits>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

int main() {
    using namespace genomes::infantry;

    const auto first = InfantryGenome::generate(0x12345678U, 1.0F);
    const auto second = InfantryGenome::generate(0x12345678U, 1.0F);
    assert(first && second);
    assert(first.value().identityHash() == second.value().identityHash());
    assert(first.value().valid());

    const auto zero_variation = first.value().applyVariation(0.0F);
    assert(zero_variation);
    assert(std::abs(zero_variation.value().geneValue(GenomeGene::Speed) - 0.5) < 1.0e-7);
    assert(std::abs(zero_variation.value().body.shoulder_width - 0.5F) < 1.0e-6F);
    assert(std::abs(zero_variation.value().face.eye_spacing - 0.5F) < 1.0e-6F);

    GenomeOverrides overrides{};
    overrides.height = 1.91F;
    overrides.eye_spacing = 1.0F;
    overrides.jaw_width = 0.0F;
    const auto resolved = PhenotypeResolver::resolve(first.value(), overrides);
    assert(resolved);
    assert(resolved.value().valid());
    assert(std::abs(resolved.value().requested.height - 1.91F) < 1.0e-6F);
    assert(resolved.value().face.brow_y > resolved.value().face.eye_y);
    assert(resolved.value().face.eye_y > resolved.value().face.nose_y);
    assert(resolved.value().face.nose_y > resolved.value().face.mouth_y);
    assert(resolved.value().face.frontZ(resolved.value().face.eye_y) > 0.0F);
    const auto eye_point = resolved.value().face.point(resolved.value().face.eye_y, 1.5707963F);
    assert(std::isfinite(eye_point.x) && std::isfinite(eye_point.z));
    assert(resolved.value().diagnostics.face_spacing_adjusted);

    GenomeOverrides typed{};
    for (std::size_t index = 0; index < GenomeGeneCount; ++index) {
        const auto gene = static_cast<GenomeGene>(index);
        assert(!genomeGeneName(gene).empty());
        assert(genomeGeneFromName(genomeGeneName(gene)) == gene);
    }
    assert(typed.set(GenomeGene::Height, 1.0));
    assert(typed.set("body.shoulderBreadthGene", 0.0));
    assert(typed.set("face.hairStyleGene", 1.0));
    assert(!typed.set("face.notAGene", 0.5));
    assert(!typed.set(GenomeGene::Speed, -0.1));
    assert(typed.get(GenomeGene::Height) == 1.0);
    const auto typed_genome = first.value().withOverrides(typed);
    assert(typed_genome);
    assert(std::abs(typed_genome.value().height - 1.95F) < 1.0e-6F);
    assert(typed_genome.value().body.shoulder_width == 0.0F);
    assert(typed_genome.value().face.hair_style == 1.0F);

    GenomeOverrides invalid{};
    invalid.nose_width = std::numeric_limits<float>::quiet_NaN();
    assert(!PhenotypeResolver::resolve(first.value(), invalid));

    // The broad catalog is the 1024-seed parity gate, not merely a checked-in
    // count: every generated gene must remain within the contract tolerance.
    std::ifstream catalog_file(std::filesystem::path(GENOMES_SOURCE_DIR) /
                               "reference/fixtures/infantry/genome_catalog_v1.json");
    assert(catalog_file.good());
    const nlohmann::json catalog = nlohmann::json::parse(catalog_file);
    assert(catalog.at("request").at("count") == 1024U);
    assert(catalog.at("catalog").size() == 1024U);
    for (const auto& entry : catalog.at("catalog")) {
        const auto generated = InfantryGenome::generate(
            entry.at("seed").get<std::uint32_t>(), 1.0F);
        assert(generated);
        const auto& genes = entry.at("genes");
        assert(genes.size() == GenomeGeneCount);
        for (std::size_t index = 0U; index < GenomeGeneCount; ++index) {
            const double expected = genes.at(index).get<double>();
            const double actual = generated.value().geneValue(static_cast<GenomeGene>(index));
            assert(std::isfinite(actual));
            assert(std::abs(actual - expected) <= 2.0e-6);
        }
    }
    return 0;
}
