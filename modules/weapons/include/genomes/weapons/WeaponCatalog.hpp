#pragma once

#include <genomes/content/ContentSnapshot.hpp>
#include <genomes/foundation/ConfigHash.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/proc/Seed.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace genomes::weapons {

using WeaponId = foundation::StableId;

enum class WeaponCategory : std::uint8_t {
    OneHanded,
    Sidearm,
    Carbine,
    Rifle,
    MarksmanRifle,
    SupportGun,
    HeavySupportGun,
};

enum class WeaponGrip : std::uint8_t {
    OneHanded,
    TwoHanded,
    Supported,
};

enum class WeaponMount : std::uint8_t {
    Melee,
    Throwable,
    Secondary,
    Primary,
};

enum class WeaponFamily : std::uint8_t { OneHanded, TwoHanded };
enum class WeaponVisualKind : std::uint8_t { Knife, Grenade, Pistol, Long };

struct WeaponDefinition final {
    WeaponId id{0};
    std::string_view identifier{};
    WeaponCategory category{WeaponCategory::OneHanded};
    WeaponGrip grip{WeaponGrip::OneHanded};
    WeaponMount mount{WeaponMount::Primary};
    bool firearm{false};
    foundation::StableId ammunition_id{0};
    foundation::Vec3 dimensions{0.1F, 0.1F, 0.1F};
    foundation::Vec3 muzzle{0.0F, 0.0F, 0.0F};
    foundation::Vec3 primary_grip{0.0F, 0.0F, 0.0F};
    foundation::Vec3 support_grip{0.0F, 0.0F, 0.0F};
    foundation::Vec3 stow_anchor{0.0F, 0.0F, 0.0F};
    float rounds_per_second{0.0F};
    float muzzle_velocity_mps{0.0F};
    float range_m{0.0F};
    float damage{0.0F};
    std::uint32_t version{1};
    WeaponFamily family{WeaponFamily::OneHanded};
    WeaponVisualKind visual_kind{WeaponVisualKind::Knife};
    std::uint8_t grip_profile_mask{1U}; // bit 0: 1H, bit 1: 2H
    float visual_length{0.1F};
    float draw_seconds{1.0F};
    float holster_seconds{1.0F};
    float fire_interval_seconds{0.0F};
    float visual_kick{0.0F};

    [[nodiscard]] bool valid() const noexcept;
};

[[nodiscard]] constexpr WeaponId weapon_id(std::string_view identifier) noexcept {
    return foundation::stable_id(identifier);
}

class WeaponCatalog final {
public:
    [[nodiscard]] static std::span<const WeaponDefinition> entries() noexcept;
    [[nodiscard]] static const WeaponDefinition* find(WeaponId) noexcept;
    [[nodiscard]] static const WeaponDefinition* find(std::string_view identifier) noexcept {
        return find(weapon_id(identifier));
    }
    [[nodiscard]] static foundation::Result<void, foundation::Error> validate() noexcept;
};

// A loaded catalog owns the resolved definitions and their source snapshot.
// The fixture is a parity manifest; numeric values remain authoritative in
// the existing native catalog until a value-bearing schema is introduced.
class FrozenWeaponCatalog final {
public:
    [[nodiscard]] std::span<const WeaponDefinition> entries() const noexcept {
        return definitions_;
    }
    [[nodiscard]] const WeaponDefinition* find(WeaponId id) const noexcept;
    [[nodiscard]] const WeaponDefinition* find(std::string_view identifier) const noexcept {
        return find(weapon_id(identifier));
    }
    [[nodiscard]] std::size_t size() const noexcept { return definitions_.size(); }
    [[nodiscard]] bool frozen() const noexcept { return frozen_; }
    [[nodiscard]] std::string_view sourceCommit() const noexcept { return source_commit_; }
    [[nodiscard]] const content::FrozenContentSnapshot& contentSnapshot() const noexcept {
        return snapshot_;
    }
    [[nodiscard]] foundation::SimConfigHash fingerprint() const noexcept { return fingerprint_; }

private:
    friend foundation::Result<FrozenWeaponCatalog, foundation::Error> loadWeaponCatalog(
        const std::filesystem::path& path);

    std::vector<WeaponDefinition> definitions_;
    std::string source_commit_;
    content::FrozenContentSnapshot snapshot_{};
    foundation::SimConfigHash fingerprint_{};
    bool frozen_{false};
};

[[nodiscard]] foundation::Result<FrozenWeaponCatalog, foundation::Error>
loadWeaponCatalog(const std::filesystem::path& path);

struct WeaponVariant final {
    proc::Seed seed{0};
    float scale{1.0F};
    float wear{0.0F};
    std::uint32_t material_variant{0};

    [[nodiscard]] bool valid() const noexcept;
};

struct WeaponVertex final {
    foundation::Vec3 position{};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
    foundation::Vec2 uv{};
    foundation::Color color{1.0F, 1.0F, 1.0F, 1.0F};
    std::uint32_t material_region{0};
};

struct WeaponMesh final {
    std::vector<WeaponVertex> vertices;
    std::vector<std::uint32_t> indices;
    foundation::Vec3 minimum{};
    foundation::Vec3 maximum{};
};

struct WeaponAttachment final {
    foundation::Vec3 local_position{};
    foundation::Vec3 local_forward{0.0F, 0.0F, 1.0F};
    std::array<float,4U> local_rotation{0.0F,0.0F,0.0F,1.0F};
};

struct WeaponArtifact final {
    std::uint32_t version{1};
    WeaponId weapon_id{0};
    foundation::StableId cache_key{0};
    WeaponMesh mesh;
    WeaponMesh slide;
    WeaponMesh muzzle_flash;
    WeaponAttachment muzzle{};
    WeaponAttachment primary_grip{};
    WeaponAttachment support_grip{};
    WeaponAttachment stow_anchor{};

    [[nodiscard]] bool valid(const WeaponDefinition&) const noexcept;
};

class WeaponGeometryGenerator final {
public:
    [[nodiscard]] static foundation::Result<WeaponArtifact, foundation::Error> build(
        const WeaponDefinition&, const WeaponVariant& = {});
    [[nodiscard]] static foundation::StableId cacheKey(const WeaponDefinition&,
                                                       const WeaponVariant&) noexcept;
};

} // namespace genomes::weapons
