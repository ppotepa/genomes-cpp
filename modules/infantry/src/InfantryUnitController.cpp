#include <genomes/infantry/InfantryUnitController.hpp>

#include <algorithm>
#include <cmath>

namespace genomes::infantry {
namespace {

[[nodiscard]] float wrapAngle(float angle) noexcept {
    constexpr float tau = 6.28318530717958647692F;
    angle = std::fmod(angle + tau * 0.5F, tau);
    if (angle < 0.0F) angle += tau;
    return angle - tau * 0.5F;
}

[[nodiscard]] float approach(float value, float target, float delta) noexcept {
    if (value < target) return std::min(value + delta, target);
    return std::max(value - delta, target);
}

[[nodiscard]] bool finite(const foundation::Vec3& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

} // namespace

foundation::StableId InfantryUnitController::entityType() const noexcept {
    return foundation::stable_id("entity-type.infantry");
}

foundation::Result<void, foundation::Error> InfantryUnitController::updateBatch(
    std::span<const simulation::EntityControlRequest> requests,
    std::span<simulation::EntityControlCommand> commands,
    const simulation::TickContext& context) const noexcept {
    if (requests.size() != commands.size() ||
        !std::isfinite(context.fixed_dt_seconds) || context.fixed_dt_seconds <= 0.0 ||
        context.fixed_dt_seconds > 0.25) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument,
             "invalid infantry entity controller batch"});
    }
    const float dt = static_cast<float>(context.fixed_dt_seconds);
    for (std::size_t index = 0U; index < requests.size(); ++index) {
        const auto& request = requests[index];
        const auto& state = request.state;
        if (!request.enabled) {
            commands[index] = {};
            continue;
        }
        if (!state.id.isValid() || !finite(state.position) || !finite(state.velocity) ||
            !std::isfinite(state.heading) || !finite(request.order.destination) ||
            !std::isfinite(request.requested_speed_mps) || request.requested_speed_mps < 0.0F ||
            !std::isfinite(request.maximum_speed_mps) || request.maximum_speed_mps < 0.0F ||
            !std::isfinite(request.acceleration_mps2) || request.acceleration_mps2 < 0.0F ||
            !std::isfinite(request.turn_rate_radians_per_second) ||
            request.turn_rate_radians_per_second < 0.0F) {
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument,
                 "invalid infantry entity controller request"});
        }

        const bool order_active = request.order.activeAt(state.id, context.tick.value);
        const bool can_move = order_active &&
            (request.order.kind == simulation::EntityOrderKind::MoveTo ||
             request.order.kind == simulation::EntityOrderKind::Retreat);
        const foundation::Vec3 destination = order_active
            ? request.order.destination : state.position;
        const float dx = destination.x - state.position.x;
        const float dz = destination.z - state.position.z;
        const float distance = std::sqrt(dx * dx + dz * dz);
        const float desired_heading = distance > 1.0e-4F
            ? std::atan2(dx, dz) : state.heading;
        const float heading_delta = wrapAngle(desired_heading - state.heading);
        const float max_turn = request.turn_rate_radians_per_second * dt;
        const float heading = wrapAngle(
            state.heading + std::clamp(heading_delta, -max_turn, max_turn));

        const float maximum_speed = std::max(request.maximum_speed_mps, 0.0F);
        float desired_speed = can_move
            ? std::clamp(request.requested_speed_mps, 0.0F, maximum_speed) : 0.0F;
        if (distance <= 0.02F) desired_speed = 0.0F;
        else if (can_move) desired_speed = std::min(desired_speed, distance / dt);
        const float current_speed = std::sqrt(
            state.velocity.x * state.velocity.x + state.velocity.z * state.velocity.z);
        const float speed = approach(current_speed, desired_speed,
                                     request.acceleration_mps2 * dt);

        auto& command = commands[index];
        command.entity = state.id;
        command.heading_radians = heading;
        command.velocity = {std::sin(heading) * speed, 0.0F,
                            std::cos(heading) * speed};
        command.position = {state.position.x + command.velocity.x * dt,
                            state.position.y,
                            state.position.z + command.velocity.z * dt};
        command.action = order_active ? request.order.action : 0U;
    }
    return foundation::Result<void, foundation::Error>::success();
}

} // namespace genomes::infantry
