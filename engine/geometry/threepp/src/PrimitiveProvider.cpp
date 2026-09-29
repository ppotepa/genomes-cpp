#include <genomes/geometry/threepp/PrimitiveProvider.hpp>

#include <threepp/core/BufferGeometry.hpp>
#include <threepp/geometries/BoxGeometry.hpp>
#include <threepp/geometries/CapsuleGeometry.hpp>
#include <threepp/geometries/CylinderGeometry.hpp>
#include <threepp/geometries/SphereGeometry.hpp>

#include <cmath>
#include <memory>
#include <utility>

namespace genomes::geometry::threepp_provider {
namespace {
foundation::Result<MeshData,foundation::Error> convert(
    const std::shared_ptr<threepp::BufferGeometry>& geometry) {
    using Result=foundation::Result<MeshData,foundation::Error>;
    if(!geometry)return Result::failure({foundation::ErrorCode::InvalidState,
                                         "threepp primitive allocation failed"});
    const auto* position=geometry->getAttribute<float>("position");
    const auto* normal=geometry->getAttribute<float>("normal");
    const auto* uv=geometry->getAttribute<float>("uv");
    if(position==nullptr||normal==nullptr||position->itemSize()!=3||normal->itemSize()!=3||
       normal->count()!=position->count())
        return Result::failure({foundation::ErrorCode::InvalidState,
                                "threepp primitive misses required vertex attributes"});
    MeshData mesh;mesh.vertices.reserve(static_cast<std::size_t>(position->count()));
    for(int index=0;index<position->count();++index){
        MeshVertex vertex{};
        vertex.position={position->getX(index),position->getY(index),position->getZ(index)};
        vertex.normal={normal->getX(index),normal->getY(index),normal->getZ(index)};
        if(uv!=nullptr&&uv->itemSize()>=2&&uv->count()==position->count())
            vertex.uv={uv->getX(index),uv->getY(index)};
        mesh.vertices.push_back(vertex);}
    if(const auto* indices=geometry->getIndex();indices!=nullptr){
        mesh.indices.assign(indices->array().begin(),indices->array().end());
    }else{
        mesh.indices.reserve(mesh.vertices.size());
        for(std::uint32_t index=0;index<mesh.vertices.size();++index)mesh.indices.push_back(index);
    }
    return mesh.valid()?Result::success(std::move(mesh))
        :Result::failure({foundation::ErrorCode::InvalidState,
                          "threepp primitive converted to an invalid mesh"});
}
bool positive(float value){return std::isfinite(value)&&value>0.0F;}
}
foundation::Result<MeshData,foundation::Error> box(foundation::Vec3 size){
    if(!positive(size.x)||!positive(size.y)||!positive(size.z))
        return foundation::Result<MeshData,foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument,"invalid threepp box dimensions"});
    return convert(threepp::BoxGeometry::create(size.x,size.y,size.z));}
foundation::Result<MeshData,foundation::Error> sphere(float radius,std::uint32_t w,std::uint32_t h){
    if(!positive(radius)||w<3U||h<2U)return foundation::Result<MeshData,foundation::Error>::failure(
        {foundation::ErrorCode::InvalidArgument,"invalid threepp sphere parameters"});
    return convert(threepp::SphereGeometry::create(radius,w,h));}
foundation::Result<MeshData,foundation::Error> cylinder(float top,float bottom,float height,std::uint32_t segments){
    if(!positive(top)||!positive(bottom)||!positive(height)||segments<3U)
        return foundation::Result<MeshData,foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument,"invalid threepp cylinder parameters"});
    return convert(threepp::CylinderGeometry::create(top,bottom,height,segments));}
foundation::Result<MeshData,foundation::Error> capsule(float radius,float length,std::uint32_t cap,std::uint32_t radial){
    if(!positive(radius)||!positive(length)||cap<1U||radial<3U)
        return foundation::Result<MeshData,foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument,"invalid threepp capsule parameters"});
    return convert(threepp::CapsuleGeometry::create(radius,length,cap,radial));}
} // namespace genomes::geometry::threepp_provider
