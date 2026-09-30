#pragma once
#include <genomes/math/Mat4.hpp>
#include <genomes/math/Quat.hpp>

namespace genomes::math {
struct Transform final {
    Vec3 translation{}; Quat rotation{}; Vec3 scale{1,1,1};
    [[nodiscard]] bool valid() const noexcept { return finite(translation)&&finite(scale)&&scale.x!=0&&scale.y!=0&&scale.z!=0; }
    [[nodiscard]] Mat4 matrix() const noexcept;
    [[nodiscard]] std::optional<Transform> inverse() const noexcept;
};
Transform operator*(const Transform&,const Transform&) noexcept;
}
