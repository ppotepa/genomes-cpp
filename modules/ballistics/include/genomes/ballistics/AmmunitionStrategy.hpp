#pragma once

#include <genomes/ballistics/Fragmentation.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/StrongId.hpp>
#include <genomes/foundation/Types.hpp>

#include <cstdint>
#include <vector>

namespace genomes::ballistics {

struct AmmunitionStrategyTag;
struct CaliberTag;
struct VariantTag;

using StrategyId = foundation::StrongId<AmmunitionStrategyTag>;
using CaliberId = foundation::StrongId<CaliberTag>;
using VariantId = foundation::StrongId<VariantTag>;

enum class ConstructionKind : std::uint8_t {
    FullMetalJacket,
    SoftPoint,
    ArmorPiercing,
    HighExplosive,
    Fragmentation,
};

struct BreakupPiece final {
    foundation::StableId child_id{0};
    float mass_fraction{0.0F};
    float energy_fraction{0.0F};
    foundation::Vec3 direction_offset{};
};

struct BreakupPlan final {
    bool detonated{false};
    std::vector<BreakupPiece> pieces;
};

struct AmmunitionStrategy final {
    StrategyId id{};
    CaliberId caliber_id{};
    VariantId variant_id{};
    ConstructionKind construction{ConstructionKind::FullMetalJacket};
    float drag_coefficient{0.3F};
    float contact_work_scale{1.0F};
    float ricochet_threshold{0.35F};
    float breakup_energy_threshold{0.0F};
    float fragment_mass_fraction{0.0F};
    std::uint32_t max_fragments{0};
    bool explosive{false};
    std::uint32_t version{1};
    FuzeMode fuze{FuzeMode::None};
    float nose_crush_work_j{0.0F};
    FragmentationProfile fragmentation{};

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] BreakupPlan breakupPlan(foundation::StableId projectile_id,
                                           std::uint32_t impact_index,
                                           float available_energy) const;
};

} // namespace genomes::ballistics
