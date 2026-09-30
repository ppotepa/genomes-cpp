#include <genomes/geometry/MeshRepair.hpp>
#include <genomes/geometry/MeshOperations.hpp>
#include <genomes/geometry/MeshOptimizer.hpp>
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
    const auto moved = transform(box, genomes::math::Transform{{1,2,3},{},{2,2,2}});
    assert(moved && moved.value().bounds.center().x > box.bounds.center().x);
    const auto sixteen = convertIndexFormat(box, IndexFormat::UInt16);
    assert(sixteen && sixteen.value().index_format == IndexFormat::UInt16);
    const auto optimized = optimizeMesh(box, OptimizationPolicy::Skinned);
    assert(optimized && !optimized.value().report.vertices_remapped);

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
    return 0;
}
