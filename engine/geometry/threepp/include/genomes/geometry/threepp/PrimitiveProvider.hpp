#pragma once
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/geometry/MeshData.hpp>

#include <cstdint>

namespace genomes::geometry::threepp_provider {

[[nodiscard]] foundation::Result<MeshData, foundation::Error> box(foundation::Vec3 size);
[[nodiscard]] foundation::Result<MeshData, foundation::Error> sphere(
    float radius, std::uint32_t width_segments=16U, std::uint32_t height_segments=12U);
[[nodiscard]] foundation::Result<MeshData, foundation::Error> cylinder(
    float radius_top, float radius_bottom, float height, std::uint32_t radial_segments=16U);
[[nodiscard]] foundation::Result<MeshData, foundation::Error> capsule(
    float radius, float length, std::uint32_t cap_segments=8U,
    std::uint32_t radial_segments=16U);

} // namespace genomes::geometry::threepp_provider
