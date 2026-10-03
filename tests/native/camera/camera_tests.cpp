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
    assert(ray && genomes::math::lengthSquared(ray.value().second)>0.99F);
    genomes::math::Aabb bounds{};
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
    CameraRequest shifted=request;
    shifted.lens.projection_offset_x=-0.25F;
    shifted.lens.projection_offset_y=0.125F;
    const auto shifted_resolved=resolve(shifted,1280,720);
    assert(shifted_resolved);
    const auto shifted_screen=project(shifted_resolved.value(),shifted.target);
    assert(shifted_screen);
    assert(std::fabs(shifted_screen.value().x-480.0F)<1.0e-3F);
    assert(std::fabs(shifted_screen.value().y-315.0F)<1.0e-3F);
    const auto shifted_world=unproject(shifted_resolved.value(),shifted_screen.value());
    assert(shifted_world && genomes::math::lengthSquared(shifted_world.value()-shifted.target)<1.0e-5F);
    CameraRequest tilted{};
    tilted.position={17.0F,31.0F,43.0F}; tilted.target={-4.0F,2.0F,7.0F};
    const auto tilted_resolved=resolve(tilted,1600,900);
    assert(tilted_resolved);
    const auto tilted_screen=project(tilted_resolved.value(),tilted.target);
    assert(tilted_screen);
    const auto tilted_world=unproject(tilted_resolved.value(),tilted_screen.value());
    // The projected depth is carried through a Float32 D3D depth value.  At
    // ~54 m with a 0.05..1000 m frustum, its quantization bounds the absolute
    // world-space round-trip error to the centimetre range.
    assert(tilted_world && genomes::math::length(tilted_world.value()-tilted.target)<1.0e-2F);
    const auto center_ray=screenRay(tilted_resolved.value(),tilted_screen.value().x,
                                    tilted_screen.value().y);
    assert(center_ray);
    const auto expected_direction=genomes::math::normalized(tilted.target-tilted.position);
    assert(genomes::math::dot(center_ray.value().second,expected_direction)>0.9999F);
    shifted.lens.projection_offset_x=NAN;
    assert(!resolve(shifted,1280,720));
    return 0;
}
