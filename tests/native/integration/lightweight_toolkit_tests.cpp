#include <genomes/camera/Camera.hpp>
#include <genomes/geometry/MeshOptimizer.hpp>
#include <genomes/geometry/PrimitiveBuilder.hpp>
#include <genomes/geometry/TangentSpace.hpp>
#include <genomes/render/PresentationSnapshot.hpp>

#include <cassert>
#include <memory>

int main() {
    using namespace genomes;
    const auto primitive=geometry::makeBoxResult({{1,2,1}});
    assert(primitive);
    const auto optimized=geometry::optimizeMesh(primitive.value(),geometry::OptimizationPolicy::Static);
    assert(optimized && optimized.value().mesh.indices.size()==primitive.value().indices.size());
    const auto tangent=geometry::generateTangents(optimized.value().mesh);
    assert(tangent && tangent.value().tangents.size()==tangent.value().positions.size());

    auto render_mesh=std::make_shared<render::RenderMesh>();
    render_mesh->mesh_id=foundation::stable_id("test.lightweight.mesh");
    render_mesh->revision=1;
    for (const auto& vertex : tangent.value().vertices) {
        render_mesh->vertices.push_back({vertex.position,vertex.normal,vertex.uv,
                                         {1,1,1,1},0});
    }
    render_mesh->indices=tangent.value().indices;
    render::PresentationSnapshot snapshot{};
    snapshot.world_mesh=render_mesh;
    snapshot.instance_prototypes.push_back(render_mesh);
    snapshot.instances.push_back({1,render_mesh->mesh_id,0,{0,0,0}});
    assert(snapshot.world_mesh && snapshot.instance_prototypes.size()==1U);

    camera::CameraRequest request{};
    const auto resolved=camera::resolve(request,1280,720);
    assert(resolved && resolved.value().viewport.width==1280);
    snapshot.resolved_camera=resolved.value();
    snapshot.has_resolved_camera=true;
    assert(snapshot.has_resolved_camera);
    return 0;
}
