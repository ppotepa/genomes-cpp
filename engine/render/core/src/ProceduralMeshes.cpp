#include <genomes/render/ProceduralMeshes.hpp>
#include <genomes/geometry/PrimitiveBuilder.hpp>

#include <limits>

namespace genomes::render::procedural {

foundation::Result<void, foundation::Error> append_box(RenderMesh& destination,
                                                        foundation::Vec3 center,
                                                        foundation::Vec3 extent,
                                                        foundation::Color color) {
    const auto generated = geometry::makeBoxResult({extent});
    if (!generated) return foundation::Result<void, foundation::Error>::failure(generated.error());
    const auto& box = generated.value();
    if (destination.vertices.size() > std::numeric_limits<std::uint32_t>::max() - box.vertices.size())
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::OutOfRange, "procedural box vertex count overflows 32-bit indices"});
    const auto base = static_cast<std::uint32_t>(destination.vertices.size());
    destination.vertices.reserve(destination.vertices.size() + box.vertices.size());
    destination.indices.reserve(destination.indices.size() + box.indices.size());
    for (const auto& vertex : box.vertices)
        destination.vertices.push_back({{vertex.position.x + center.x,
                                         vertex.position.y + center.y,
                                         vertex.position.z + center.z},
                                        vertex.normal, vertex.uv, color});
    for (const auto index : box.indices) destination.indices.push_back(base + index);
    return foundation::Result<void, foundation::Error>::success();
}

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
