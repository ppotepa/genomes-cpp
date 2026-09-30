#pragma once

#include <genomes/math/Vec.hpp>
#include <cmath>

namespace genomes::math {

struct Quat final {
    float x{0}, y{0}, z{0}, w{1};
    static Quat identity() noexcept { return {}; }
    static bool axisAngle(Vec3 axis, float angle, Quat& result) noexcept {
        if (!normalize(axis)) return false;
        const float half=angle*0.5F, s=std::sin(half);
        result={axis.x*s,axis.y*s,axis.z*s,std::cos(half)}; return true;
    }
    bool normalizeSelf(float epsilon=1e-8F) noexcept {
        const float n=std::sqrt(x*x+y*y+z*z+w*w);
        if (!(n>epsilon)||!std::isfinite(n)) return false;
        x/=n; y/=n; z/=n; w/=n; return true;
    }
    [[nodiscard]] Quat inverse() const noexcept { return {-x,-y,-z,w}; }
    [[nodiscard]] Vec3 rotate(Vec3 v) const noexcept {
        const Vec3 q{x,y,z}, t=2.0F*cross(q,v); return v+w*t+cross(q,t);
    }
};
inline Quat operator*(Quat a, Quat b) noexcept { return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z}; }

} // namespace genomes::math
