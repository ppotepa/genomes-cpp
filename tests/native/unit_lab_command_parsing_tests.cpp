#include <genomes/runtime/UnitLabCommandParsing.hpp>

#include <cassert>

int main() {
    using namespace genomes::runtime;
    assert(parseSetVariation("1.75"));
    assert(!parseSetVariation("1.750 trailing"));
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
    assert(!parseUnitLabExpression("Eyes closed"));

    const auto gene = parseSetGeneOverride("height", "0.82");
    assert(gene && gene.value().gene == infantry::GenomeGene::Height);
    assert(!parseSetGeneOverride("height", "inf"));
    assert(!parseSetGeneOverride("height", "1.1"));
    return 0;
}
