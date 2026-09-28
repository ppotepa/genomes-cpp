#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/infantry/RigSchema.hpp>
#include <genomes/proc/Seed.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace genomes::infantry {

inline constexpr std::size_t kEquipmentSlotCount = 22U;
inline constexpr std::size_t kEquipmentItemCount = 61U;
inline constexpr std::size_t kInfantryLoadoutCount = 10U;

enum class EquipmentSlot : std::uint8_t {
    Head,
    Face,
    Neck,
    TorsoBase,
    Legs,
    Feet,
    Hands,
    TorsoArmor,
    ChestRig,
    Back,
    Belt,
    LeftHip,
    RightHip,
    LeftThigh,
    RightThigh,
    Utility1,
    Utility2,
    Utility3,
    MeleeWeapon,
    Throwable,
    PrimaryWeapon,
    SecondaryWeapon,
};

enum class EquipmentKind : std::uint8_t {
    Cap,
    Helmet,
    Eyewear,
    Mask,
    Neckwear,
    Clothing,
    Armor,
    Rig,
    Pack,
    Belt,
    Pouch,
    Weapon,
};

struct EquipmentSlotDefinition final {
    EquipmentSlot slot{};
    std::string_view identifier{};
    std::string_view required_item{};
};

struct EquipmentItemDefinition final {
    foundation::StableId id{0};
    std::string_view identifier{};
    EquipmentKind kind{EquipmentKind::Clothing};
    std::array<EquipmentSlot, 7U> allowed_slots{};
    std::uint8_t allowed_slot_count{0};
    float weight_kg{0.0F};
    float fit_scale{1.0F};
    float fit_thickness{0.0F};
    std::string_view style{};

    [[nodiscard]] bool allows(EquipmentSlot slot) const noexcept;
};

struct EquipmentVariant final {
    float size{1.0F};
    float shade{1.0F};
    float detail{0.0F};
};

struct EquipmentItem final {
    foundation::StableId definition_id{0};
    EquipmentSlot slot{};
    proc::Seed seed{0};
    EquipmentVariant variant{};
    float weight_kg{0.0F};
};

struct EquipmentOverride final {
    bool specified{false};
    bool empty{false};
    foundation::StableId definition_id{0};

    [[nodiscard]] static constexpr EquipmentOverride absent() noexcept { return {}; }
    [[nodiscard]] static constexpr EquipmentOverride nullValue() noexcept {
        return {true, true, 0};
    }
    [[nodiscard]] static constexpr EquipmentOverride item(foundation::StableId id) noexcept {
        return {true, false, id};
    }
};

struct EquipmentOverrideSet final {
    std::array<EquipmentOverride, kEquipmentSlotCount> slots{};
};

struct EquipmentState final {
    std::uint32_t version{1};
    proc::Seed unit_seed{0};
    proc::Seed equipment_seed{0};
    foundation::StableId loadout_id{0};
    std::array<std::optional<EquipmentItem>, kEquipmentSlotCount> slots{};
    foundation::StableId identity{0};
    float total_weight_kg{0.0F};
    std::uint64_t revision{0};

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::size_t count() const noexcept;
    [[nodiscard]] const EquipmentItem* item(EquipmentSlot slot) const noexcept;
};

class EquipmentResolver final {
public:
    [[nodiscard]] static foundation::Result<EquipmentState, foundation::Error> resolve(
        proc::Seed unit_seed,
        foundation::StableId loadout_id,
        const EquipmentOverrideSet& overrides = {});
};

[[nodiscard]] constexpr std::size_t equipmentSlotIndex(EquipmentSlot slot) noexcept {
    return static_cast<std::size_t>(slot);
}

} // namespace genomes::infantry
