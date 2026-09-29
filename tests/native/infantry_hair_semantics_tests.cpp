#include <genomes/infantry/AppearanceArtifact.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/FaceAnatomy.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/RigBuilder.hpp>

#include <array>
#include <cassert>
#include <cmath>

int main() {
    using namespace genomes::infantry;
    const auto genome = InfantryGenome::generate(0x5EED2026U, 1.0F);
    assert(genome);
    const auto phenotype = PhenotypeResolver::resolve(genome.value());
    assert(phenotype);
    const auto rig = RigBuilder::build(phenotype.value().body, phenotype.value().face);
    assert(rig);
    std::array<bool, 7U> names{};
    const std::array styles{HairStyle::Bald, HairStyle::Buzz, HairStyle::Crew,
                            HairStyle::Crop, HairStyle::SidePart, HairStyle::Fade,
                            HairStyle::Messy};
    for (std::size_t index = 0U; index < styles.size(); ++index) {
        assert(!hairStyleName(styles[index]).empty());
        names[index] = true;
        AppearanceOptions options{};
        options.seed = 0x5EED2026U;
        options.hair_style = styles[index];
        const auto first = AppearanceCompiler::build(phenotype.value(), rig.value(), options);
        const auto second = AppearanceCompiler::build(phenotype.value(), rig.value(), options);
        assert(first && second && first.value().cache_key == second.value().cache_key);
        if (styles[index] == HairStyle::Bald) {
            assert(first.value().hair.vertices.empty());
        }
    }
    for (const bool present : names) {
        assert(present);
    }
    const auto anatomy = FaceAnatomyEvaluator::resolve(phenotype.value());
    assert(anatomy);
    auto more_volume = phenotype.value();
    more_volume.face.hair_volume *= 1.8F;
    const auto volume_anatomy = FaceAnatomyEvaluator::resolve(more_volume);
    assert(volume_anatomy);
    assert(std::abs(anatomy.value().scalp.crown_y -
                    volume_anatomy.value().scalp.crown_y) < 1.0e-6F);
    assert(std::abs(anatomy.value().scalp.crown_y -
                    anatomy.value().head_sections.back().y) < 1.0e-6F);

    auto no_shape = anatomy.value();
    no_shape.scalp.temple_recession = 0.0F;
    no_shape.scalp.widow_peak = 0.0F;
    auto recession = no_shape;
    recession.scalp.temple_recession = 0.012F;
    auto widow = no_shape;
    widow.scalp.widow_peak = 0.009F;
    constexpr float pi = 3.14159265358979323846F;
    const float temple_angle = pi * 0.25F;
    const float front_angle = pi * 0.5F;
    assert(FaceAnatomyEvaluator::hairlineY(recession, temple_angle) >
           FaceAnatomyEvaluator::hairlineY(no_shape, temple_angle));
    assert(FaceAnatomyEvaluator::hairlineY(widow, front_angle) <
           FaceAnatomyEvaluator::hairlineY(no_shape, front_angle));

    AppearanceOptions volume_options{};
    volume_options.seed = 0x5EED2026U;
    volume_options.hair_style = HairStyle::Crew;
    auto fuller = phenotype.value();
    fuller.face.hair_volume *= 1.8F;
    const auto base_hair = AppearanceCompiler::build(phenotype.value(), rig.value(), volume_options);
    const auto full_hair = AppearanceCompiler::build(fuller, rig.value(), volume_options);
    assert(base_hair && full_hair);
    assert(full_hair.value().maximum.y > base_hair.value().maximum.y);
    auto receding = phenotype.value();
    receding.face.temple_recession = 0.012F;
    const auto receded = AppearanceCompiler::build(receding, rig.value(), volume_options);
    assert(receded && receded.value().hair.vertices.size() == base_hair.value().hair.vertices.size());
    AppearanceOptions covered_options = volume_options;
    covered_options.hair_coverage = 0.92F;
    const auto covered = AppearanceCompiler::build(
        phenotype.value(), rig.value(), covered_options);
    assert(covered && covered.value().cache_key != base_hair.value().cache_key);
    assert(covered.value().hair.vertices.size() == base_hair.value().hair.vertices.size());
    bool coverage_changed_geometry = false;
    for (std::size_t index = 0U; index < base_hair.value().hair.vertices.size(); ++index) {
        coverage_changed_geometry = coverage_changed_geometry ||
            covered.value().hair.vertices[index].position.y !=
            base_hair.value().hair.vertices[index].position.y;
    }
    assert(coverage_changed_geometry);
    return 0;
}
