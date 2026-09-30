#include <genomes/geometry/ParametricSurface.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace genomes::geometry {

foundation::Vec3 ellipsoidPoint(const EllipsoidSpec& spec, float latitude,
                                float longitude) noexcept {
    const float cp = std::cos(latitude);
    return {spec.center.x + spec.radii.x * cp * std::cos(longitude),
            spec.center.y + spec.radii.y * std::sin(latitude),
            spec.center.z + spec.radii.z * cp * std::sin(longitude)};
}

foundation::Vec3 ellipsoidNormal(float latitude, float longitude) noexcept {
    const float cp = std::cos(latitude);
    return {cp * std::cos(longitude), std::sin(latitude), cp * std::sin(longitude)};
}

foundation::Result<MeshData, foundation::Error> tessellateParametric(const ParametricSurfaceSpec& spec) {
    using Result=foundation::Result<MeshData,foundation::Error>;
    if(!spec.position||spec.u_segments==0U||spec.v_segments==0U||!std::isfinite(spec.u_min)||!std::isfinite(spec.u_max)||!std::isfinite(spec.v_min)||!std::isfinite(spec.v_max)||spec.u_max<=spec.u_min||spec.v_max<=spec.v_min)return Result::failure({foundation::ErrorCode::InvalidArgument,"parametric surface domain or sampling is invalid"});
    MeshData mesh{}; const std::size_t width=static_cast<std::size_t>(spec.u_segments)+1U; const std::size_t height=static_cast<std::size_t>(spec.v_segments)+1U; mesh.vertices.resize(width*height);
    for(std::uint32_t v=0;v<=spec.v_segments;++v)for(std::uint32_t u=0;u<=spec.u_segments;++u){const float fu=static_cast<float>(u)/spec.u_segments,fv=static_cast<float>(v)/spec.v_segments,du=(spec.u_max-spec.u_min)/spec.u_segments*0.5F,dv=(spec.v_max-spec.v_min)/spec.v_segments*0.5F,U=spec.u_min+fu*(spec.u_max-spec.u_min),V=spec.v_min+fv*(spec.v_max-spec.v_min);const auto p=spec.position(U,V),pu=spec.position(std::min(spec.u_max,U+du),V)-spec.position(std::max(spec.u_min,U-du),V),pv=spec.position(U,std::min(spec.v_max,V+dv))-spec.position(U,std::max(spec.v_min,V-dv));auto normal=math::normalized(math::cross(pu,pv));if(!math::finite(p)||math::lengthSquared(normal)<1e-10F)return Result::failure({foundation::ErrorCode::InvalidArgument,"parametric callback produced invalid or degenerate sample"});mesh.vertices[static_cast<std::size_t>(v)*width+u]={{p.x,p.y,p.z},{normal.x,normal.y,normal.z},{fu,fv}};}
    mesh.indices.reserve(static_cast<std::size_t>(spec.u_segments)*spec.v_segments*6U);for(std::uint32_t v=0;v<spec.v_segments;++v)for(std::uint32_t u=0;u<spec.u_segments;++u){const auto base=static_cast<std::uint32_t>(static_cast<std::size_t>(v)*width+u);const auto row=static_cast<std::uint32_t>(width);mesh.indices.insert(mesh.indices.end(),{base,base+row,base+1U,base+1U,base+row,base+row+1U});}mesh.rebuildStreams();mesh.submeshes.push_back({0U,static_cast<std::uint32_t>(mesh.indices.size()),0U});return Result::success(std::move(mesh));
}

} // namespace genomes::geometry
