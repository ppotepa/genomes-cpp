#include <genomes/geometry/MeshBuilder.hpp>
#include <genomes/geometry/MeshValidation.hpp>

#include <cassert>
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
    return 0;
}
