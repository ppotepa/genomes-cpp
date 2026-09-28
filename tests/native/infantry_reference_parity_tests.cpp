#include <genomes/infantry/AppearanceArtifact.hpp>
#include <genomes/infantry/EquipmentCatalog.hpp>
#include <genomes/infantry/EquipmentFit.hpp>
#include <genomes/infantry/FaceAnimation.hpp>
#include <genomes/infantry/FaceAnatomy.hpp>
#include <genomes/infantry/InfantryDamage.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/RagdollSchema.hpp>
#include <genomes/infantry/RigBuilder.hpp>

#include <array>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>

namespace {

[[nodiscard]] float fixtureNumber(const std::string& text, std::size_t capture_start,
                                  std::string_view field) {
    const std::size_t field_start = text.find('"' + std::string(field) + '"', capture_start);
    assert(field_start != std::string::npos);
    const std::size_t colon = text.find(':', field_start);
    assert(colon != std::string::npos);
    char* end = nullptr;
    const float value = std::strtof(text.c_str() + colon + 1U, &end);
    assert(end != text.c_str() + colon + 1U);
    return value;
}

} // namespace

int main() {
    using namespace genomes::infantry;
    std::ifstream fixture(std::string(GENOMES_SOURCE_DIR) +
                          "/reference/fixtures/infantry/semantic_reference_v1.json");
    const std::string fixture_text((std::istreambuf_iterator<char>(fixture)),
                                   std::istreambuf_iterator<char>());
    assert(fixture_text.find("\"schema_version\": 1") != std::string::npos);
    assert(fixture_text.find("da885ca68b2ae63154a004574fed00eb9dfeb458") !=
           std::string::npos);
    assert(fixture_text.find("1592598566") != std::string::npos);
    assert(fixture_text.find("305419896") != std::string::npos);
    assert(fixture_text.find("3405691582") != std::string::npos);
    const std::array<genomes::proc::Seed, 3U> fixture_seeds{
        0x5EED2026U, 0x12345678U, 0xCAFEBABEU};
    for (const auto seed : fixture_seeds) {
        const std::size_t capture_start = fixture_text.find(
            "\"seed\": " + std::to_string(seed));
        assert(capture_start != std::string::npos);
        const auto genome = InfantryGenome::generate(seed, 1.0F);
        assert(genome);
        const auto phenotype = PhenotypeResolver::resolve(genome.value());
        assert(phenotype);
        const float reference_height = fixtureNumber(fixture_text, capture_start, "height");
        const float reference_shoulder = fixtureNumber(
            fixture_text, capture_start, "shoulder_width_scale");
        const float reference_hip = fixtureNumber(
            fixture_text, capture_start, "hip_width_scale");
        const float reference_eye_ratio = fixtureNumber(fixture_text, capture_start, "eye_y");
        const float reference_mouth_ratio = fixtureNumber(fixture_text, capture_start, "mouth_y");
        const float reference_head_level_count = fixtureNumber(
            fixture_text, capture_start, "head_level_count");
        assert(reference_height >= 1.60F && reference_height <= 1.95F);
        assert(std::abs(phenotype.value().body.height - reference_height) < 1.0e-6F);
        assert(std::abs(phenotype.value().body.shoulder_width /
                            (0.256F * phenotype.value().body.height) - reference_shoulder) <
               1.0e-5F);
        assert(std::abs(phenotype.value().body.hip_width -
                        (0.104F * phenotype.value().body.height * reference_hip)) < 1.0e-5F);
        assert(std::abs(phenotype.value().face.eye_y / phenotype.value().body.height -
                        reference_eye_ratio) < 0.02F);
        assert(std::abs(phenotype.value().face.mouth_y / phenotype.value().body.height -
                        reference_mouth_ratio) < 0.02F);
        const auto anatomy = FaceAnatomyEvaluator::resolve(phenotype.value());
        assert(anatomy && anatomy.value().head_sections.size() ==
                            static_cast<std::size_t>(reference_head_level_count));
    }
    constexpr std::array<genomes::proc::Seed, 5U> seeds{
        0U, 1U, 0x12345678U, 0xDEADBEEFU, 0xFFFFFFFFU};
    for (const auto seed : seeds) {
        const auto first = InfantryGenome::generate(seed, 1.0F);
        const auto second = InfantryGenome::generate(seed, 1.0F);
        assert(first && second && first.value().identityHash() == second.value().identityHash());
        const auto phenotype = PhenotypeResolver::resolve(first.value());
        const auto repeated = PhenotypeResolver::resolve(second.value());
        assert(phenotype && repeated && phenotype.value().cache_key == repeated.value().cache_key);
        const auto rig = RigBuilder::build(phenotype.value().body, phenotype.value().face);
        assert(rig && rig.value().valid() && rig.value().bones().size() == kRigBoneCount);

        AppearanceOptions options{};
        options.seed = seed;
        options.hair_style = static_cast<HairStyle>(seed % 7U);
        const auto appearance = AppearanceCompiler::build(phenotype.value(), rig.value(), options);
        assert(appearance && appearance.value().valid(rig.value()));

        for (const auto& loadout : infantryLoadouts()) {
            const auto equipment = EquipmentResolver::resolve(seed, loadout.id);
            assert(equipment && equipment.value().valid());
            const auto fit = EquipmentFitter::build(equipment.value(), phenotype.value().body,
                                                     rig.value());
            assert(fit && fit.value().valid(rig.value()));
            const auto gear = GearGenerator::build(equipment.value(), fit.value(), rig.value());
            assert(gear && gear.value().valid(rig.value()));
        }

        const auto locomotion = LocomotionController::create(phenotype.value().body);
        assert(locomotion);
        LocomotionState locomotion_state = locomotion.value().initialState();
        assert(locomotion.value().setPreset(locomotion_state, BipedPreset::Walk));
        for (int tick = 0; tick < 60; ++tick) {
            assert(locomotion.value().step(locomotion_state, 1.0F / 60.0F));
        }
        assert(locomotion_state.valid());

        const auto face = FaceAnimator::create(seed, phenotype.value().face);
        assert(face);
        FaceAnimator face_copy = face.value();
        assert(face_copy.setExpression(FaceExpression::Alert, 0.5F));
        for (int tick = 0; tick < 60; ++tick) {
            assert(face_copy.step(1.0F / 60.0F));
        }
        assert(face_copy.output().valid() && face_copy.identity().valid());

        const auto damage_schema = RagdollSchema::build(phenotype.value().body, rig.value());
        assert(damage_schema && damage_schema.value().valid());
        const auto volumes = InfantryDamageModel::buildVolumes({1U, 1U}, rig.value());
        assert(volumes && volumes.value().size() == InfantryDamageModel::recipes().size());
    }
    return 0;
}
