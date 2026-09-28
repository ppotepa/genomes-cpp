#include <genomes/infantry/PhenotypeResolver.hpp>

#include <cassert>
#include <cmath>
#include <limits>

int main() {
    using namespace genomes::infantry;

    const auto first = InfantryGenome::generate(0x12345678U, 1.0F);
    const auto second = InfantryGenome::generate(0x12345678U, 1.0F);
    assert(first && second);
    assert(first.value().identityHash() == second.value().identityHash());
    assert(first.value().valid());

    const auto zero_variation = first.value().applyVariation(0.0F);
    assert(zero_variation);
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

    GenomeOverrides invalid{};
    invalid.nose_width = std::numeric_limits<float>::quiet_NaN();
    assert(!PhenotypeResolver::resolve(first.value(), invalid));
    return 0;
}
