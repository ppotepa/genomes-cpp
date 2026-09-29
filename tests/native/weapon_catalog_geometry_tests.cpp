#include <genomes/weapons/WeaponCatalog.hpp>

#include <cassert>

int main() {
    using namespace genomes::weapons;
    assert(WeaponCatalog::entries().size() == 8U);
    assert(WeaponCatalog::validate());
    const auto* knife = WeaponCatalog::find("knife");
    const auto* grenade = WeaponCatalog::find("grenade");
    const auto* rifle = WeaponCatalog::find("rifle");
    assert(knife != nullptr && grenade != nullptr && rifle != nullptr);
    assert(!knife->firearm && !grenade->firearm);
    assert(rifle->firearm && rifle->ammunition_id != 0U);
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
