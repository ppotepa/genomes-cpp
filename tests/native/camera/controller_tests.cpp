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
    return 0;
}
