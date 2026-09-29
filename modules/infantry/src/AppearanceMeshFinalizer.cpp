#include <genomes/infantry/AppearanceMeshFinalizer.hpp>
#include <genomes/geometry/MeshRepair.hpp>

#include <algorithm>
#include <cmath>
#include <vector>
#include <utility>

namespace genomes::infantry {

foundation::Result<void, foundation::Error> finalizeAppearanceMesh(AppearanceMesh& mesh) {
    using Result=foundation::Result<void,foundation::Error>;
    if(mesh.vertices.empty()||mesh.indices.empty())
        return Result::failure({foundation::ErrorCode::InvalidArgument,
                                "cannot finalize an empty appearance mesh"});
    std::vector<foundation::Vec3> positions,normals;
    positions.reserve(mesh.vertices.size());
    normals.reserve(mesh.vertices.size());
    for(const auto& vertex:mesh.vertices){positions.push_back(vertex.position);normals.push_back(vertex.normal);}
    std::vector<geometry::TriangleGroup> groups;
    groups.reserve(mesh.groups.size());
    for(const auto& group:mesh.groups)groups.push_back({group.start,group.count,group.material});
    auto repaired=geometry::repairTriangleMesh(positions,normals,mesh.indices,groups);
    if(!repaired)return Result::failure(repaired.error());
    auto value=std::move(repaired.value());
    mesh.indices=std::move(value.indices);
    mesh.groups.clear();mesh.groups.reserve(value.groups.size());
    for(const auto& group:value.groups)mesh.groups.push_back({group.start,group.count,group.material});
    for(std::size_t index=0;index<mesh.vertices.size();++index)mesh.vertices[index].normal=value.normals[index];

    mesh.minimum=mesh.maximum=mesh.vertices.front().position;
    for(const auto& vertex:mesh.vertices){
        mesh.minimum.x=std::min(mesh.minimum.x,vertex.position.x);
        mesh.minimum.y=std::min(mesh.minimum.y,vertex.position.y);
        mesh.minimum.z=std::min(mesh.minimum.z,vertex.position.z);
        mesh.maximum.x=std::max(mesh.maximum.x,vertex.position.x);
        mesh.maximum.y=std::max(mesh.maximum.y,vertex.position.y);
        mesh.maximum.z=std::max(mesh.maximum.z,vertex.position.z);}
    mesh.sphere_center={(mesh.minimum.x+mesh.maximum.x)*.5F,
                        (mesh.minimum.y+mesh.maximum.y)*.5F,
                        (mesh.minimum.z+mesh.maximum.z)*.5F};
    float radius2=0.0F;
    for(const auto& vertex:mesh.vertices){const float x=vertex.position.x-mesh.sphere_center.x;
        const float y=vertex.position.y-mesh.sphere_center.y,z=vertex.position.z-mesh.sphere_center.z;
        radius2=std::max(radius2,x*x+y*y+z*z);}
    mesh.sphere_radius=std::sqrt(radius2);
    return Result::success();
}

} // namespace genomes::infantry
