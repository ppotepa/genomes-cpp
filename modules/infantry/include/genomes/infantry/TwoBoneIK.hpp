#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>

namespace genomes::infantry {

struct TwoBoneIKSolution final {
    foundation::Vec3 joint_position{};
    foundation::Vec3 end_position{};
    foundation::Vec3 bend_normal{0.0F, 0.0F, 1.0F};
    float root_angle{0.0F};
    float joint_angle{0.0F};
    float target_distance{0.0F};
    float clamped_distance{0.0F};
    float residual_error{0.0F};
    bool reachable{false};
};

class TwoBoneIK final {
public:
    [[nodiscard]] static foundation::Result<TwoBoneIKSolution, foundation::Error> solve(
        foundation::Vec3 root,
        foundation::Vec3 target,
        foundation::Vec3 pole,
        float upper_length,
        float lower_length) noexcept;
};

} // namespace genomes::infantry
