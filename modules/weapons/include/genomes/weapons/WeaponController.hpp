#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Time.hpp>
#include <genomes/foundation/Types.hpp>

#include <cstdint>

namespace genomes::weapons {

struct WeaponSpec final {
    float rounds_per_second{8.0F};
    float muzzle_velocity{700.0F};
    float damage{12.0F};
    float range{300.0F};
    std::uint32_t magazine_size{30};

    [[nodiscard]] bool valid() const noexcept;
};

struct WeaponState final {
    std::uint32_t ammunition{30};
    foundation::SimulationTick next_fire{};
};

struct ShotRequest final {
    foundation::StableId shooter{0};
    foundation::Vec3 origin{};
    foundation::Vec3 direction{0.0F, 0.0F, 1.0F};
    float range{0.0F};
    float damage{0.0F};
    foundation::SimulationTick tick{};
};

class WeaponController final {
public:
    [[nodiscard]] static foundation::Result<ShotRequest, foundation::Error> tryFire(
        foundation::StableId shooter,
        foundation::Vec3 origin,
        foundation::Vec3 direction,
        const WeaponSpec&,
        WeaponState&,
        foundation::SimulationTick tick);
};

} // namespace genomes::weapons
