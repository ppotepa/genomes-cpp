#pragma once

#include <genomes/camera/Camera.hpp>

namespace genomes::camera {

using ControlMode = CameraMode;
struct CameraInput final {
    float orbit_x{0};
    float orbit_y{0};
    float move_x{0};
    float move_y{0};
    float move_z{0};
    float zoom{0};
    bool reset{false};
    bool cancel{false};
    bool focus_lost{false};
};

class CameraController final {
public:
    explicit CameraController(ControlMode mode = ControlMode::Orbit) noexcept : mode_(mode) {}
    void setMode(ControlMode mode) noexcept { mode_ = mode; }
    [[nodiscard]] ControlMode mode() const noexcept { return mode_; }
    void reset(const CameraRequest& request) noexcept;
    void update(CameraRequest& request, const CameraInput& input, float delta_seconds) noexcept;

private:
    ControlMode mode_{ControlMode::Orbit};
    math::Vec3 home_position_{0,1,3};
    math::Vec3 home_target_{};
    float yaw_{0};
    float pitch_{0.2F};
    float distance_{3};
};

} // namespace genomes::camera
