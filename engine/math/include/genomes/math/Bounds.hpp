#pragma once
#include <genomes/math/Mat4.hpp>
#include <array>

namespace genomes::math {
struct Aabb final {
    Vec3 min{0,0,0}, max{0,0,0}; bool empty{true};
    void include(Vec3 p) noexcept;
    [[nodiscard]] Vec3 center() const noexcept { return (min+max)*0.5F; }
    [[nodiscard]] Vec3 extent() const noexcept { return (max-min)*0.5F; }
    [[nodiscard]] Aabb transformed(const Mat4&) const noexcept;
};
struct Sphere final { Vec3 center{}; float radius{0}; };
struct Plane final { Vec3 normal{}; float distance{0}; float signedDistance(Vec3 p) const noexcept { return dot(normal,p)+distance; } };
struct Frustum final { std::array<Plane,6> planes{}; [[nodiscard]] bool contains(Vec3) const noexcept; };
}
