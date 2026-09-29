#include <genomes/render/ProceduralMeshes.hpp>
#include <genomes/geometry/PrimitiveBuilder.hpp>

namespace genomes::render::procedural {

std::shared_ptr<const RenderMesh> make_box(foundation::StableId mesh_id,
                                           foundation::Vec3 half_extents,
                                           foundation::Color color) {
    const auto geometry = geometry::makeBox({
        {half_extents.x*2.0F, half_extents.y*2.0F, half_extents.z*2.0F}});
    auto mesh=std::make_shared<RenderMesh>();
    mesh->mesh_id=mesh_id;mesh->revision=1U;
    mesh->vertices.reserve(geometry.vertices.size());
    mesh->indices=geometry.indices;
    for(const auto& vertex:geometry.vertices)
        mesh->vertices.push_back({vertex.position,vertex.normal,vertex.uv,color});
    return mesh;
}

} // namespace genomes::render::procedural
