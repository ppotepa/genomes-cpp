#include <genomes/geometry/ParametricSurface.hpp>

#include <cmath>

namespace genomes::geometry {

foundation::Vec3 ellipsoidPoint(const EllipsoidSpec& spec, float latitude,
                                float longitude) noexcept {
    const float cp = std::cos(latitude);
    return {spec.center.x + spec.radii.x * cp * std::cos(longitude),
            spec.center.y + spec.radii.y * std::sin(latitude),
            spec.center.z + spec.radii.z * cp * std::sin(longitude)};
}

foundation::Vec3 ellipsoidNormal(float latitude, float longitude) noexcept {
    const float cp = std::cos(latitude);
    return {cp * std::cos(longitude), std::sin(latitude), cp * std::sin(longitude)};
}

} // namespace genomes::geometry
