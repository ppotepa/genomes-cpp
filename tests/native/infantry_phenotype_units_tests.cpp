#include <genomes/infantry/PhenotypeResolver.hpp>

#include <cassert>
#include <cmath>

int main() {
    for (const auto seed : {0x5EED2026U, 0x12345678U, 0xCAFEBABEU, 0x42U}) {
        const auto genome = genomes::infantry::InfantryGenome::generate(seed, 1.0F);
        assert(genome);
        const auto phenotype = genomes::infantry::PhenotypeResolver::resolve(genome.value());
        assert(phenotype);
        const auto& body = phenotype.value().body;
        const float h = body.height;
        assert(body.shoulder_width > 0.15F * h && body.shoulder_width < 0.40F * h);
        assert(body.hip_width > 0.08F * h && body.hip_width < 0.16F * h);
        assert(body.chest_depth > 0.08F * h && body.chest_depth < 0.35F * h);
        assert(body.arm_length > 0.25F * h && body.arm_length < 0.70F * h);
        assert(body.leg_length > 0.35F * h && body.leg_length < 0.90F * h);
        assert(body.waist_width > 0.12F * h && body.waist_width < 0.35F * h);
        assert(body.limb_thickness > 0.04F * h && body.limb_thickness < 0.16F * h);
        assert(body.hip_width / h > 0.08F && body.hip_width / h < 0.14F);
        assert(body.arm_length / h > 0.30F && body.arm_length / h < 0.40F);
        assert(body.leg_length / h > 0.43F && body.leg_length / h < 0.60F);
        const auto& face = phenotype.value().face;
        assert(face.brow_y > face.eye_y && face.eye_y > face.nose_y &&
               face.nose_y > face.mouth_y && face.hairline_y > face.brow_y);
        assert(std::isfinite(body.shoulder_width) && std::isfinite(face.eye_y));
    }
    return 0;
}
