#include <genomes/camera/CameraController.hpp>
#include <algorithm>
#include <cmath>

namespace genomes::camera {
void CameraController::reset(const CameraRequest& request) noexcept {
    mode_=request.mode;
    home_position_=request.position; home_target_=request.target;
    orbit_target_=request.target;
    const math::Vec3 offset=request.position-request.target;
    distance_=std::max(0.01F,math::length(offset));
    yaw_=std::atan2(offset.x,offset.z);
    pitch_=std::asin(std::clamp(offset.y/distance_,-0.999F,0.999F));
    orbit_velocity_x_=orbit_velocity_y_=pan_velocity_x_=pan_velocity_y_=0.0F;
}
void CameraController::update(CameraRequest& request,const CameraInput& input,float dt) noexcept {
    if (input.reset||input.cancel||input.focus_lost) { request.position=home_position_; request.target=home_target_; reset(request); return; }
    if (!(std::isfinite(dt)&&dt>=0)) return;
    if (mode_==ControlMode::Fixed) return;
    if (mode_==ControlMode::Orbit) {
        const float damping = 1.0F-std::exp(-std::max(dt,0.0F)*18.0F);
        orbit_velocity_x_ += (input.orbit_x-orbit_velocity_x_)*damping;
        orbit_velocity_y_ += (input.orbit_y-orbit_velocity_y_)*damping;
        pan_velocity_x_ += (input.pan_x-pan_velocity_x_)*damping;
        pan_velocity_y_ += (input.pan_y-pan_velocity_y_)*damping;
        const float pan_scale = distance_;
        const math::Vec3 offset = request.position - request.target;
        const float offset_length = math::length(offset);
        if (std::isfinite(offset_length) && offset_length > 1.0e-5F &&
            (pan_velocity_x_ != 0.0F || pan_velocity_y_ != 0.0F)) {
            const math::Vec3 radial = offset / offset_length;
            math::Vec3 screen_right{radial.z, 0.0F, -radial.x};
            const float right_length = math::length(screen_right);
            if (right_length > 1.0e-5F) {
                screen_right = screen_right / right_length;
                const math::Vec3 screen_up{
                    radial.y * screen_right.z,
                    radial.z * screen_right.x - radial.x * screen_right.z,
                    -radial.y * screen_right.x};
                orbit_target_ = orbit_target_ - screen_right * (pan_velocity_x_ * pan_scale) +
                                screen_up * (pan_velocity_y_ * pan_scale);
            }
        }
        yaw_+=orbit_velocity_x_; pitch_=std::clamp(pitch_+orbit_velocity_y_,-1.5F,1.5F);
        distance_=std::max(0.05F,distance_*std::exp(input.zoom));
        const float cp=std::cos(pitch_); request.target=orbit_target_;
        request.position=request.target+math::Vec3{std::sin(yaw_)*cp*distance_,std::sin(pitch_)*distance_,std::cos(yaw_)*cp*distance_};
        return;
    }
    const float speed=(mode_==ControlMode::RTS?8.0F:4.0F)*dt;
    request.position=request.position+math::Vec3{input.move_x*speed,input.move_y*speed,input.move_z*speed};
    request.target=request.target+math::Vec3{input.move_x*speed,input.move_y*speed,input.move_z*speed};
    if (mode_==ControlMode::RTS) request.position.y=std::max(0.05F,request.position.y+input.zoom*speed*4.0F);
}
} // namespace genomes::camera
