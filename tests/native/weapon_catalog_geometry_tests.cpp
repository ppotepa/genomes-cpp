#include <genomes/weapons/WeaponCatalog.hpp>

#include <cassert>
#include <filesystem>

int main() {
    using namespace genomes::weapons;
    assert(WeaponCatalog::entries().size() == 9U);
    assert(WeaponCatalog::validate());
    for (const auto& definition : WeaponCatalog::entries()) {
        assert(definition.id == weapon_id(definition.identifier));
        if (definition.firearm) assert(definition.ammunition_id != 0U);
    }
    const auto* knife = WeaponCatalog::find("knife");
    const auto* grenade = WeaponCatalog::find("grenade");
    const auto* rifle = WeaponCatalog::find("rifle");
    const auto* infantry_default = WeaponCatalog::find("infantry_default");
    assert(knife != nullptr && grenade != nullptr && rifle != nullptr && infantry_default != nullptr);
    assert(!knife->firearm && !grenade->firearm);
    assert(rifle->firearm && rifle->ammunition_id != 0U);
    assert(infantry_default->firearm && infantry_default->rounds_per_second == 2.0F &&
           infantry_default->muzzle_velocity_mps == 700.0F && infantry_default->damage == 4.0F);

    const auto loaded_result = loadWeaponCatalog(
        std::filesystem::path{GENOMES_SOURCE_DIR} /
        "reference/fixtures/weapons_catalog_profiles.json");
    assert(loaded_result);
    const FrozenWeaponCatalog& loaded = loaded_result.value();
    assert(loaded.frozen() && loaded.size() == 8U);
    assert(loaded.sourceCommit() == "da885ca68b2ae63154a004574fed00eb9dfeb458");
    assert(loaded.contentSnapshot().package_id == "weapons.catalog");
    assert(loaded.fingerprint().value != 0U);
    for (const auto& expected : loaded.entries()) {
        const auto* actual = WeaponCatalog::find(expected.identifier);
        assert(actual != nullptr);
        assert(actual->id == expected.id);
        assert(actual->firearm == expected.firearm);
        assert(actual->ammunition_id == expected.ammunition_id);
        assert(actual->dimensions.x == expected.dimensions.x &&
               actual->dimensions.y == expected.dimensions.y &&
               actual->dimensions.z == expected.dimensions.z);
        assert(actual->rounds_per_second == expected.rounds_per_second);
        assert(actual->muzzle_velocity_mps == expected.muzzle_velocity_mps);
        assert(actual->damage == expected.damage);
    }
    const auto knife_geometry=WeaponGeometryGenerator::build(*knife);
    const auto grenade_geometry=WeaponGeometryGenerator::build(*grenade);
    assert(knife_geometry && grenade_geometry);
    assert(knife_geometry.value().mesh.vertices.size()==125U);
    assert(knife_geometry.value().mesh.indices.size()/3U==120U);
    assert(grenade_geometry.value().mesh.vertices.size()==408U);
    assert(grenade_geometry.value().mesh.indices.size()/3U==616U);

    WeaponVariant variant{};
    variant.seed = 0x1234U;
    variant.material_variant = 3U;
    const auto first = WeaponGeometryGenerator::build(*rifle, variant);
    const auto second = WeaponGeometryGenerator::build(*rifle, variant);
    assert(first && second && first.value().valid(*rifle));
    assert(first.value().cache_key == second.value().cache_key);
    assert(first.value().mesh.vertices.size() == 3588U);
    assert(first.value().mesh.indices.size() / 3U == 3396U);
    WeaponArtifactCache cache;
    assert(cache.acquire(*rifle, variant) != nullptr);
    assert(cache.acquire(*rifle, variant) != nullptr);
    assert(cache.size() == 1U);
    return 0;
}
