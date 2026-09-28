#pragma once

#include <genomes/ballistics/Fragmentation.hpp>
#include <genomes/ballistics/ProjectileState.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <cstdint>
#include <vector>

namespace genomes::ballistics {

inline constexpr std::uint32_t DetonationEventVersion = 1;

struct BlastCommand final {
    foundation::StableId command_id{0};
    foundation::StableId source_projectile_id{0};
    foundation::Vec3 origin{};
    float radius_m{0.0F};
    float energy_j{0.0F};
    std::uint64_t seed{0};

    [[nodiscard]] bool valid() const noexcept;
};

struct DetonationInput final {
    foundation::StableId contact_id{0};
    foundation::Vec3 point{};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
    bool refined_surface{false};
    std::uint32_t available_fragment_capacity{0};
};

struct DetonationEvent final {
    std::uint32_t version{DetonationEventVersion};
    ProjectileId projectile_id{};
    ShotId shot_id{};
    TraceId parent_trace_id{};
    AmmunitionId ammunition_id{};
    foundation::Vec3 origin{};
    foundation::Vec3 normal{0.0F, 1.0F, 0.0F};
    std::uint64_t seed{0};
    BlastCommand blast{};
    FragmentationLedger ledger{};
    std::vector<FragmentSpawn> fragments;

    [[nodiscard]] bool valid() const noexcept;
};

class Detonation final {
public:
    [[nodiscard]] static foundation::Result<DetonationEvent, foundation::Error> detonate(
        const ProjectileState&,
        const AmmunitionDefinition&,
        const AmmunitionStrategy&,
        const DetonationInput&);

    [[nodiscard]] static FireRequest makeFireRequest(const DetonationEvent&,
                                                     const FragmentSpawn&,
                                                     std::uint64_t tick) noexcept;
};

} // namespace genomes::ballistics
