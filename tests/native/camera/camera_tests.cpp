#include <genomes/camera/Camera.hpp>
#include <cassert>
#include <cmath>

int main() {
    using namespace genomes::camera;
    CameraRequest request{};
    const auto resolved=resolve(request,1280,720);
    assert(resolved);
    const auto screen=project(resolved.value(),request.target);
    assert(screen && screen.value().z>=0.0F && screen.value().z<=1.0F);
    const auto world=unproject(resolved.value(),screen.value());
    assert(world && std::fabs(world.value().z-request.target.z)<1e-3F);
    const auto ray=screenRay(resolved.value(),screen.value().x,screen.value().y);
    assert(ray && math::lengthSquared(ray.value().second)>0.99F);
    const auto invalid=resolve(request,0,720);
    assert(!invalid && invalid.error().code==genomes::foundation::ErrorCode::InvalidArgument);
    return 0;
}
