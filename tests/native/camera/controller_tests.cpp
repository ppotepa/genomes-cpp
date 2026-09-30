#include <genomes/camera/CameraController.hpp>
#include <cassert>

int main() {
    using namespace genomes::camera;
    CameraRequest request{}; CameraController controller{}; controller.reset(request);
    const auto original=request.position;
    controller.update(request,{1.0F,0,0,0,0,0,false,false,false},1.0F/60.0F);
    assert(request.position.x!=original.x || request.position.z!=original.z);
    controller.update(request,{},1.0F/60.0F);
    controller.update(request,{0,0,0,0,0,0,true,false,false},1.0F/60.0F);
    assert(request.position.x==original.x && request.position.y==original.y && request.position.z==original.z);
    controller.setMode(ControlMode::Fly);
    controller.update(request,{0,0,1,0,0,0,false,false,false},1.0F);
    assert(request.position.x>original.x);
    return 0;
}
