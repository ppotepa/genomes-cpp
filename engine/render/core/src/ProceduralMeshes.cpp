#include <genomes/render/ProceduralMeshes.hpp>
#include <genomes/geometry/PrimitiveBuilder.hpp>

namespace genomes::render::procedural {

MeshResult make_box_result(foundation::StableId mesh_id,
                           foundation::Vec3 half_extents,
                           foundation::Color color) {
    const auto primitive = geometry::makeBoxResult({
        {half_extents.x*2.0F, half_extents.y*2.0F, half_extents.z*2.0F}});
    if (!primitive) return MeshResult::failure(primitive.error());
    auto mesh=std::make_shared<RenderMesh>();
    mesh->mesh_id=mesh_id;mesh->revision=1U;
    mesh->vertices.reserve(primitive.value().vertices.size());
    mesh->indices=primitive.value().indices;
    for(const auto& vertex:primitive.value().vertices)
        mesh->vertices.push_back({vertex.position,vertex.normal,vertex.uv,color});
    return MeshResult::success(std::shared_ptr<const RenderMesh>{std::move(mesh)});
}

std::shared_ptr<const RenderMesh> make_box(foundation::StableId mesh_id,
                                           foundation::Vec3 half_extents,
                                           foundation::Color color) {
    auto result=make_box_result(mesh_id,half_extents,color);
    return result ? std::move(result).value() : nullptr;
}

} // namespace genomes::render::procedural
