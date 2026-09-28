#include <genomes/ballistics/Ballistics.hpp>

#include <algorithm>
#include <cmath>

namespace genomes::ballistics {

namespace {

[[nodiscard]] foundation::Vec3 subtract(foundation::Vec3 left,
                                        foundation::Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] foundation::Vec3 add(foundation::Vec3 left,
                                   foundation::Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] foundation::Vec3 multiply(foundation::Vec3 value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] float length(foundation::Vec3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

} // namespace

bool Segment::valid() const noexcept {
    return std::isfinite(origin.x) && std::isfinite(origin.y) && std::isfinite(origin.z) &&
           std::isfinite(end.x) && std::isfinite(end.y) && std::isfinite(end.z);
}

bool Projectile::valid() const noexcept {
    return std::isfinite(position.x) && std::isfinite(position.y) &&
           std::isfinite(position.z) && std::isfinite(velocity.x) &&
           std::isfinite(velocity.y) && std::isfinite(velocity.z) &&
           std::isfinite(gravity.x) && std::isfinite(gravity.y) &&
           std::isfinite(gravity.z) && std::isfinite(remaining_seconds) &&
           remaining_seconds > 0.0F;
}

foundation::Result<TraceHit, foundation::Error> BallisticsSolver::trace(
    const physics::PhysicsWorld& physics, const Segment& segment, std::uint32_t collision_mask) {
    if (!segment.valid()) {
        return foundation::Result<TraceHit, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid ballistic segment"});
    }
    const foundation::Vec3 delta = subtract(segment.end, segment.origin);
    const float distance = length(delta);
    if (!std::isfinite(distance) || distance <= 0.000001F) {
        return foundation::Result<TraceHit, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "ballistic segment has no length"});
    }
    physics::RaycastHit physics_hit{};
    const physics::RaycastQuery query{segment.origin, delta, distance, collision_mask};
    if (!physics.raycast(query, physics_hit)) {
        return foundation::Result<TraceHit, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "ballistic segment did not hit a body"});
    }
    return foundation::Result<TraceHit, foundation::Error>::success(
        {physics_hit.body, physics_hit.point, physics_hit.normal, physics_hit.distance,
         physics_hit.ground});
}

foundation::Result<Projectile, foundation::Error> BallisticsSolver::advance(
    const Projectile& projectile, float dt, const physics::PhysicsWorld& physics,
    TraceHit* hit) {
    if (!projectile.valid() || !std::isfinite(dt) || dt <= 0.0F) {
        return foundation::Result<Projectile, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid ballistic projectile step"});
    }
    Projectile next = projectile;
    const float step = std::min(dt, projectile.remaining_seconds);
    next.velocity = add(next.velocity, multiply(projectile.gravity, step));
    const foundation::Vec3 next_position =
        add(projectile.position, multiply(projectile.velocity, step));
    const auto trace_result = trace(physics, {projectile.position, next_position},
                                    projectile.collision_mask);
    if (trace_result) {
        if (hit != nullptr) {
            *hit = trace_result.value();
        }
        next.position = trace_result.value().point;
        next.remaining_seconds = 0.0F;
        return foundation::Result<Projectile, foundation::Error>::success(next);
    }
    next.position = next_position;
    next.remaining_seconds -= step;
    return foundation::Result<Projectile, foundation::Error>::success(next);
}

} // namespace genomes::ballistics
