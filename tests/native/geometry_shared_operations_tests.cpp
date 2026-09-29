#include <genomes/geometry/MeshRepair.hpp>
#include <genomes/geometry/PrimitiveBuilder.hpp>

#include <cassert>

int main() {
    using namespace genomes::geometry;
    const auto box=makeBox({{2.0F,4.0F,6.0F}});
    assert(box.valid());
    assert(box.vertices.size()==24U);
    assert(box.indices.size()==36U);

    MeshData transformed;
    appendTransformed(transformed,box,{{2.0F,3.0F,4.0F},{1.0F,2.0F,1.0F},0.5F});
    assert(transformed.valid());
    assert(transformed.vertices.size()==box.vertices.size());

    const foundation::Vec3 positions[]={{0,0,0},{1,0,0},{0,1,0},{0,0,0}};
    const foundation::Vec3 normals[]={{0,0,1},{0,0,1},{0,0,1},{0,0,0}};
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
