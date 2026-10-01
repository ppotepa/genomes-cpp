#pragma once

#include <genomes/content/ContentSnapshot.hpp>
#include <genomes/foundation/ConfigHash.hpp>
#include <genomes/infantry/Equipment.hpp>

#include <filesystem>
#include <span>
#include <string>
#include <vector>

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
    std::string identifier{};
    std::array<LoadoutChoice, kEquipmentSlotCount> choices{};
};

[[nodiscard]] std::span<const InfantryLoadout> infantryLoadouts() noexcept;
[[nodiscard]] const InfantryLoadout* findInfantryLoadout(foundation::StableId id) noexcept;

// The fixture-backed catalog owns the resolved text and provenance snapshot.
// Native definitions remain authoritative until a value-bearing schema is
// introduced; loading still validates the complete exported catalog against
// those definitions before publishing a frozen view.
class FrozenEquipmentCatalog final {
public:
    [[nodiscard]] std::span<const EquipmentSlotDefinition> slots() const noexcept {
        return {slots_.data(), slots_.size()};
    }
    [[nodiscard]] std::span<const EquipmentItemDefinition> items() const noexcept {
        return {items_.data(), items_.size()};
    }
    [[nodiscard]] std::span<const InfantryLoadout> loadouts() const noexcept {
        return {loadouts_.data(), loadouts_.size()};
    }
    [[nodiscard]] const EquipmentItemDefinition* findItem(
        foundation::StableId id) const noexcept;
    [[nodiscard]] const EquipmentItemDefinition* findItem(
        std::string_view identifier) const noexcept {
        return findItem(foundation::stable_id(identifier));
    }
    [[nodiscard]] std::size_t size() const noexcept { return items_.size(); }
    [[nodiscard]] bool frozen() const noexcept { return frozen_; }
    [[nodiscard]] std::string_view sourceCommit() const noexcept { return source_commit_; }
    [[nodiscard]] const content::FrozenContentSnapshot& contentSnapshot() const noexcept {
        return snapshot_;
    }
    [[nodiscard]] foundation::SimConfigHash fingerprint() const noexcept { return fingerprint_; }

private:
    friend foundation::Result<FrozenEquipmentCatalog, foundation::Error>
    loadEquipmentCatalog(const std::filesystem::path& path);

    std::vector<EquipmentSlotDefinition> slots_;
    std::vector<EquipmentItemDefinition> items_;
    std::vector<InfantryLoadout> loadouts_;
    std::string source_commit_;
    content::FrozenContentSnapshot snapshot_{};
    foundation::SimConfigHash fingerprint_{};
    bool frozen_{false};
};

[[nodiscard]] foundation::Result<FrozenEquipmentCatalog, foundation::Error>
loadEquipmentCatalog(const std::filesystem::path& path);

} // namespace genomes::infantry
