#include <genomes/geometry/ProfileSurface.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace genomes::geometry {
foundation::Result<MeshData, foundation::Error> sweepProfile(const SweepSpec& spec) {
    using Result=foundation::Result<MeshData,foundation::Error>;
    if(spec.centers.size()<2U||spec.centers.size()!=spec.radii.size()||spec.profile_segments<3U)return Result::failure({foundation::ErrorCode::InvalidArgument,"sweep requires matching path/radius samples and three profile segments"});
    const std::size_t count=spec.centers.size(); if(spec.closed&&count<3U)return Result::failure({foundation::ErrorCode::InvalidArgument,"closed sweep requires three path samples"});
    for(std::size_t i=0;i<count;++i)if(!math::finite(spec.centers[i])||!math::finite(spec.radii[i])||spec.radii[i].x<=0||spec.radii[i].y<=0)return Result::failure({foundation::ErrorCode::InvalidArgument,"sweep contains invalid path or radius"});
    MeshBuilder builder; std::vector<Ring> rings; rings.reserve(count); math::Vec3 tangent=math::normalized(spec.centers[1]-spec.centers[0]); math::Vec3 axis_u=math::normalized(std::fabs(math::dot({0,1,0},tangent))<0.9F?math::cross({0,1,0},tangent):math::cross({1,0,0},tangent)); math::Vec3 axis_v=math::normalized(math::cross(tangent,axis_u));
    for(std::size_t path=0;path<count;++path){const std::size_t previous=path==0? (spec.closed?count-1U:0U):path-1U;const std::size_t next=(path+1U)%count;tangent=spec.closed?math::normalized(spec.centers[next]-spec.centers[previous]):math::normalized(path==0?spec.centers[1]-spec.centers[0]:(path+1U==count?spec.centers[path]-spec.centers[path-1U]:spec.centers[path+1U]-spec.centers[path-1U]));if(path>0){axis_u=math::normalized(axis_u-tangent*math::dot(axis_u,tangent));if(math::lengthSquared(axis_u)<1e-8F)axis_u=math::normalized(std::fabs(math::dot({0,1,0},tangent))<0.9F?math::cross({0,1,0},tangent):math::cross({1,0,0},tangent));axis_v=math::normalized(math::cross(tangent,axis_u));}ProfileRingSpec ring_spec{spec.centers[path],axis_u,axis_v,spec.radii[path].x,spec.radii[path].y, spec.profile_segments,static_cast<float>(path)/static_cast<float>(std::max<std::size_t>(1U,count-1U))};rings.push_back(appendProfileRing(builder,ring_spec,[](MeshBuilder& target,foundation::Vec3 p,foundation::Vec3 n,foundation::Vec2 uv){return target.appendVertex({p,n,uv});}));}
    for(std::size_t i=0;i+1U<rings.size();++i)bridgeProfileRings(builder,rings[i],rings[i+1U]); if(spec.closed)bridgeProfileRings(builder,rings.back(),rings.front()); auto result=builder.build();if(!result)return result;return Result::success(std::move(result.value()));
}
}
