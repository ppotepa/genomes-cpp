#include <genomes/camera/Camera.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace genomes::camera {
namespace {
foundation::Error error(foundation::ErrorCode code, std::string_view message) { return {code, message}; }
bool validViewport(ViewportNormalized v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.width)&&std::isfinite(v.height)&&v.x>=0&&v.y>=0&&v.width>0&&v.height>0&&v.x+v.width<=1&&v.y+v.height<=1; }
bool validLens(CameraLens l) { return std::isfinite(l.vertical_fov)&&std::isfinite(l.near_plane)&&std::isfinite(l.far_plane)&&std::isfinite(l.projection_offset_x)&&std::isfinite(l.projection_offset_y)&&l.vertical_fov>0&&l.vertical_fov<3.13F&&l.near_plane>0&&l.far_plane>l.near_plane&&std::abs(l.projection_offset_x)<=1.0F&&std::abs(l.projection_offset_y)<=1.0F; }
void normalizePlane(math::Plane& p) { const float n=math::length(p.normal); if(n>1e-8F){p.normal=p.normal/n;p.distance/=n;} }
}

foundation::Result<ResolvedCamera, foundation::Error> resolve(const CameraRequest& request, int width, int height) {
    if (width<=0||height<=0) return foundation::Result<ResolvedCamera, foundation::Error>::failure(error(foundation::ErrorCode::InvalidArgument,"framebuffer dimensions must be positive"));
    if (!validViewport(request.viewport)||!validLens(request.lens)||!math::finite(request.position)||!math::finite(request.target)||!math::finite(request.up)) return foundation::Result<ResolvedCamera, foundation::Error>::failure(error(foundation::ErrorCode::InvalidArgument,"camera request is invalid"));
    const math::Vec3 forward=request.target-request.position;
    if (math::lengthSquared(forward)<1e-10F||math::lengthSquared(request.up)<1e-10F) return foundation::Result<ResolvedCamera, foundation::Error>::failure(error(foundation::ErrorCode::InvalidArgument,"camera pose is degenerate"));
    ResolvedCamera result{};
    result.viewport={static_cast<int>(std::lround(request.viewport.x*width)),static_cast<int>(std::lround(request.viewport.y*height)),static_cast<int>(std::lround(request.viewport.width*width)),static_cast<int>(std::lround(request.viewport.height*height))};
    result.viewport.width=std::max(1,result.viewport.width); result.viewport.height=std::max(1,result.viewport.height);
    result.view=math::lookAtRH(request.position,request.target,request.up);
    result.projection=math::perspectiveD3D(request.lens.vertical_fov,static_cast<float>(result.viewport.width)/result.viewport.height,request.lens.near_plane,request.lens.far_plane);
    result.projection(0,2)=-request.lens.projection_offset_x;
    result.projection(1,2)=-request.lens.projection_offset_y;
    result.view_projection=result.projection*result.view;
    auto iv=result.view.rigidInverse(); auto ip=result.projection.inverse(); auto ivp=result.view_projection.inverse();
    if(!iv||!ip||!ivp) return foundation::Result<ResolvedCamera, foundation::Error>::failure(error(foundation::ErrorCode::InvalidState,"camera matrix inversion failed"));
    result.inverse_view=*iv; result.inverse_projection=*ip; result.inverse_view_projection=*ivp;
    const auto& m=result.view_projection;
    result.frustum.planes[0]={{m(3,0)+m(0,0),m(3,1)+m(0,1),m(3,2)+m(0,2)},m(3,3)+m(0,3)};
    result.frustum.planes[1]={{m(3,0)-m(0,0),m(3,1)-m(0,1),m(3,2)-m(0,2)},m(3,3)-m(0,3)};
    result.frustum.planes[2]={{m(3,0)+m(1,0),m(3,1)+m(1,1),m(3,2)+m(1,2)},m(3,3)+m(1,3)};
    result.frustum.planes[3]={{m(3,0)-m(1,0),m(3,1)-m(1,1),m(3,2)-m(1,2)},m(3,3)-m(1,3)};
    result.frustum.planes[4]={{m(2,0),m(2,1),m(2,2)},m(2,3)};
    result.frustum.planes[5]={{m(3,0)-m(2,0),m(3,1)-m(2,1),m(3,2)-m(2,2)},m(3,3)-m(2,3)};
    for(auto& p:result.frustum.planes) normalizePlane(p);
    return foundation::Result<ResolvedCamera, foundation::Error>::success(std::move(result));
}

