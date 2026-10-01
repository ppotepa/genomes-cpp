#pragma once

#include <genomes/infantry/Equipment.hpp>

#include <span>

namespace genomes::infantry {

class EquipmentCatalog final {
public:
    [[nodiscard]] static foundation::Result<void, foundation::Error> validate();
    [[nodiscard]] static std::span<const EquipmentSlotDefinition> slots() noexcept;
    [[nodiscard]] static std::span<const EquipmentItemDefinition> items() noexcept;
    [[nodiscard]] static const EquipmentSlotDefinition* findSlot(
        foundation::StableId id) noexcept;
    [[nodiscard]] static const EquipmentItemDefinition* findItem(
        foundation::StableId id) noexcept;
    [[nodiscard]] static const EquipmentItemDefinition* findItem(
        std::string_view identifier) noexcept;
    [[nodiscard]] static foundation::StableId slotId(EquipmentSlot slot) noexcept;
    [[nodiscard]] static foundation::StableId loadoutId(std::string_view identifier) noexcept;
};

struct LoadoutChoice final {
    std::array<foundation::StableId, 4U> definitions{};
    std::uint8_t count{0};
};

struct InfantryLoadout final {
    foundation::StableId id{0};
    std::string_view identifier{};
    std::array<LoadoutChoice, kEquipmentSlotCount> choices{};
};

[[nodiscard]] std::span<const InfantryLoadout> infantryLoadouts() noexcept;
[[nodiscard]] const InfantryLoadout* findInfantryLoadout(foundation::StableId id) noexcept;

} // namespace genomes::infantry
