#pragma once

#include <genomes/ballistics/AmmunitionStrategy.hpp>
#include <genomes/ballistics/ProjectileState.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>

#include <cstdint>

namespace genomes::ballistics {

struct FlightEnvironment final {
    foundation::Vec3 wind{};
    foundation::Vec3 gravity{0.0F, -9.81F, 0.0F};
    float density{1.225F};
    float sound_speed{343.0F};
    float drag_scale{1.0F};

    [[nodiscard]] bool valid() const noexcept;
};

struct FlightIntervalInput final {
    const ProjectileState* projectile{nullptr};
    const AmmunitionStrategy* strategy{nullptr};
    const FlightEnvironment* environment{nullptr};
    float remaining_seconds{0.0F};
    float projectile_diameter_m{0.0F};
    bool fragment{false};
};

struct FlightStep final {
    foundation::Vec3 position{};
    foundation::Vec3 velocity{};
    float seconds{0.0F};
    float travel_distance_m{0.0F};
    float flight_work{0.0F};
};

class FlightIntegrator final {
public:
    [[nodiscard]] static foundation::Result<float, foundation::Error> chooseInterval(
        const FlightIntervalInput&);

    [[nodiscard]] static foundation::Result<FlightStep, foundation::Error> integrate(
        const FlightIntervalInput&, float seconds);
};

} // namespace genomes::ballistics
