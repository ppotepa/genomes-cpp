#include <genomes/infantry/EquipmentCatalog.hpp>
#include <genomes/infantry/EquipmentFit.hpp>
#include <genomes/infantry/InfantryGenome.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/RigBuilder.hpp>

#include <cassert>

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
        const auto fit = EquipmentFitter::build(state.value(), phenotype.value().body, rig.value());
        assert(fit && fit.value().valid(rig.value()));
        const auto gear = GearGenerator::build(state.value(), fit.value(), rig.value());
        assert(gear && gear.value().valid(rig.value()));
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
    return 0;
}
