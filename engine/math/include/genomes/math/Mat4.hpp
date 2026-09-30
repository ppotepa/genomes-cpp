#pragma once

#include <genomes/math/Vec.hpp>
#include <array>
#include <cstddef>
#include <optional>

namespace genomes::math {

struct Mat4 final {
    std::array<float,16> m{};
    constexpr float& operator()(int row,int column) noexcept { return m[static_cast<std::size_t>(column*4+row)]; }
    constexpr float operator()(int row,int column) const noexcept { return m[static_cast<std::size_t>(column*4+row)]; }
    static constexpr Mat4 identity() noexcept { Mat4 r{}; r(0,0)=r(1,1)=r(2,2)=r(3,3)=1; return r; }
    [[nodiscard]] bool finite() const noexcept;
    [[nodiscard]] std::optional<Mat4> inverse() const noexcept;
    [[nodiscard]] std::optional<Mat4> rigidInverse() const noexcept;
};
Mat4 operator*(const Mat4&,const Mat4&) noexcept;
Vec4 operator*(const Mat4&,Vec4) noexcept;
Vec3 transformPoint(const Mat4&,Vec3) noexcept;
Vec3 transformVector(const Mat4&,Vec3) noexcept;
Mat4 lookAtRH(Vec3 eye,Vec3 target,Vec3 up) noexcept;
Mat4 perspectiveD3D(float verticalFov,float aspect,float nearPlane,float farPlane) noexcept;

} // namespace genomes::math
