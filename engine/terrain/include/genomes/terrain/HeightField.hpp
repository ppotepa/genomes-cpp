#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/terrain/TerrainSpec.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace genomes::terrain {

class HeightField final {
public:
    static foundation::Result<HeightField, foundation::Error> create(const TerrainSpec& spec);

    [[nodiscard]] std::uint32_t width() const noexcept {
        return width_;
    }

    [[nodiscard]] std::uint32_t height() const noexcept {
        return height_;
    }

    [[nodiscard]] float& at(std::uint32_t x, std::uint32_t z) noexcept {
        return samples_[static_cast<std::size_t>(z) * width_ + x];
    }

    [[nodiscard]] float at(std::uint32_t x, std::uint32_t z) const noexcept {
        return samples_[static_cast<std::size_t>(z) * width_ + x];
    }

    [[nodiscard]] float sampleNearest(double x, double z) const noexcept;
    [[nodiscard]] float sampleBilinear(double x, double z) const noexcept;
    [[nodiscard]] foundation::Vec3 normal(double x, double z) const noexcept;

    [[nodiscard]] double originX() const noexcept {
        return origin_x_;
    }

    [[nodiscard]] double originZ() const noexcept {
        return origin_z_;
    }

    [[nodiscard]] float cellSize() const noexcept {
        return cell_size_m_;
    }

    [[nodiscard]] std::span<const float> samples() const noexcept { return samples_; }

private:
    HeightField(std::uint32_t width,
                std::uint32_t height,
                double origin_x,
                double origin_z,
                float cell_size_m);

    [[nodiscard]] std::uint32_t clampX(int value) const noexcept;
    [[nodiscard]] std::uint32_t clampZ(int value) const noexcept;

    std::uint32_t width_{0};
    std::uint32_t height_{0};
    double origin_x_{0.0};
    double origin_z_{0.0};
    float cell_size_m_{1.0F};
    std::vector<float> samples_;
};

} // namespace genomes::terrain