foundation::Result<math::Vec3, foundation::Error> project(const ResolvedCamera& camera, math::Vec3 world) {
    if (!math::finite(world))
        return foundation::Result<math::Vec3, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument, "world point is not finite"));
    const math::Vec4 clip=camera.view_projection*math::Vec4{world.x,world.y,world.z,1};
    if(!std::isfinite(clip.w)||std::fabs(clip.w)<1e-8F) return foundation::Result<math::Vec3, foundation::Error>::failure(error(foundation::ErrorCode::InvalidState,"point cannot be projected"));
    const math::Vec3 ndc{clip.x/clip.w,clip.y/clip.w,clip.z/clip.w};
    return foundation::Result<math::Vec3, foundation::Error>::success({camera.viewport.x+(ndc.x+1)*0.5F*camera.viewport.width,camera.viewport.y+(1-ndc.y)*0.5F*camera.viewport.height,ndc.z});
}
foundation::Result<math::Vec3, foundation::Error> unproject(const ResolvedCamera& camera, math::Vec3 screen) {
    if (!math::finite(screen))
        return foundation::Result<math::Vec3, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument, "screen point is not finite"));
    if(camera.viewport.width<=0||camera.viewport.height<=0) return foundation::Result<math::Vec3, foundation::Error>::failure(error(foundation::ErrorCode::InvalidArgument,"viewport is empty"));
    const math::Vec4 world=camera.inverse_view_projection*math::Vec4{(screen.x-camera.viewport.x)*2.0F/camera.viewport.width-1,1-(screen.y-camera.viewport.y)*2.0F/camera.viewport.height,screen.z,1};
    if(std::fabs(world.w)<1e-8F) return foundation::Result<math::Vec3, foundation::Error>::failure(error(foundation::ErrorCode::InvalidState,"point cannot be unprojected"));
    return foundation::Result<math::Vec3, foundation::Error>::success({world.x/world.w,world.y/world.w,world.z/world.w});
}
foundation::Result<std::pair<math::Vec3,math::Vec3>, foundation::Error> screenRay(const ResolvedCamera& camera,float x,float y) {
    if (!std::isfinite(x) || !std::isfinite(y))
        return foundation::Result<std::pair<math::Vec3,math::Vec3>, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument, "screen coordinate is not finite"));
    auto near_point=unproject(camera,{x,y,0}); auto far_point=unproject(camera,{x,y,1});
    if(!near_point||!far_point) return foundation::Result<std::pair<math::Vec3,math::Vec3>, foundation::Error>::failure(error(foundation::ErrorCode::InvalidArgument,"screen coordinate is invalid"));
    math::Vec3 direction=far_point.value()-near_point.value(); if(!math::normalize(direction)) return foundation::Result<std::pair<math::Vec3,math::Vec3>, foundation::Error>::failure(error(foundation::ErrorCode::InvalidState,"screen ray is degenerate"));
    return foundation::Result<std::pair<math::Vec3,math::Vec3>, foundation::Error>::success(std::make_pair(near_point.value(),direction));
}
foundation::Result<CameraRequest, foundation::Error> fitToBounds(const math::Aabb& bounds,const CameraRequest& source) {
    if(bounds.empty || !math::finite(bounds.min) || !math::finite(bounds.max) ||
       bounds.min.x > bounds.max.x || bounds.min.y > bounds.max.y || bounds.min.z > bounds.max.z)
        return foundation::Result<CameraRequest, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument,"cannot fit invalid bounds"));
    if(!validLens(source.lens) || !math::finite(source.position) ||
       !math::finite(source.target) || !math::finite(source.up))
        return foundation::Result<CameraRequest, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument,"camera template is invalid"));
    const math::Vec3 source_offset=source.position-source.target;
    bool direction_valid=false;
    const math::Vec3 direction=math::normalized(source_offset,&direction_valid);
    if(!direction_valid)
        return foundation::Result<CameraRequest, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument,"camera template pose is degenerate"));
    const float half_fov=source.lens.vertical_fov*0.5F;
    const float tangent=std::tan(half_fov);
    if(!std::isfinite(tangent) || tangent<=0.0F)
        return foundation::Result<CameraRequest, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument,"camera template lens is degenerate"));
    CameraRequest result=source;
    const math::Vec3 center=bounds.center();
    const float radius=std::max(1e-4F,math::length(bounds.extent()));
    const float distance=radius/tangent;
    result.target=center;
    result.position=center+direction*distance;
    return foundation::Result<CameraRequest, foundation::Error>::success(std::move(result));
}
}
