#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/proc/Seed.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
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
};

struct WeaponArtifact final {
    std::uint32_t version{1};
    WeaponId weapon_id{0};
    foundation::StableId cache_key{0};
    WeaponMesh mesh;
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

class WeaponArtifactCache final {
public:
    [[nodiscard]] const WeaponArtifact* find(foundation::StableId key) const noexcept;
    void store(WeaponArtifact artifact);
    [[nodiscard]] const WeaponArtifact* acquire(const WeaponDefinition&,
                                                const WeaponVariant& = {});
    [[nodiscard]] std::size_t size() const noexcept { return artifacts_.size(); }

private:
    std::vector<WeaponArtifact> artifacts_;
};

} // namespace genomes::weapons
