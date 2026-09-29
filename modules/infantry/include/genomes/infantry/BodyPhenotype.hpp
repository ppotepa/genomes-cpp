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
    float height{1.75};
    double reference_height{1.75};
    float speed_multiplier{1.0F};
    float walk_speed{1.4F};
    float run_speed{4.2F};
    float prone_speed{0.55F};
    float crouch_speed{0.75F};
    float frame{1.0};
    float mass{1.0};
    float muscle{1.0};
    float fat{1.0};
    float torso_leg_bias{0.0};
    float shoulder_width_scale{1.0};
    float hip_width_scale{1.0};
    float chest_width_scale{1.0};
    float chest_depth_scale{1.0};
    float waist_width_scale{1.0};
    float waist_depth_scale{1.0};
    float arm_thickness_scale{1.0};
    float leg_thickness_scale{1.0};
    float leg_length_scale{1.0};
    float arm_length_scale{1.0};
    float hip_y{0.54};
    // Binary64 authoring value retained for the JS-parity surface path.
    // Runtime/GPU consumers continue to use hip_y at the artifact boundary.
    double reference_hip_y{0.54};
    double reference_shoulder_width_scale{1.0};
    double reference_chest_width_scale{1.0};
    double reference_chest_depth_scale{1.0};
    double reference_waist_width_scale{1.0};
    double reference_waist_depth_scale{1.0};
    double reference_head_scale{1.0};
    std::uint32_t skin_color_hex{0};
    float shoulder_width{0.46};
    float chest_depth{0.24};
    float hip_width{0.32};
    float arm_length{0.72};
    float leg_length{0.92};
    float anatomy_leg_length{0.84F};
    float waist_width{0.30};
    float limb_thickness{0.12};
    float neck_scale{1.0};
    float head_scale{1.0};
    float hand_scale{1.0};
    float foot_scale{1.0};
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
