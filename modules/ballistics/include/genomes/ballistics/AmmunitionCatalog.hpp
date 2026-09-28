#pragma once

#include <genomes/ballistics/AmmunitionStrategy.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace genomes::ballistics {

struct AmmunitionTag;
using AmmunitionId = foundation::StrongId<AmmunitionTag>;

[[nodiscard]] inline AmmunitionId ammunition_id(std::string_view name) noexcept {
    return AmmunitionId{foundation::stable_id(name)};
}

[[nodiscard]] inline StrategyId strategy_id(std::string_view name) noexcept {
    return StrategyId{foundation::stable_id(name)};
}

[[nodiscard]] inline CaliberId caliber_id(std::string_view name) noexcept {
    return CaliberId{foundation::stable_id(name)};
}

[[nodiscard]] inline VariantId variant_id(std::string_view name) noexcept {
    return VariantId{foundation::stable_id(name)};
}

struct AmmunitionDefinition final {
    AmmunitionId id{};
    StrategyId strategy_id_value{};
    CaliberId caliber_id_value{};
    VariantId variant_id_value{};
    float mass_kg{0.0F};
    float diameter_m{0.0F};
    float drag_diameter_m{0.0F};
    float muzzle_velocity_mps{0.0F};
    float inertia_factor{0.25F};
    std::uint32_t version{1};
    std::string_view provenance{};
    float explosive_energy_j{0.0F};

    [[nodiscard]] bool valid() const noexcept;
};

class AmmunitionCatalog final {
public:
    [[nodiscard]] foundation::Result<void, foundation::Error> add(
        AmmunitionDefinition definition, AmmunitionStrategy strategy);
    [[nodiscard]] foundation::Result<void, foundation::Error> freeze();

    [[nodiscard]] const AmmunitionDefinition* find(AmmunitionId) const noexcept;
    [[nodiscard]] const AmmunitionStrategy* strategy(StrategyId) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] bool frozen() const noexcept { return frozen_; }

private:
    struct Entry final {
        AmmunitionDefinition definition{};
        AmmunitionStrategy strategy{};
    };

    std::vector<Entry> entries_;
    bool frozen_{false};
};

} // namespace genomes::ballistics
