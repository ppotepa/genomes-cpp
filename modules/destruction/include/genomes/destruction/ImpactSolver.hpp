#pragma once

#include <genomes/destruction/ImpactResult.hpp>
#include <genomes/destruction/MaterialAssembly.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>

#include <cstdint>

namespace genomes::destruction {

enum class ImpactStrategy : std::uint8_t {
    Ball,
    ArmorPiercing,
    HighExplosive,
    Fragment,
};

struct ImpactOrientation final {
    foundation::Vec3 right{1.0F, 0.0F, 0.0F};
    foundation::Vec3 up{0.0F, 1.0F, 0.0F};
    foundation::Vec3 forward{0.0F, 0.0F, 1.0F};

    [[nodiscard]] static constexpr ImpactOrientation identity() noexcept { return {}; }
    [[nodiscard]] bool valid(float tolerance = 1.0e-4F) const noexcept;
    [[nodiscard]] foundation::Vec3 toLocal(foundation::Vec3 world) const noexcept;
    [[nodiscard]] foundation::Vec3 toWorld(foundation::Vec3 local) const noexcept;
};

struct ImpactLimits final {
    std::uint32_t max_intervals{64};
    std::uint32_t max_interfaces{64};
    float stop_energy{1.0e-4F};
};

struct ImpactInput final {
    float mass_kg{0.0F};
    float diameter_m{0.0F};
    foundation::Vec3 projectile_velocity{};
    foundation::Vec3 projectile_angular_velocity{};
    ImpactOrientation projectile_orientation{};
    foundation::Vec3 projectile_center{};
    foundation::Vec3 contact_point{};
    foundation::Vec3 contact_normal{0.0F, 1.0F, 0.0F};
    foundation::Vec3 target_linear_velocity{};
    foundation::Vec3 target_angular_velocity{};
    foundation::Vec3 target_center_of_mass{};
    const MaterialAssembly* assembly{nullptr};
    foundation::Vec3 entry_point{};
    foundation::Vec3 exit_point{};
    ImpactStrategy strategy{ImpactStrategy::Ball};
    foundation::StableId strategy_id{foundation::stable_id("impact.strategy.default")};
    ImpactLimits limits{};
};

class ImpactSolver final {
public:
    [[nodiscard]] static foundation::Result<ImpactResult, foundation::Error> solve(
        const ImpactInput&, const MaterialCatalog&) noexcept;
};

} // namespace genomes::destruction
