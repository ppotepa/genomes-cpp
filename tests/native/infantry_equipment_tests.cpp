#include <genomes/infantry/EquipmentCatalog.hpp>
#include <genomes/infantry/EquipmentFit.hpp>
#include <genomes/infantry/GearSurfaceGenerator.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/InfantryMaterials.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/RigBuilder.hpp>

#include <cassert>
#include <cmath>
#include <cstdint>

int main() {
    using namespace genomes::infantry;
    assert(EquipmentCatalog::slots().size() == kEquipmentSlotCount);
    assert(EquipmentCatalog::items().size() == kEquipmentItemCount);
    assert(infantryLoadouts().size() == kInfantryLoadoutCount);

    const auto genome = InfantryGenome::generate(0xA11CEU, 1.0F);
    assert(genome);
    const auto phenotype = PhenotypeResolver::resolve(genome.value());
    assert(phenotype);
    const auto rig = RigBuilder::build(phenotype.value().body, phenotype.value().face);
    assert(rig);

    for (const auto& loadout : infantryLoadouts()) {
        const auto state = EquipmentResolver::resolve(17U, loadout.id);
        assert(state && state.value().valid());
        const auto repeat = EquipmentResolver::resolve(17U, loadout.id);
        assert(repeat && repeat.value().identity == state.value().identity);
        const auto fit = EquipmentFitter::build(state.value(), phenotype.value(), rig.value());
        assert(fit && fit.value().valid(rig.value()));
        const auto gear = GearGenerator::build(state.value(), fit.value(), rig.value());
        assert(gear && gear.value().valid(rig.value()));
        const auto gear_surface = GearSurfaceGenerator::build(gear.value());
        assert(gear_surface);
        assert(gear_surface.value().vertices.size() >= gear.value().pieces.size());
        assert(gear_surface.value().indices.size() % 3U == 0U);
        for (const auto& vertex : gear_surface.value().vertices) {
            assert(std::isfinite(vertex.position.x));
            assert(std::isfinite(vertex.position.y));
            assert(std::isfinite(vertex.position.z));
            assert(std::isfinite(vertex.normal.x));
            assert(std::isfinite(vertex.normal.y));
            assert(std::isfinite(vertex.normal.z));
            assert(vertex.influence_count > 0U);
            assert(vertex.material_region <=
                   static_cast<std::uint16_t>(AppearanceMaterialRegion::EquipmentPaint));
        }
        for (const auto index : gear_surface.value().indices) {
            assert(index < gear_surface.value().vertices.size());
        }
        GearCache gear_cache;
        const auto cached_first = gear_cache.acquire(state.value(), fit.value(), rig.value());
        const auto cached_second = gear_cache.acquire(state.value(), fit.value(), rig.value());
        assert(cached_first && cached_second && cached_first->cache_key == cached_second->cache_key);
        assert(gear_cache.size() == 1U);
    }

    const auto rifleman = EquipmentCatalog::loadoutId("RIFLEMAN");
    EquipmentOverrideSet overrides{};
    overrides.slots[equipmentSlotIndex(EquipmentSlot::Head)] =
        EquipmentOverride::item(EquipmentCatalog::findItem("field_cap")->id);
    overrides.slots[equipmentSlotIndex(EquipmentSlot::LeftHip)] =
        EquipmentOverride::nullValue();
    const auto overridden = EquipmentResolver::resolve(17U, rifleman, overrides);
    assert(overridden && overridden.value().valid());
    assert(overridden.value().item(EquipmentSlot::Head) != nullptr);
    assert(overridden.value().item(EquipmentSlot::LeftHip) == nullptr);

    EquipmentOverrideSet invalid{};
    invalid.slots[equipmentSlotIndex(EquipmentSlot::Head)] =
        EquipmentOverride::item(EquipmentCatalog::findItem("rifle")->id);
    const auto rejected = EquipmentResolver::resolve(17U, rifleman, invalid);
    assert(!rejected);

    const auto reference_genome = InfantryGenome::generate(0U, 0.0);
    const auto reference_phenotype = reference_genome
        ? PhenotypeResolver::resolve(reference_genome.value())
        : genomes::foundation::Result<PhenotypeArtifact, genomes::foundation::Error>::failure(
              reference_genome.error());
    const auto reference_rig = reference_phenotype
        ? RigBuilder::build(reference_phenotype.value().body, reference_phenotype.value().face)
        : genomes::foundation::Result<SkeletonData, genomes::foundation::Error>::failure(
              reference_phenotype.error());
    const auto reference_state = EquipmentResolver::resolve(0U, rifleman);
    assert(reference_phenotype && reference_rig && reference_state);
    const auto reference_fit = EquipmentFitter::build(
        reference_state.value(), reference_phenotype.value(), reference_rig.value());
    assert(reference_fit);
    const auto reference_gear = GearGenerator::build(
        reference_state.value(), reference_fit.value(), reference_rig.value(),
        kDefaultUniformColor, 3U, .25F);
    assert(reference_gear);
    const auto reference_surface = GearSurfaceGenerator::build(reference_gear.value());
    assert(reference_surface);
    const auto tag_size = [&reference_surface](std::string_view name) {
        for (const auto& tag : reference_surface.value().tags)
            if (tag.name == name) return tag.vertices.size();
        return std::size_t{0U};
    };
    assert(tag_size("gear.head") == 1569U);
    assert(tag_size("gear.legs") == 1200U);
    assert(tag_size("gear.chestRig") == 1806U);
    assert(tag_size("gear.torsoArmor") == 1302U);
    assert(tag_size("gear.back") == 2508U);
    assert(tag_size("gear.belt") == 246U);
    assert(tag_size("gear.leftHip") == 262U);
    assert(tag_size("gear.rightHip") == 600U);
    assert(tag_size("gear.primaryWeapon") == 4U);
    assert(reference_surface.value().vertices.size() == 9497U);
    return 0;
}
