#include <genomes/weapons/WeaponCatalog.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace genomes::weapons {

namespace {

[[nodiscard]] foundation::Vec3 dimensions(float x, float y, float z) noexcept { return {x, y, z}; }

const std::array<WeaponDefinition, 8> kCatalog{{
    {weapon_id("knife"), "knife", WeaponCategory::OneHanded, WeaponGrip::OneHanded,
     WeaponMount::Melee, false, 0U, dimensions(0.42F, 0.035F, 0.07F), {0.18F, 0.0F, 0.0F},
     {-0.12F, 0.0F, 0.0F}, {}, {-0.08F, -0.08F, 0.0F}, 0.0F, 0.0F, 4.0F, 28.0F, 1U,
     WeaponFamily::OneHanded, WeaponVisualKind::Knife, 1U, .25F, 1.00F, 1.05F, 0, 0},
    {weapon_id("grenade"), "grenade", WeaponCategory::OneHanded, WeaponGrip::OneHanded,
     WeaponMount::Throwable, false, 0U, dimensions(0.08F, 0.12F, 0.08F), {},
     {0.0F, -0.06F, 0.0F}, {}, {-0.08F, -0.08F, 0.0F}, 0.0F, 0.0F, 35.0F, 80.0F, 1U,
     WeaponFamily::OneHanded, WeaponVisualKind::Grenade, 1U, .105F, .95F, 1.00F, 0, 0},
    {weapon_id("sidearm"), "sidearm", WeaponCategory::Sidearm, WeaponGrip::OneHanded,
     WeaponMount::Secondary, true, foundation::stable_id("ammo_9mm"),
     dimensions(0.20F, 0.14F, 0.035F), {0.20F, 0.0F, 0.0F}, {0.0F, -0.07F, 0.0F}, {},
     {-0.08F, -0.08F, 0.0F}, 10.0F, 360.0F, 120.0F, 20.0F, 1U,
     WeaponFamily::OneHanded, WeaponVisualKind::Pistol, 3U, .215F, 1.05F, 1.12F, .23F, .20F},
    {weapon_id("carbine"), "carbine", WeaponCategory::Carbine, WeaponGrip::TwoHanded,
     WeaponMount::Primary, true, foundation::stable_id("ammo_556"),
     dimensions(0.82F, 0.18F, 0.065F), {0.40F, 0.0F, 0.0F}, {-0.14F, -0.03F, 0.0F},
     {0.15F, -0.02F, 0.0F}, {-0.16F, -0.10F, 0.0F}, 12.0F, 870.0F, 300.0F, 18.0F, 1U,
     WeaponFamily::TwoHanded, WeaponVisualKind::Long, 2U, .70F, 1.38F, 1.45F, .17F, .23F},
    {weapon_id("rifle"), "rifle", WeaponCategory::Rifle, WeaponGrip::TwoHanded,
     WeaponMount::Primary, true, foundation::stable_id("ammo_556"),
     dimensions(0.98F, 0.20F, 0.07F), {0.49F, 0.0F, 0.0F}, {-0.16F, -0.03F, 0.0F},
     {0.19F, -0.02F, 0.0F}, {-0.18F, -0.11F, 0.0F}, 10.0F, 900.0F, 350.0F, 22.0F, 1U,
     WeaponFamily::TwoHanded, WeaponVisualKind::Long, 2U, .84F, 1.45F, 1.50F, .19F, .25F},
    {weapon_id("marksman_rifle"), "marksman_rifle", WeaponCategory::MarksmanRifle,
     WeaponGrip::TwoHanded, WeaponMount::Primary, true, foundation::stable_id("ammo_762"),
     dimensions(1.18F, 0.22F, 0.075F), {0.59F, 0.0F, 0.0F}, {-0.18F, -0.03F, 0.0F},
     {0.24F, -0.02F, 0.0F}, {-0.20F, -0.12F, 0.0F}, 3.5F, 820.0F, 600.0F, 58.0F, 1U,
     WeaponFamily::TwoHanded, WeaponVisualKind::Long, 2U, .96F, 1.48F, 1.53F, .36F, .29F},
    {weapon_id("support_gun"), "support_gun", WeaponCategory::SupportGun, WeaponGrip::Supported,
     WeaponMount::Primary, true, foundation::stable_id("ammo_556"),
     dimensions(1.05F, 0.25F, 0.09F), {0.51F, 0.0F, 0.0F}, {-0.19F, -0.04F, 0.0F},
     {0.22F, -0.03F, 0.0F}, {-0.22F, -0.14F, 0.0F}, 11.0F, 850.0F, 350.0F, 20.0F, 1U,
     WeaponFamily::TwoHanded, WeaponVisualKind::Long, 2U, .94F, 1.55F, 1.60F, .15F, .22F},
    {weapon_id("heavy_support_gun"), "heavy_support_gun", WeaponCategory::HeavySupportGun,
     WeaponGrip::Supported, WeaponMount::Primary, true, foundation::stable_id("ammo_762"),
     dimensions(1.34F, 0.30F, 0.12F), {0.66F, 0.0F, 0.0F}, {-0.23F, -0.05F, 0.0F},
     {0.30F, -0.03F, 0.0F}, {-0.26F, -0.16F, 0.0F}, 8.0F, 780.0F, 500.0F, 38.0F, 1U,
     WeaponFamily::TwoHanded, WeaponVisualKind::Long, 2U, 1.00F, 1.60F, 1.65F, .18F, .28F},
}};

[[nodiscard]] bool finite_position(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

} // namespace

bool WeaponDefinition::valid() const noexcept {
    return id != 0U && !identifier.empty() && finite_position(dimensions) && dimensions.x > 0.0F &&
           dimensions.y > 0.0F && dimensions.z > 0.0F && finite_position(muzzle) && finite_position(primary_grip) &&
           finite_position(support_grip) && finite_position(stow_anchor) && version > 0U &&
           grip_profile_mask > 0U && grip_profile_mask <= 3U &&
           std::isfinite(visual_length) && visual_length > 0.0F &&
           std::isfinite(draw_seconds) && draw_seconds > 0.0F &&
           std::isfinite(holster_seconds) && holster_seconds > 0.0F &&
           std::isfinite(fire_interval_seconds) && fire_interval_seconds >= 0.0F &&
           std::isfinite(visual_kick) && visual_kick >= 0.0F &&
           ((!firearm && ammunition_id == 0U && rounds_per_second == 0.0F &&
             muzzle_velocity_mps == 0.0F) ||
            (firearm && ammunition_id != 0U && std::isfinite(rounds_per_second) &&
             rounds_per_second > 0.0F && std::isfinite(muzzle_velocity_mps) &&
             muzzle_velocity_mps > 0.0F && std::isfinite(range_m) && range_m > 0.0F &&
             std::isfinite(damage) && damage > 0.0F));
}

std::span<const WeaponDefinition> WeaponCatalog::entries() noexcept { return kCatalog; }

const WeaponDefinition* WeaponCatalog::find(WeaponId id) noexcept {
    for (const WeaponDefinition& definition : kCatalog) {
        if (definition.id == id) {
            return &definition;
        }
    }
    return nullptr;
}

foundation::Result<void, foundation::Error> WeaponCatalog::validate() noexcept {
    for (std::size_t index = 0U; index < kCatalog.size(); ++index) {
        if (!kCatalog[index].valid()) {
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState, "invalid weapon catalog definition"});
        }
        for (std::size_t other = index + 1U; other < kCatalog.size(); ++other) {
            if (kCatalog[index].id == kCatalog[other].id) {
                return foundation::Result<void, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState, "duplicate weapon catalog ID"});
            }
        }
    }
    return foundation::Result<void, foundation::Error>::success();
}

bool WeaponVariant::valid() const noexcept {
    return std::isfinite(scale) && scale > 0.5F && scale < 1.5F && std::isfinite(wear) &&
           wear >= 0.0F && wear <= 1.0F;
}

} // namespace genomes::weapons
