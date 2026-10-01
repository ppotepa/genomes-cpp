#include <genomes/infantry/EquipmentCatalog.hpp>

#include <cassert>
#include <filesystem>

int main() {
    const auto fixture = std::filesystem::path{GENOMES_SOURCE_DIR} /
                         "reference/fixtures/infantry/equipment_catalog_v1.json";
    const auto loaded = genomes::infantry::loadEquipmentCatalog(fixture);
    assert(loaded);
    const auto& catalog = loaded.value();
    assert(catalog.frozen());
    assert(catalog.slots().size() == genomes::infantry::kEquipmentSlotCount);
    assert(catalog.items().size() == genomes::infantry::EquipmentCatalog::items().size());
    assert(catalog.loadouts().size() == genomes::infantry::kInfantryLoadoutCount);
    assert(catalog.sourceCommit() == "da885ca68b2ae63154a004574fed00eb9dfeb458");
    assert(catalog.contentSnapshot().package_id == "infantry.equipment.catalog");
    assert(catalog.contentSnapshot().sources.size() == 1U);
    assert(catalog.fingerprint().value != 0U);
    assert(catalog.findItem("rifle") != nullptr);
    assert(catalog.findItem("not-a-catalog-item") == nullptr);
    return 0;
}
