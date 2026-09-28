#pragma once

#include <genomes/foundation/Types.hpp>

#include <cstdint>

namespace genomes::infantry {

struct FaceSection final {
    float radius_x{0.0F};
    float radius_z{0.0F};
    float center_z{0.0F};
};

struct FacePhenotype final {
    std::uint32_t version{1};
    float eye_spacing{0.06F};
    float eye_radius{0.015F};
    float brow_y{1.60F};
    float eye_y{1.56F};
    float nose_y{1.47F};
    float nose_length{0.04F};
    float nose_width{0.025F};
    float mouth_y{1.38F};
    float mouth_width{0.055F};
    float upper_lip{0.002F};
    float lower_lip{0.002F};
    float jaw_width{0.14F};
    float hairline_y{1.68F};
    float head_width{0.18F};
    float head_depth{0.16F};
    float head_length_scale{1.0F};
    float forehead_width_scale{1.0F};
    float forehead_slope{0.0F};
    float temple_width_scale{1.0F};
    float brow_ridge{0.0F};
    float jaw_length_scale{1.0F};
    float jaw_angle{1.0F};
    float chin_width_scale{1.0F};
    float chin_height{0.0F};
    float chin_projection{0.0F};
    float cheekbone_scale{1.0F};
    float cheekbone_y{0.0F};
    float cheek_fullness{1.0F};
    float midface_projection{0.0F};
    float eye_width_scale{1.0F};
    float eye_height_scale{1.0F};
    float eye_roundness{1.0F};
    float eye_depth{0.0F};
    float eye_tilt{0.0F};
    float hair_density{1.0F};
    std::uint8_t hair_style{1};
    float hair_thickness{0.004F};
    float hair_volume{0.008F};
    float hair_brightness{1.0F};
    float nose_projection_scale{1.0F};
    float nose_bridge_scale{1.0F};
    float nose_tip_width_scale{1.0F};
    float nostril_width_scale{1.0F};
    float ear_scale{1.0F};
    float ear_angle{0.0F};
    float eye_asymmetry{0.0F};
    float brow_asymmetry{0.0F};
    float mouth_asymmetry{0.0F};
    float ear_asymmetry{0.0F};
    float temple_recession{0.0F};
    float widow_peak{0.0F};
    foundation::Color hair_color{0.20F, 0.12F, 0.08F, 1.0F};
    float blink_interval{4.0F};
    float blink_duration{0.14F};
    float gaze_restlessness{1.0F};

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] FaceSection section(float y) const noexcept;
    [[nodiscard]] foundation::Vec3 point(float y, float theta) const noexcept;
    [[nodiscard]] float frontZ(float y) const noexcept;
};

} // namespace genomes::infantry
