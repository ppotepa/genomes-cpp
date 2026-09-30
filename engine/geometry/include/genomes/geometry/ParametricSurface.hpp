#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/geometry/MeshData.hpp>

#include <cstdint>
#include <functional>

namespace genomes::geometry {

struct EllipsoidSpec final {
    foundation::Vec3 center{};
    foundation::Vec3 radii{1.0F, 1.0F, 1.0F};
    std::uint32_t longitude_segments{16U};
    std::uint32_t latitude_segments{8U};
};

struct ParametricSurfaceSpec final {
    float u_min{0};
    float u_max{1};
    float v_min{0};
    float v_max{1};
    std::uint32_t u_segments{1};
    std::uint32_t v_segments{1};
    std::function<foundation::Vec3(float,float)> position;
};

[[nodiscard]] foundation::Result<MeshData, foundation::Error>
tessellateParametric(const ParametricSurfaceSpec&);

[[nodiscard]] foundation::Vec3 ellipsoidPoint(const EllipsoidSpec& spec,
                                              float latitude,
                                              float longitude) noexcept;
[[nodiscard]] foundation::Vec3 ellipsoidNormal(float latitude,
                                               float longitude) noexcept;

} // namespace genomes::geometry
