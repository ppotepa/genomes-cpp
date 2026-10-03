#include <genomes/camera/CameraController.hpp>
#include <cassert>
#include <cmath>

int main() {
    using namespace genomes::camera;
    CameraRequest request{}; CameraController controller{}; controller.reset(request);
    assert(request.mode == CameraMode::Orbit && controller.mode() == CameraMode::Orbit);
    const auto original=request.position;
    controller.update(request,{1.0F,0,0,0,0,0,false,false,false},1.0F/60.0F);
    assert(request.position.x!=original.x || request.position.z!=original.z);
    const auto before_pan = request.target;
    controller.update(request,{0,0,0,0,0,0,false,false,false,24.0F,-12.0F},1.0F/60.0F);
    assert(request.target.x != before_pan.x || request.target.y != before_pan.y ||
           request.target.z != before_pan.z);
    const auto orbit_offset = request.position - request.target;
    controller.rebaseOrbitTarget({2.0F, 0.0F, 0.0F});
    controller.update(request,{},1.0F/60.0F);
    const auto held_position = request.position;
    controller.update(request,{.focus_lost=true},1.0F/60.0F);
    assert(genomes::math::length(request.position-held_position) < 1.0e-5F);
    controller.update(request,{.cancel=true},1.0F/60.0F);
    assert(genomes::math::length(request.position-held_position) < 1.0e-5F);
    assert(request.target.x == 2.0F);
    assert(std::abs(request.position.x - request.target.x - orbit_offset.x) < 1.0e-5F);
    controller.update(request,{},1.0F/60.0F);
    controller.update(request,{0,0,0,0,0,0,true,false,false},1.0F/60.0F);
    assert(request.position.x==original.x && request.position.y==original.y && request.position.z==original.z);
    controller.setMode(ControlMode::Fly);
    request.mode = CameraMode::Fly;
    controller.reset(request);
    assert(controller.mode() == CameraMode::Fly);
    controller.update(request,{0,0,1,0,0,0,false,false,false},1.0F);
    assert(request.position.x>original.x);
    CameraRequest rts{};
    rts.mode=CameraMode::RTS; rts.position={0.0F,50.0F,80.0F}; rts.target={0,0,0};
    rts.rts.target_min={-20,-20}; rts.rts.target_max={20,20};
    CameraController rts_controller{CameraMode::RTS}; rts_controller.reset(rts);
    rts_controller.update(rts,{.move_x=1.0F,.move_z=1.0F},1.0F);
    assert(rts.target.x>0.0F && rts.target.z<0.0F);
    assert(genomes::math::length(rts.target)<29.0F);
    rts_controller.update(rts,{.orbit_y=10.0F,.zoom=-10.0F},0.0F);
    const float close_distance = genomes::math::length(rts.position-rts.target);
    assert(close_distance >= rts.rts.min_distance-0.001F);
    assert(close_distance <= rts.rts.max_distance+0.001F);
    assert(rts.position.y>rts.target.y);
    rts_controller.updateHome({.mode=CameraMode::RTS,.position={4,40,30},.target={4,0,3}});
    rts_controller.update(rts,{.reset=true},0.0F);
    assert(rts.target.x==4.0F && rts.target.z==3.0F);
    // The same physical drag must produce the same orbit at any frame rate.
    CameraRequest reference{};
    CameraController reference_controller;
    reference_controller.reset(reference);
    reference_controller.update(reference,{.orbit_x=.6F,.orbit_y=.2F},1.0F);
    for (int fps : {30,60,144}) {
        CameraRequest sampled{};
        CameraController sampled_controller;
        sampled_controller.reset(sampled);
        for (int frame = 0; frame < fps; ++frame)
            sampled_controller.update(sampled,{.orbit_x=.6F/fps,.orbit_y=.2F/fps},1.0F/fps);
        assert(genomes::math::length(sampled.position-reference.position) < 1.0e-4F);
        const auto stopped = sampled.position;
        for (int frame = 0; frame < fps; ++frame) sampled_controller.update(sampled,{},1.0F/fps);
        assert(genomes::math::length(sampled.position-stopped) < 1.0e-5F);
        CameraRequest pan_reference{}, pan_sampled{};
        CameraController pan_once, pan_frames;
        pan_once.reset(pan_reference); pan_frames.reset(pan_sampled);
        pan_once.update(pan_reference,{.pan_x=.2F,.pan_y=.1F},1.0F);
        for (int frame = 0; frame < fps; ++frame)
            pan_frames.update(pan_sampled,{.pan_x=.2F/fps,.pan_y=.1F/fps},1.0F/fps);
        assert(genomes::math::length(pan_sampled.target-pan_reference.target) < 1.0e-4F);
        pan_once.update(pan_reference,{.zoom=-.4F},1.0F);
        for (int frame = 0; frame < fps; ++frame)
            pan_frames.update(pan_sampled,{.zoom=-.4F/fps},1.0F/fps);
        assert(genomes::math::length(pan_sampled.position-pan_reference.position) < 1.0e-4F);
    }
    return 0;
}
