#pragma once

#include <genomes/ballistics/AmmunitionCatalog.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>

#include <cstdint>
#include <optional>

namespace genomes::ballistics {

inline constexpr std::uint32_t ProjectileStateVersion = 3;

struct ProjectileTag;
struct ShotTag;
struct TraceTag;
using ProjectileId = foundation::StrongId<ProjectileTag>;
using ShotId = foundation::StrongId<ShotTag>;
using TraceId = foundation::StrongId<TraceTag>;

struct Quaternion final {
    float w{1.0F};
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] Quaternion normalized() const noexcept;
};

struct ProjectileEnergyLedger final {
    float material_work{0.0F};
    float contact_loss{0.0F};
    float target_work{0.0F};
    float flight_work{0.0F};
    float fragment_energy{0.0F};
    float unrepresented_energy{0.0F};
    float rotation_work{0.0F};
    float explosive_energy{0.0F};
    float blast_energy{0.0F};

    [[nodiscard]] bool valid() const noexcept;
};

struct FireRequest final {
    ProjectileId projectile_id{};
    ShotId shot_id{};
    TraceId trace_id{};
    std::optional<TraceId> parent_trace_id;
    AmmunitionId ammunition_id{};
    foundation::Vec3 position{};
    foundation::Vec3 direction{0.0F, 0.0F, 1.0F};
    std::uint64_t tick{0};
    std::uint64_t seed{0};
    bool fragment{false};
    std::optional<foundation::Vec3> velocity_override;
    std::optional<float> mass_override_kg;
    std::optional<float> diameter_override_m;
    std::optional<float> drag_diameter_override_m;
};

struct ImpactTransition final {
    foundation::Vec3 velocity_delta{};
    foundation::Vec3 angular_impulse{};
    float integrity_delta{0.0F};
    float deformation_delta{0.0F};
    float stability_delta{0.0F};
    float mass_scale{1.0F};
    float diameter_scale{1.0F};
    float drag_diameter_scale{1.0F};
    float material_work{0.0F};
    float contact_loss{0.0F};
    float target_work{0.0F};
    float fragment_energy{0.0F};
    float unrepresented_energy{0.0F};
    std::uint32_t ricochet_count_delta{0};
};

struct ProjectileState final {
    std::uint32_t version{ProjectileStateVersion};
    ProjectileId projectile_id{};
    ShotId shot_id{};
    TraceId trace_id{};
    std::optional<TraceId> parent_trace_id;
    AmmunitionId ammunition_id{};
    StrategyId strategy_id{};
    std::uint64_t seed{0};
    bool fragment{false};
    foundation::Vec3 position{};
    foundation::Vec3 velocity{};
    Quaternion orientation{};
    foundation::Vec3 body_forward{0.0F, 0.0F, 1.0F};
    foundation::Vec3 angular_velocity{};
    foundation::Vec3 inertia{1.0F, 1.0F, 1.0F};
    foundation::Vec3 inverse_inertia{1.0F, 1.0F, 1.0F};
    float mass_kg{0.0F};
    float diameter_m{0.0F};
    float drag_diameter_m{0.0F};
    float integrity{1.0F};
    float deformation{0.0F};
    float stability{1.0F};
    std::uint32_t impact_index{0};
    std::uint32_t ricochet_count{0};
    std::uint64_t age_ticks{0};
    float age_seconds{0.0F};
    float travel_distance_m{0.0F};
    ProjectileEnergyLedger energy{};

    [[nodiscard]] static foundation::Result<ProjectileState, foundation::Error> create(
        const FireRequest&, const AmmunitionCatalog&);
    [[nodiscard]] foundation::Result<void, foundation::Error> apply(
        const ImpactTransition&);
    [[nodiscard]] foundation::Result<void, foundation::Error> advanceAge(
        std::uint64_t ticks, float seconds, float distance_m) noexcept;
    [[nodiscard]] foundation::Result<void, foundation::Error> setVelocity(
        foundation::Vec3 velocity) noexcept;
    [[nodiscard]] float rotationalEnergy() const noexcept;
    [[nodiscard]] float translationalEnergy() const noexcept;
    [[nodiscard]] float energyBalanceError(float initial_translational,
                                           float initial_rotational) const noexcept;
    void refreshInertia() noexcept;
    [[nodiscard]] bool valid() const noexcept;
};

} // namespace genomes::ballistics
