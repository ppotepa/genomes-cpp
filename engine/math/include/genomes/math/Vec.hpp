#pragma once

#include <cmath>

namespace genomes::math {

struct Vec2 { float x{0}; float y{0}; };
struct Vec3 { float x{0}; float y{0}; float z{0}; };
struct Vec4 { float x{0}; float y{0}; float z{0}; float w{0}; };

constexpr Vec2 operator+(Vec2 a, Vec2 b) noexcept { return {a.x+b.x,a.y+b.y}; }
constexpr Vec2 operator-(Vec2 a, Vec2 b) noexcept { return {a.x-b.x,a.y-b.y}; }
constexpr Vec2 operator*(Vec2 a, float s) noexcept { return {a.x*s,a.y*s}; }
constexpr Vec3 operator+(Vec3 a, Vec3 b) noexcept { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
constexpr Vec3 operator-(Vec3 a, Vec3 b) noexcept { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
constexpr Vec3 operator-(Vec3 a) noexcept { return {-a.x,-a.y,-a.z}; }
constexpr Vec3 operator*(Vec3 a, float s) noexcept { return {a.x*s,a.y*s,a.z*s}; }
constexpr Vec3 operator*(float s, Vec3 a) noexcept { return a*s; }
constexpr Vec3 operator/(Vec3 a, float s) noexcept { return a*(1.0F/s); }
constexpr Vec4 operator+(Vec4 a, Vec4 b) noexcept { return {a.x+b.x,a.y+b.y,a.z+b.z,a.w+b.w}; }
constexpr Vec4 operator*(Vec4 a, float s) noexcept { return {a.x*s,a.y*s,a.z*s,a.w*s}; }
constexpr float dot(Vec2 a, Vec2 b) noexcept { return a.x*b.x+a.y*b.y; }
constexpr float dot(Vec3 a, Vec3 b) noexcept { return a.x*b.x+a.y*b.y+a.z*b.z; }
constexpr float dot(Vec4 a, Vec4 b) noexcept { return a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w; }
constexpr Vec3 cross(Vec3 a, Vec3 b) noexcept { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
constexpr float lengthSquared(Vec2 v) noexcept { return dot(v,v); }
constexpr float lengthSquared(Vec3 v) noexcept { return dot(v,v); }
inline float length(Vec2 v) noexcept { return std::sqrt(lengthSquared(v)); }
inline float length(Vec3 v) noexcept { return std::sqrt(lengthSquared(v)); }
inline bool finite(Vec2 v) noexcept { return std::isfinite(v.x)&&std::isfinite(v.y); }
inline bool finite(Vec3 v) noexcept { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
inline bool finite(Vec4 v) noexcept { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.w); }
inline bool normalize(Vec2& v, float epsilon=1e-8F) noexcept { const float n=length(v); if (!(n>epsilon)||!std::isfinite(n)) return false; v=v/n; return true; }
inline bool normalize(Vec3& v, float epsilon=1e-8F) noexcept { const float n=length(v); if (!(n>epsilon)||!std::isfinite(n)) return false; v=v/n; return true; }
inline Vec2 normalized(Vec2 v, bool* ok=nullptr) noexcept { const bool valid=normalize(v); if(ok)*ok=valid; return valid?v:Vec2{}; }
inline Vec3 normalized(Vec3 v, bool* ok=nullptr) noexcept { const bool valid=normalize(v); if(ok)*ok=valid; return valid?v:Vec3{}; }

} // namespace genomes::math
