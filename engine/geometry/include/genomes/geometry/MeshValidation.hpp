#pragma once

#include <genomes/foundation/Types.hpp>

#include <cstdint>
#include <span>

namespace genomes::geometry {

struct MeshValidationReport final {
    std::uint64_t triangle_count{0};
    std::uint64_t degenerate_triangle_count{0};
    std::uint64_t nonfinite_vertex_count{0};
    std::uint64_t invalid_index_count{0};
    std::uint64_t winding_mismatch_count{0};

    [[nodiscard]] bool valid() const noexcept {
        return degenerate_triangle_count == 0U && nonfinite_vertex_count == 0U &&
               invalid_index_count == 0U && winding_mismatch_count == 0U;
    }
};

[[nodiscard]] MeshValidationReport validateMesh(
    std::span<const foundation::Vec3> positions,
    std::span<const foundation::Vec3> normals,
    std::span<const std::uint32_t> indices,
    float area_epsilon = 1.0e-8F,
    float winding_epsilon = 1.0e-5F) noexcept;

} // namespace genomes::geometry
