#pragma once

#include <genomes/foundation/Types.hpp>

#include <cstdint>

namespace genomes::infantry {

struct FaceSection final {
    float radius_x{0.0};
    float radius_z{0.0};
    float center_z{0.0};
};

// Binary64 authoring values used by the pinned JavaScript-parity surface path.
// Runtime and GPU consumers continue to use the legacy fields in FacePhenotype;
// the explicit Float32 mesh boundary remains in ReferenceSurfaceBuilder.
struct ReferenceFaceParameters final {
    double head_width_scale{1.0};
    double head_depth_scale{1.0};
    double head_length_scale{1.0};
    double forehead_width_scale{1.0};
    double forehead_slope{0.0};
    double temple_width_scale{1.0};
    double brow_ridge{0.0};
    double jaw_width_scale{1.0};
    double jaw_length_scale{1.0};
    double jaw_angle{1.0};
    double chin_width_scale{1.0};
    double chin_height{0.0};
    double chin_projection{0.0};
    double cheekbone_scale{1.0};
    double cheekbone_y{0.0};
    double cheek_fullness{0.0};
    double midface_projection{0.0};
    double eye_spacing{0.02};
    double eye_width_scale{1.0};
    double eye_height_scale{1.0};
    double eye_roundness{1.0};
    double eye_depth{0.0};
    double eye_tilt{0.0};
    double eye_y{0.94};
    double brow_y{0.95};
    double brow_thickness{0.0008};
    double brow_tilt{0.0};
    double brow_spacing{0.0};
    double nose_width_scale{1.0};
    double nose_length_scale{1.0};
    double nose_projection_scale{1.0};
    double nose_bridge_scale{1.0};
    double nose_tip_width_scale{1.0};
    double nose_tip_rotation{0.0};
    double nostril_width_scale{1.0};
    double mouth_width{0.022};
    double upper_lip{0.002};
    double lower_lip{0.002};
    double mouth_y{0.90};
    double ear_scale{1.0};
    double ear_angle{0.0};
    double hair_density{1.0};
    double hair_thickness{0.004};
    double hair_volume{0.008};
    double hairline{0.95};
    double temple_recession{0.0};
    double widow_peak{0.0};
    double neutral_mouth{0.0};
    double eye_asymmetry{0.0};
    double brow_asymmetry{0.0};
    double mouth_asymmetry{0.0};
    double ear_asymmetry{0.0};
    std::uint32_t eye_color_hex{0};
    std::uint32_t hair_color_hex{0};
    std::uint8_t hair_style{1};
};

struct FacePhenotype final {
    std::uint32_t version{1};
    float head_width_scale{1.0};
    float head_depth_scale{1.0};
    float jaw_width_scale{1.0};
    float eye_spacing_ratio{0.02};
    float eye_y_ratio{0.94};
    float eye_size_scale{1.0};
    std::uint32_t eye_color_hex{0};
    float brow_y_ratio{0.95};
    float brow_thickness{0.0008};
    float brow_tilt{0.0};
    float brow_spacing{0.0};
    float nose_width_scale{1.0};
    float nose_length_scale{1.0};
    float nose_tip_rotation{0.0};
    float mouth_width_ratio{0.022};
    float mouth_y_ratio{0.90};
    std::uint32_t hair_color_hex{0};
    float hairline_ratio{0.95};
    float neutral_eye_open{0.8};
    float neutral_brow{0.0};
    float neutral_mouth{0.0};
    float expression_scale{1.0};
    float eye_expression_scale{1.0};
    float mouth_expression_scale{1.0};
    float brow_expression_scale{1.0};
    float eye_spacing{0.06};
    float eye_radius{0.015};
    float brow_y{1.60};
    float eye_y{1.56};
    float nose_y{1.47};
    float nose_length{0.04};
    float nose_width{0.025};
    float mouth_y{1.38};
    float mouth_width{0.055};
    float upper_lip{0.002};
    float lower_lip{0.002};
    float jaw_width{0.14};
    float hairline_y{1.68};
    float head_width{0.18};
    float head_depth{0.16};
    double reference_head_width_scale{1.0};
    double reference_head_depth_scale{1.0};
    float head_length_scale{1.};
    float forehead_width_scale{1.};
    float forehead_slope{0.};
    float temple_width_scale{1.};
    float brow_ridge{0.};
    float jaw_length_scale{1.};
    float jaw_angle{1.};
    float chin_width_scale{1.};
    float chin_height{0.};
    float chin_projection{0.};
    float cheekbone_scale{1.};
    float cheekbone_y{0.};
    float cheek_fullness{1.};
    float midface_projection{0.};
    float eye_width_scale{1.};
    float eye_height_scale{1.};
    float eye_roundness{1.};
    float eye_depth{0.};
    float eye_tilt{0.};
    float hair_density{1.};
    std::uint8_t hair_style{1};
    float hair_thickness{0.004};
    float hair_volume{0.008};
    float hair_brightness{1.};
    float nose_projection_scale{1.};
    float nose_bridge_scale{1.};
    float nose_tip_width_scale{1.};
    float nostril_width_scale{1.};
    float ear_scale{1.};
    float ear_angle{0.};
    float eye_asymmetry{0.};
    float brow_asymmetry{0.};
    float mouth_asymmetry{0.};
    float ear_asymmetry{0.};
    float temple_recession{0.};
    float widow_peak{0.};
    foundation::Color hair_color{0.20F, 0.12F, 0.08F, 1.0F};
    float blink_interval{4.0};
    float blink_duration{0.14};
    float gaze_restlessness{1.0};
    ReferenceFaceParameters reference{};

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] FaceSection section(float y) const noexcept;
    [[nodiscard]] foundation::Vec3 point(float y, float theta) const noexcept;
    [[nodiscard]] float frontZ(float y) const noexcept;
};

} // namespace genomes::infantry
