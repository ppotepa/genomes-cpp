#include <genomes/runtime/UnitLabCommandParsing.hpp>

#include <cassert>

int main() {
    using namespace genomes::runtime;
    assert(parseSetVariation("1.75"));
    assert(!parseSetVariation("1.750 trailing"));
    assert(parseSetVariation("1.750 trailing").error().command == "set-variation");
    assert(parseSetVariation("1.750 trailing").error().field == "variation");
    assert(parseSetVariation("1.750 trailing").error().input == "1.750 trailing");
    assert(!parseSetVariation("nan"));
    assert(!parseSetVariation("2.0"));

    assert(parseSetCameraMode("3q").value().value == UnitLabCameraMode::ThreeQuarter);
    assert(parseSetCameraMode("three-quarter").value().value == UnitLabCameraMode::ThreeQuarter);
    assert(!parseSetCameraMode("front trailing"));
    assert(parseSetLocomotionPreset("crouch-walk").value().value ==
           infantry::BipedPreset::CrouchWalk);
    assert(parseSetLocomotionPreset("Crouch Walk").value().value ==
           infantry::BipedPreset::CrouchWalk);
    assert(!parseSetLocomotionPreset("run "));
    assert(parseUnitLabExpression("eyes-closed"));
    assert(parseUnitLabExpression("Eyes closed"));
    assert(parseSetExpression("anger").value().value == infantry::FaceExpression::Anger);

    const auto gene = parseSetGeneOverride("height", "0.82");
    assert(gene && gene.value().gene == infantry::GenomeGene::Height);
    const auto invalid_gene_value = parseSetGeneOverride("height", "inf");
    assert(!invalid_gene_value);
    assert(invalid_gene_value.error().command == "set-gene-override");
    assert(invalid_gene_value.error().field == "gene-value");
    const auto out_of_range_gene_value = parseSetGeneOverride("height", "1.1");
    assert(!out_of_range_gene_value);
    assert(out_of_range_gene_value.error().field == "gene-value");
    const auto invalid_gene = parseSetGeneOverride("not-a-gene", "0.5");
    assert(!invalid_gene);
    assert(invalid_gene.error().field == "gene");
    return 0;
}
