#include <genomes/camera/CameraController.hpp>
#include <algorithm>
#include <cmath>

namespace genomes::camera {
void CameraController::reset(const CameraRequest& request) noexcept {
    mode_=request.mode;
    home_position_=request.position; home_target_=request.target;
    const math::Vec3 offset=request.position-request.target;
    distance_=std::max(0.01F,math::length(offset));
    yaw_=std::atan2(offset.x,offset.z);
    pitch_=std::asin(std::clamp(offset.y/distance_,-0.999F,0.999F));
}
void CameraController::update(CameraRequest& request,const CameraInput& input,float dt) noexcept {
    if (input.reset||input.cancel||input.focus_lost) { request.position=home_position_; request.target=home_target_; reset(request); return; }
    if (!(std::isfinite(dt)&&dt>=0)) return;
    if (mode_==ControlMode::Fixed) return;
    if (mode_==ControlMode::Orbit) {
        yaw_+=input.orbit_x; pitch_=std::clamp(pitch_+input.orbit_y,-1.5F,1.5F);
        distance_=std::max(0.05F,distance_*(1.0F+input.zoom));
        const float cp=std::cos(pitch_); request.target=home_target_;
        request.position=request.target+math::Vec3{std::sin(yaw_)*cp*distance_,std::sin(pitch_)*distance_,std::cos(yaw_)*cp*distance_};
        return;
    }
    const float speed=(mode_==ControlMode::RTS?8.0F:4.0F)*dt;
    request.position=request.position+math::Vec3{input.move_x*speed,input.move_y*speed,input.move_z*speed};
    request.target=request.target+math::Vec3{input.move_x*speed,input.move_y*speed,input.move_z*speed};
    if (mode_==ControlMode::RTS) request.position.y=std::max(0.05F,request.position.y+input.zoom*speed*4.0F);
}
} // namespace genomes::camera
