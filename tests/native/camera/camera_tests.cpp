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
    math::Aabb bounds{};
    bounds.include({-1.0F, -2.0F, -1.0F});
    bounds.include({3.0F, 2.0F, 1.0F});
    const auto fitted=fitToBounds(bounds,request);
    assert(fitted && fitted.value().target.x==1.0F && fitted.value().target.y==0.0F);
    CameraRequest degenerate=request;
    degenerate.position=degenerate.target;
    const auto bad_fit=fitToBounds(bounds,degenerate);
    assert(!bad_fit && bad_fit.error().code==genomes::foundation::ErrorCode::InvalidArgument);
    const auto bad_project=project(resolved.value(), {NAN, 0.0F, 0.0F});
    assert(!bad_project && bad_project.error().code==genomes::foundation::ErrorCode::InvalidArgument);
    const auto bad_unproject=unproject(resolved.value(), {0.0F, INFINITY, 0.5F});
    assert(!bad_unproject && bad_unproject.error().code==genomes::foundation::ErrorCode::InvalidArgument);
    const auto bad_ray=screenRay(resolved.value(), NAN, 0.0F);
    assert(!bad_ray && bad_ray.error().code==genomes::foundation::ErrorCode::InvalidArgument);
    const auto invalid=resolve(request,0,720);
    assert(!invalid && invalid.error().code==genomes::foundation::ErrorCode::InvalidArgument);
    return 0;
}
