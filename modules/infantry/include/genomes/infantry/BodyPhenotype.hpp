#pragma once

#include <genomes/foundation/Types.hpp>

#include <cstdint>

namespace genomes::infantry {

// Genome dimensions are normalized genes; every field in BodyPhenotype is a
// resolved metre quantity. Keep conversions explicit at the resolver boundary.
[[nodiscard]] constexpr float relativeToHeight(float normalized, float height) noexcept {
    return normalized * height;
}

struct BodyPhenotype final {
    std::uint32_t version{1};
    float height{1.75F};
    float shoulder_width{0.46F};
    float chest_depth{0.24F};
    float hip_width{0.32F};
    float arm_length{0.72F};
    float leg_length{0.92F};
    float waist_width{0.30F};
    float limb_thickness{0.12F};
    float neck_scale{1.0F};
    float head_scale{1.0F};
    float hand_scale{1.0F};
    float foot_scale{1.0F};
    foundation::Color skin_color{0.72F, 0.50F, 0.38F, 1.0F};
    foundation::Vec3 pelvis{};
    foundation::Vec3 chest{};
    foundation::Vec3 head{};
    foundation::Vec3 left_hand{};
    foundation::Vec3 right_hand{};
    foundation::Vec3 left_foot{};
    foundation::Vec3 right_foot{};

    [[nodiscard]] bool valid() const noexcept;
};

} // namespace genomes::infantry
