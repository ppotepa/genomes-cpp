#include <genomes/geometry/MeshData.hpp>
#include <genomes/geometry/MeshOperations.hpp>

#include <cassert>
#include <limits>

int main() {
    using namespace genomes::geometry;
    MeshData mesh{};
    mesh.vertices={{{0,0,0},{0,1,0},{0,0}},
                   {{1,0,0},{0,1,0},{1,0}},
                   {{0,0,1},{0,1,0},{0,1}}};
    mesh.indices={0,1,2};
    mesh.submeshes={{0,3,7}};
    mesh.rebuildStreams();
    assert(mesh.hasStreams() && mesh.positions.size()==3U);
    assert(mesh.normals.size()==mesh.positions.size());
    assert(mesh.uvs.size()==mesh.positions.size());
    assert(mesh.valid());
    mesh.colors.resize(mesh.positions.size(),{1,1,1,1});
    mesh.tangents.resize(mesh.positions.size(),{1,0,0,1});
    assert(mesh.valid());
    mesh.rebuildStreams();
    assert(mesh.colors.size() == mesh.positions.size());
    assert(mesh.tangents.size() == mesh.positions.size());
    auto inconsistent = mesh;
    inconsistent.vertices.pop_back();
    assert(!inconsistent.valid());
    auto nonfinite_tangent = mesh;
    nonfinite_tangent.tangents.front().x = std::numeric_limits<float>::quiet_NaN();
    assert(!nonfinite_tangent.valid());
    auto nonfinite_color = mesh;
    nonfinite_color.colors.front().a = std::numeric_limits<float>::infinity();
    assert(!nonfinite_color.valid());

    const auto transformed=transform(mesh,{{2,0,0},{},{2,1,1}});
    assert(transformed && transformed.value().positions.size()==mesh.positions.size());
    assert(transformed.value().indices==mesh.indices);
    assert(transformed.value().submeshes.size()==mesh.submeshes.size());
    assert(transformed.value().submeshes[0].first_index==mesh.submeshes[0].first_index);
    assert(transformed.value().submeshes[0].index_count==mesh.submeshes[0].index_count);

    mesh.normals.pop_back();
    assert(!mesh.valid());
    return 0;
}
