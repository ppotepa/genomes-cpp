#include <genomes/infantry/AppearanceArtifact.hpp>
#include <genomes/infantry/EquipmentCatalog.hpp>
#include <genomes/infantry/EquipmentFit.hpp>
#include <genomes/infantry/FaceAnimation.hpp>
#include <genomes/infantry/InfantryDamage.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/RagdollSchema.hpp>
#include <genomes/infantry/RigBuilder.hpp>

#include <array>
#include <cassert>

int main() {
    using namespace genomes::infantry;
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
