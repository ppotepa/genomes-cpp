#include <genomes/ballistics/FlightIntegrator.hpp>

#include <algorithm>
#include <cmath>

namespace genomes::ballistics {

namespace {

[[nodiscard]] bool finite(foundation::Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] foundation::Vec3 add(foundation::Vec3 left,
                                   foundation::Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] foundation::Vec3 multiply(foundation::Vec3 value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] foundation::Vec3 subtract(foundation::Vec3 left,
                                        foundation::Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] float dot(foundation::Vec3 left, foundation::Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] float length(foundation::Vec3 value) noexcept {
    return std::sqrt(std::max(0.0F, dot(value, value)));
}

[[nodiscard]] foundation::Vec3 acceleration(foundation::Vec3 velocity,
                                            const AmmunitionStrategy& strategy,
                                            const FlightEnvironment& environment,
                                            float mass_kg,
                                            float diameter_m) noexcept {
    const foundation::Vec3 relative = subtract(velocity, environment.wind);
    const float speed = length(relative);
    const float area = 3.14159265358979323846F * diameter_m * diameter_m * 0.25F;
    const float drag = 0.5F * environment.density * strategy.drag_coefficient *
                       environment.drag_scale * area * speed / std::max(1.0e-6F, mass_kg);
    return subtract(environment.gravity, multiply(relative, drag));
}

} // namespace

bool FlightEnvironment::valid() const noexcept {
    return finite(wind) && finite(gravity) && std::isfinite(density) && density >= 0.0F &&
           std::isfinite(sound_speed) && sound_speed > 0.0F && std::isfinite(drag_scale) &&
           drag_scale >= 0.0F;
}

foundation::Result<float, foundation::Error> FlightIntegrator::chooseInterval(
    const FlightIntervalInput& input) {
    if (input.projectile == nullptr || input.strategy == nullptr || input.environment == nullptr ||
        !input.environment->valid() || !input.projectile->valid() ||
        !input.strategy->valid() || !std::isfinite(input.remaining_seconds) ||
        input.remaining_seconds <= 0.0F || !std::isfinite(input.projectile_diameter_m) ||
        input.projectile_diameter_m <= 0.0F) {
        return foundation::Result<float, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid flight interval input"});
    }
    const float speed = std::max(1.0F, length(input.projectile->velocity));
    const foundation::Vec3 direction = multiply(input.projectile->velocity, 1.0F / speed);
    const foundation::Vec3 current_acceleration = acceleration(
        input.projectile->velocity, *input.strategy, *input.environment,
        input.projectile->mass_kg, input.projectile_diameter_m);
    const foundation::Vec3 lateral = subtract(
        current_acceleration, multiply(direction, dot(current_acceleration, direction)));
    const float curve = length(lateral);
    const float sagitta = std::clamp(input.projectile_diameter_m * 0.2F, 0.00035F, 0.003F);
    const float curve_dt = curve > 1.0e-8F ? std::sqrt(8.0F * sagitta / curve)
                                          : input.remaining_seconds;
    const float max_distance = input.fragment ? 6.0F : 12.0F;
    const float interval = std::max(1.0e-7F,
                                    std::min({input.remaining_seconds,
                                              max_distance / speed,
                                              curve_dt}));
    return foundation::Result<float, foundation::Error>::success(interval);
}

foundation::Result<FlightStep, foundation::Error> FlightIntegrator::integrate(
    const FlightIntervalInput& input, float seconds) {
    if (input.projectile == nullptr || input.strategy == nullptr || input.environment == nullptr ||
        !input.environment->valid() || !input.projectile->valid() || !input.strategy->valid() ||
        !std::isfinite(seconds) || seconds <= 0.0F) {
        return foundation::Result<FlightStep, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid flight integration input"});
    }
    const ProjectileState& projectile = *input.projectile;
    const AmmunitionStrategy& strategy = *input.strategy;
    const FlightEnvironment& environment = *input.environment;
    const auto acceleration_for = [&](foundation::Vec3 velocity) {
        return acceleration(velocity, strategy, environment, projectile.mass_kg,
                            projectile.diameter_m);
    };
    const foundation::Vec3 v1 = projectile.velocity;
    const foundation::Vec3 a1 = acceleration_for(v1);
    const foundation::Vec3 v2 = add(v1, multiply(a1, seconds * 0.5F));
    const foundation::Vec3 a2 = acceleration_for(v2);
    const foundation::Vec3 v3 = add(v1, multiply(a2, seconds * 0.5F));
    const foundation::Vec3 a3 = acceleration_for(v3);
    const foundation::Vec3 v4 = add(v1, multiply(a3, seconds));
    const foundation::Vec3 a4 = acceleration_for(v4);
    const foundation::Vec3 next_velocity = add(
        v1, multiply(add(add(a1, multiply(add(a2, a3), 2.0F)), a4), seconds / 6.0F));
    const foundation::Vec3 next_position = add(
        projectile.position,
        multiply(add(add(add(v1, multiply(v2, 2.0F)), multiply(v3, 2.0F)), v4),
                 seconds / 6.0F));
    if (!finite(next_position) || !finite(next_velocity)) {
        return foundation::Result<FlightStep, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "flight integration became nonfinite"});
    }
    const float initial_energy = projectile.translationalEnergy();
    const float final_energy = 0.5F * projectile.mass_kg * dot(next_velocity, next_velocity);
    const foundation::Vec3 displacement = {next_position.x - projectile.position.x,
                                            next_position.y - projectile.position.y,
                                            next_position.z - projectile.position.z};
    return foundation::Result<FlightStep, foundation::Error>::success(
        {next_position,
         next_velocity,
         seconds,
         length(displacement),
         initial_energy - final_energy});
}

} // namespace genomes::ballistics
