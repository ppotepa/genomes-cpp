#include <genomes/weapons/WeaponController.hpp>

#include <algorithm>
#include <cmath>

namespace genomes::weapons {

namespace {

[[nodiscard]] float length_squared(foundation::Vec3 value) noexcept {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

[[nodiscard]] foundation::Vec3 normalized(foundation::Vec3 value) noexcept {
    const float length = std::sqrt(length_squared(value));
    return {value.x / length, value.y / length, value.z / length};
}

} // namespace

bool WeaponSpec::valid() const noexcept {
    return std::isfinite(rounds_per_second) && rounds_per_second > 0.0F &&
           std::isfinite(muzzle_velocity) && muzzle_velocity > 0.0F &&
           std::isfinite(damage) && damage > 0.0F && std::isfinite(range) && range > 0.0F &&
           magazine_size > 0;
}

foundation::Result<ShotRequest, foundation::Error> WeaponController::tryFire(
    foundation::StableId shooter, foundation::Vec3 origin, foundation::Vec3 direction,
    const WeaponSpec& spec, WeaponState& state, foundation::SimulationTick tick) {
    if (shooter == 0 || !spec.valid() || !std::isfinite(origin.x) ||
        !std::isfinite(origin.y) || !std::isfinite(origin.z) ||
        !std::isfinite(direction.x) || !std::isfinite(direction.y) ||
        !std::isfinite(direction.z) || length_squared(direction) <= 0.000001F) {
        return foundation::Result<ShotRequest, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid weapon fire request"});
    }
    if (state.ammunition == 0) {
        return foundation::Result<ShotRequest, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "weapon magazine is empty"});
    }
    if (tick < state.next_fire) {
        return foundation::Result<ShotRequest, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "weapon fire cadence has not elapsed"});
    }

    const std::uint64_t cadence_ticks = std::max<std::uint64_t>(
        1, static_cast<std::uint64_t>(std::ceil(60.0 / spec.rounds_per_second)));
    state.next_fire = foundation::SimulationTick{tick.value + cadence_ticks};
    --state.ammunition;
    return foundation::Result<ShotRequest, foundation::Error>::success(
        {shooter, origin, normalized(direction), spec.range, spec.damage, tick});
}

} // namespace genomes::weapons
