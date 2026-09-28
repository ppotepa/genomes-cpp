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
    float jaw_width{0.14F};
    float hairline_y{1.68F};
    float head_width{0.18F};
    float head_depth{0.16F};
    float cheek_fullness{1.0F};
    float eye_roundness{1.0F};
    float hair_density{1.0F};
    std::uint8_t hair_style{1};
    float hair_thickness{0.004F};
    float hair_volume{0.008F};
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
