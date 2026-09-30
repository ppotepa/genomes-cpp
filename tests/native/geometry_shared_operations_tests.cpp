#include <genomes/geometry/MeshRepair.hpp>
#include <genomes/geometry/MeshOperations.hpp>
#include <genomes/geometry/MeshOptimizer.hpp>
#include <genomes/geometry/GeometryRecipe.hpp>
#include <genomes/geometry/TangentSpace.hpp>
#include <genomes/geometry/PrimitiveBuilder.hpp>

#include <cassert>

int main() {
    using namespace genomes::geometry;
    const auto box=makeBox({{2.0F,4.0F,6.0F}});
    assert(box.valid());
    assert(box.vertices.size()==24U);
    assert(box.indices.size()==36U);
    const auto invalid = makeBoxResult({{-1.0F, 1.0F, 1.0F}});
    assert(!invalid && invalid.error().code == genomes::foundation::ErrorCode::InvalidArgument);
    assert(box.positions.size() == box.vertices.size() && !box.bounds.empty);
    const auto combined = combine(std::span<const MeshData>(&box, 1));
    assert(combined && combined.value().vertices.size() == box.vertices.size());
    auto attributed = box;
    attributed.tangents.assign(attributed.vertices.size(), {1.0F, 0.0F, 0.0F, 1.0F});
    attributed.colors.assign(attributed.vertices.size(), {0.2F, 0.4F, 0.6F, 1.0F});
    assert(attributed.valid());
    const MeshData attributed_meshes[] = {attributed, attributed};
    const auto combined_attributed = combine(std::span<const MeshData>(attributed_meshes, 2));
    assert(combined_attributed && combined_attributed.value().vertices.size() == 48U);
    assert(combined_attributed.value().tangents.size() == 48U);
    assert(combined_attributed.value().colors.size() == 48U);
    assert(combined_attributed.value().submeshes.size() == 2U);
    auto stream_only = attributed;
    stream_only.vertices.clear();
    const MeshData stream_meshes[] = {stream_only, stream_only};
    const auto combined_streams = combine(std::span<const MeshData>(stream_meshes, 2));
    assert(combined_streams && combined_streams.value().vertices.size() == 48U);
    const auto moved = transform(attributed, genomes::math::Transform{{1,2,3},{},{2,3,4}});
    assert(moved && moved.value().bounds.center().x > box.bounds.center().x);
    assert(moved.value().tangents.size() == attributed.tangents.size());
    assert(moved.value().colors.size() == attributed.colors.size());
    assert(moved.value().colors.front().r == attributed.colors.front().r);
    assert(moved.value().colors.front().g == attributed.colors.front().g);
    assert(moved.value().colors.front().b == attributed.colors.front().b);
    assert(moved.value().colors.front().a == attributed.colors.front().a);
    assert(moved.value().vertices.size() == attributed.vertices.size());
    assert(moved.value().normals.size() == attributed.normals.size());
    const auto reflected = transform(attributed, genomes::math::Transform{{},{},{-1,1,1}});
    assert(reflected && reflected.value().indices.size() == attributed.indices.size());
    assert(reflected.value().indices[0] == attributed.indices[0]);
    assert(reflected.value().indices[1] == attributed.indices[2]);
    assert(reflected.value().indices[2] == attributed.indices[1]);
    assert(reflected.value().colors.size() == attributed.colors.size());
    assert(reflected.value().colors.front().r == attributed.colors.front().r);
    const auto sixteen = convertIndexFormat(box, IndexFormat::UInt16);
    assert(sixteen && sixteen.value().index_format == IndexFormat::UInt16);
    const auto optimized = optimizeMesh(box, OptimizationPolicy::Skinned);
    assert(optimized && !optimized.value().report.vertices_remapped);
    const float recipe_parameters[] = {2.0F, 4.0F, 6.0F};
    const GeometryRecipe recipe{GeometryOperation::Primitive, "box.v1", recipe_parameters, 7U};
    const GeometryRecipe recipe_copy{GeometryOperation::Primitive, "box.v1", recipe_parameters, 7U};
    assert(recipe.identity() == recipe_copy.identity());
    const auto tangents = generateTangents(box);
    assert(tangents && tangents.value().tangents.size() == box.vertices.size());

    MeshData transformed;
    appendTransformed(transformed,box,{{2.0F,3.0F,4.0F},{1.0F,2.0F,1.0F},0.5F});
    assert(transformed.valid());
    assert(transformed.vertices.size()==box.vertices.size());

    const genomes::foundation::Vec3 positions[]={{0,0,0},{1,0,0},{0,1,0},{0,0,0}};
    const genomes::foundation::Vec3 normals[]={{0,0,1},{0,0,1},{0,0,1},{0,0,0}};
    const std::uint32_t indices[]={0,1,2,0,3,1};
    const TriangleGroup groups[]={{0U,6U,0U}};
    const auto repaired=repairTriangleMesh(positions,normals,indices,groups);
    assert(repaired);
    assert(repaired.value().indices.size()==3U);
    assert(repaired.value().groups.size()==1U);
    assert(repaired.value().groups.front().count==3U);
    assert(repaired.value().stats.removed_degenerate_triangles==1U);
    const auto repaired_mesh=repairMesh(box);
    assert(repaired_mesh && repaired_mesh.value().mesh.valid());
    return 0;
}
