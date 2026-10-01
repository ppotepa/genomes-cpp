#include <genomes/weapons/WeaponCatalog.hpp>

#include <cassert>

int main() {
    using namespace genomes::weapons;
    assert(WeaponCatalog::entries().size() == 9U);
    assert(WeaponCatalog::validate());
    const auto* knife = WeaponCatalog::find("knife");
    const auto* grenade = WeaponCatalog::find("grenade");
    const auto* rifle = WeaponCatalog::find("rifle");
    const auto* infantry_default = WeaponCatalog::find("infantry_default");
    assert(knife != nullptr && grenade != nullptr && rifle != nullptr && infantry_default != nullptr);
    assert(!knife->firearm && !grenade->firearm);
    assert(rifle->firearm && rifle->ammunition_id != 0U);
    assert(infantry_default->firearm && infantry_default->rounds_per_second == 2.0F &&
           infantry_default->muzzle_velocity_mps == 700.0F && infantry_default->damage == 4.0F);
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
