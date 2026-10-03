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
    if (mode_==ControlMode::RTS) {
        pitch_=std::clamp(pitch_,request.rts.min_pitch,request.rts.max_pitch);
        distance_=std::clamp(distance_,request.rts.min_distance,request.rts.max_distance);
    }
}
void CameraController::updateHome(const CameraRequest& request) noexcept {
    home_position_=request.position;
    home_target_=request.target;
}
void CameraController::update(CameraRequest& request,const CameraInput& input,float dt) noexcept {
    if (input.reset) { request.position=home_position_; request.target=home_target_; reset(request); return; }
    if (input.cancel || input.focus_lost) return;
    if (!(std::isfinite(dt)&&dt>=0)) return;
    if (mode_==ControlMode::Fixed) return;
    if (mode_==ControlMode::Orbit) {
        // Pointer inputs are displacements, not velocities. Apply once without
        // frame-dependent filtering or continued drift after pointer release.
        const float pan_scale = 2.0F * distance_ * std::tan(request.lens.vertical_fov * .5F);
        const math::Vec3 offset = request.position - request.target;
        const float offset_length = math::length(offset);
        if (std::isfinite(offset_length) && offset_length > 1.0e-5F &&
            (input.pan_x != 0.0F || input.pan_y != 0.0F)) {
            const math::Vec3 radial = offset / offset_length;
            math::Vec3 screen_right{radial.z, 0.0F, -radial.x};
            const float right_length = math::length(screen_right);
            if (right_length > 1.0e-5F) {
                screen_right = screen_right / right_length;
                const math::Vec3 screen_up{
                    radial.y * screen_right.z,
                    radial.z * screen_right.x - radial.x * screen_right.z,
                    -radial.y * screen_right.x};
                orbit_target_ = orbit_target_ - screen_right * (input.pan_x * pan_scale) +
                                screen_up * (input.pan_y * pan_scale);
            }
        }
        yaw_+=input.orbit_x; pitch_=std::clamp(pitch_+input.orbit_y,-1.5F,1.5F);
        distance_=std::clamp(distance_*std::exp(std::clamp(input.zoom,-10.0F,10.0F)),
                             std::max(.05F, request.lens.near_plane * 2.0F),
                             std::max(.1F, request.lens.far_plane * .8F));
        const float cp=std::cos(pitch_); request.target=orbit_target_;
        request.position=request.target+math::Vec3{std::sin(yaw_)*cp*distance_,std::sin(pitch_)*distance_,std::cos(yaw_)*cp*distance_};
        return;
    }
    if (mode_==ControlMode::RTS) {
        yaw_+=input.orbit_x;
        pitch_=std::clamp(pitch_+input.orbit_y,request.rts.min_pitch,request.rts.max_pitch);
        distance_=std::clamp(distance_*std::exp(std::clamp(input.zoom,-10.0F,10.0F)),
                             request.rts.min_distance,request.rts.max_distance);
        const math::Vec3 forward{-std::sin(yaw_),0.0F,-std::cos(yaw_)};
        const math::Vec3 right{std::cos(yaw_),0.0F,-std::sin(yaw_)};
        math::Vec3 movement=right*input.move_x+forward*input.move_z;
        const float movement_length=math::length(movement);
        if (movement_length>1.0F) movement=movement/movement_length;
        const float speed=std::clamp(distance_*request.rts.move_speed_factor,
                                     request.rts.min_move_speed,
                                     request.rts.max_move_speed)*dt;
        const float pan_distance = distance_ * request.rts.pan_distance_factor;
        orbit_target_=orbit_target_+movement*speed-right*(input.pan_x*pan_distance)+
                      forward*(input.pan_y*pan_distance);
        orbit_target_.x=std::clamp(orbit_target_.x,request.rts.target_min.x,request.rts.target_max.x);
        orbit_target_.z=std::clamp(orbit_target_.z,request.rts.target_min.y,request.rts.target_max.y);
        request.target=orbit_target_;
        const float cp=std::cos(pitch_);
        request.position=request.target+math::Vec3{std::sin(yaw_)*cp*distance_,
            std::sin(pitch_)*distance_,std::cos(yaw_)*cp*distance_};
        return;
    }
    const float speed=4.0F*dt;
    request.position=request.position+math::Vec3{input.move_x*speed,input.move_y*speed,input.move_z*speed};
    request.target=request.target+math::Vec3{input.move_x*speed,input.move_y*speed,input.move_z*speed};
}
} // namespace genomes::camera
