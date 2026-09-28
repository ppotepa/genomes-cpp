#pragma once

#include <genomes/foundation/Types.hpp>

#include <cstdint>

namespace genomes::geometry {

struct EllipsoidSpec final {
    foundation::Vec3 center{};
    foundation::Vec3 radii{1.0F, 1.0F, 1.0F};
    std::uint32_t longitude_segments{16U};
    std::uint32_t latitude_segments{8U};
};

[[nodiscard]] foundation::Vec3 ellipsoidPoint(const EllipsoidSpec& spec,
                                              float latitude,
                                              float longitude) noexcept;
[[nodiscard]] foundation::Vec3 ellipsoidNormal(float latitude,
                                               float longitude) noexcept;

} // namespace genomes::geometry
