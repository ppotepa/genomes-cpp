#include <genomes/infantry/TwoBoneIK.hpp>

#include <algorithm>
#include <cmath>

namespace genomes::infantry {

namespace {

using foundation::Vec3;

[[nodiscard]] Vec3 add(Vec3 a, Vec3 b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
[[nodiscard]] Vec3 sub(Vec3 a, Vec3 b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
[[nodiscard]] Vec3 mul(Vec3 a, float s) noexcept { return {a.x * s, a.y * s, a.z * s}; }
[[nodiscard]] float dot(Vec3 a, Vec3 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
[[nodiscard]] Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
[[nodiscard]] float length(Vec3 a) noexcept { return std::sqrt(dot(a, a)); }
[[nodiscard]] Vec3 normalize(Vec3 a, Vec3 fallback) noexcept {
    const float value = length(a);
    return value > 1.0e-6F && std::isfinite(value) ? mul(a, 1.0F / value) : fallback;
}
[[nodiscard]] bool finite(Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

} // namespace

foundation::Result<TwoBoneIKSolution, foundation::Error> TwoBoneIK::solve(
    Vec3 root, Vec3 target, Vec3 pole, float upper_length, float lower_length) noexcept {
    if (!finite(root) || !finite(target) || !finite(pole) || !std::isfinite(upper_length) ||
        !std::isfinite(lower_length) || upper_length <= 1.0e-5F || lower_length <= 1.0e-5F) {
        return foundation::Result<TwoBoneIKSolution, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid two-bone IK input"});
    }
    const Vec3 to_target = sub(target, root);
    const float raw_distance = length(to_target);
    if (!(raw_distance > 1.0e-6F) || !std::isfinite(raw_distance)) {
        return foundation::Result<TwoBoneIKSolution, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "degenerate two-bone IK target"});
    }
    const Vec3 direction = normalize(to_target, {0.0F, 1.0F, 0.0F});
    Vec3 plane = cross(direction, sub(pole, root));
    if (length(plane) <= 1.0e-6F) {
        plane = cross(direction, {0.0F, 1.0F, 0.0F});
        if (length(plane) <= 1.0e-6F) {
            plane = cross(direction, {1.0F, 0.0F, 0.0F});
        }
    }
    const Vec3 bend_normal = normalize(plane, {0.0F, 0.0F, 1.0F});
    const Vec3 bend_direction = normalize(cross(bend_normal, direction), {0.0F, 1.0F, 0.0F});
    constexpr float epsilon = 1.0e-5F;
    const float minimum_distance = std::abs(upper_length - lower_length) + epsilon;
    const float maximum_distance = upper_length + lower_length - epsilon;
    const float clamped_distance = std::clamp(raw_distance, minimum_distance, maximum_distance);
    const float cos_root = std::clamp((upper_length * upper_length + clamped_distance * clamped_distance -
                                       lower_length * lower_length) /
                                          (2.0F * upper_length * clamped_distance),
                                      -1.0F, 1.0F);
    const float cos_joint = std::clamp((upper_length * upper_length + lower_length * lower_length -
                                        clamped_distance * clamped_distance) /
                                           (2.0F * upper_length * lower_length),
                                       -1.0F, 1.0F);
    const float root_height = upper_length * std::sqrt(std::max(0.0F, 1.0F - cos_root * cos_root));
    const Vec3 joint = add(root, add(mul(direction, upper_length * cos_root),
                                     mul(bend_direction, root_height)));
    TwoBoneIKSolution solution{};
    solution.joint_position = joint;
    solution.end_position = add(root, mul(direction, clamped_distance));
    solution.bend_normal = bend_normal;
    solution.root_angle = std::acos(cos_root);
    solution.joint_angle = std::acos(cos_joint);
    solution.target_distance = raw_distance;
    solution.clamped_distance = clamped_distance;
    solution.residual_error = std::abs(raw_distance - clamped_distance);
    solution.reachable = raw_distance >= minimum_distance && raw_distance <= maximum_distance;
    return foundation::Result<TwoBoneIKSolution, foundation::Error>::success(solution);
}

} // namespace genomes::infantry
