#include <genomes/geometry/MeshBuilder.hpp>
#include <genomes/geometry/MeshValidation.hpp>
#include <genomes/geometry/PrimitiveBuilder.hpp>

#include <cassert>
#include <limits>
#include <vector>

int main() {
    using namespace genomes::geometry;
    MeshBuilder builder;
    (void)builder.appendPosition({0.0F, 0.0F, 0.0F});
    (void)builder.appendPosition({1.0F, 0.0F, 0.0F});
    (void)builder.appendPosition({0.0F, 1.0F, 0.0F});
    builder.appendTriangle(0U, 1U, 2U);
    const std::vector<genomes::foundation::Vec3> normals{
        {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F}};
    const auto valid = validateMesh(builder.positions(), normals, builder.indices());
    assert(valid.triangle_count == 1U);
    assert(valid.valid());

    builder.appendTriangle(0U, 2U, 1U);
    const auto inverted = validateMesh(builder.positions(), normals, builder.indices());
    assert(inverted.winding_mismatch_count == 1U);

    const std::vector<std::uint32_t> bad_indices{0U, 1U, 99U};
    const auto invalid = validateMesh(builder.positions(), normals, bad_indices);
    assert(invalid.invalid_index_count == 1U);
    auto box=makeBox({{1,1,1}});
    const auto mesh_report=validateMesh(box);
    assert(mesh_report.valid() && mesh_report.stream_mismatch_count==0U);
    box.uvs.pop_back();
    const auto mismatch=validateMesh(box);
    assert(mismatch.stream_mismatch_count==1U && !mismatch.valid());
    auto attributed = makeBox({{1,1,1}});
    attributed.tangents.resize(attributed.positions.size(), {1,0,0,1});
    attributed.colors.resize(attributed.positions.size(), {1,1,1,1});
    assert(validateMesh(attributed).valid());
    attributed.tangents.front().z = std::numeric_limits<float>::quiet_NaN();
    const auto nonfinite_tangent = validateMesh(attributed);
    assert(nonfinite_tangent.nonfinite_vertex_count != 0U && !nonfinite_tangent.valid());
    attributed = makeBox({{1,1,1}});
    attributed.colors.resize(attributed.positions.size(), {1,1,1,1});
    attributed.colors.front().a = std::numeric_limits<float>::infinity();
    const auto nonfinite_color = validateMesh(attributed);
    assert(nonfinite_color.nonfinite_vertex_count != 0U && !nonfinite_color.valid());
    return 0;
}
