#include <genomes/terrain/HeightField.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace genomes::terrain {

HeightField::HeightField(std::uint32_t width,
                         std::uint32_t height,
                         double origin_x,
                         double origin_z,
                         float cell_size_m)
    : width_(width),
      height_(height),
      origin_x_(origin_x),
      origin_z_(origin_z),
      cell_size_m_(cell_size_m),
      samples_(static_cast<std::size_t>(width) * height, 0.0F) {}

foundation::Result<HeightField, foundation::Error> HeightField::create(const TerrainSpec& spec) {
    if (!spec.valid()) {
        return foundation::Result<HeightField, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid terrain specification"});
    }
    const world::WorldPosition region_origin = world::regionOrigin(spec.region, spec.coordinates);
    return foundation::Result<HeightField, foundation::Error>::success(
        HeightField(spec.samples_x, spec.samples_z, region_origin.x + spec.origin_offset_x,
                    region_origin.z + spec.origin_offset_z, spec.cell_size_m));
}

std::uint32_t HeightField::clampX(int value) const noexcept {
    return static_cast<std::uint32_t>(std::clamp(value, 0, static_cast<int>(width_ - 1)));
}

std::uint32_t HeightField::clampZ(int value) const noexcept {
    return static_cast<std::uint32_t>(std::clamp(value, 0, static_cast<int>(height_ - 1)));
}

float HeightField::sampleNearest(double x, double z) const noexcept {
    const int ix = static_cast<int>(std::lround((x - origin_x_) / cell_size_m_));
    const int iz = static_cast<int>(std::lround((z - origin_z_) / cell_size_m_));
    return at(clampX(ix), clampZ(iz));
}

float HeightField::sampleBilinear(double x, double z) const noexcept {
    const double gx = (x - origin_x_) / cell_size_m_;
    const double gz = (z - origin_z_) / cell_size_m_;
    const int ix = static_cast<int>(std::floor(gx));
    const int iz = static_cast<int>(std::floor(gz));
    const double fx = gx - static_cast<double>(ix);
    const double fz = gz - static_cast<double>(iz);
    const std::uint32_t x0 = clampX(ix);
    const std::uint32_t z0 = clampZ(iz);
    const std::uint32_t x1 = clampX(ix + 1);
    const std::uint32_t z1 = clampZ(iz + 1);
    const double h0 = std::lerp(static_cast<double>(at(x0, z0)),
                                static_cast<double>(at(x1, z0)), fx);
    const double h1 = std::lerp(static_cast<double>(at(x0, z1)),
                                static_cast<double>(at(x1, z1)), fx);
    return static_cast<float>(std::lerp(h0, h1, fz));
}

foundation::Vec3 HeightField::normal(double x, double z) const noexcept {
    const double step = static_cast<double>(cell_size_m_);
    const double dx = (sampleBilinear(x + step, z) - sampleBilinear(x - step, z)) /
                      (2.0 * step);
    const double dz = (sampleBilinear(x, z + step) - sampleBilinear(x, z - step)) /
                      (2.0 * step);
    const double length = std::sqrt(dx * dx + 1.0 + dz * dz);
    return {static_cast<float>(-dx / length), static_cast<float>(1.0 / length),
            static_cast<float>(-dz / length)};
}

} // namespace genomes::terrain
