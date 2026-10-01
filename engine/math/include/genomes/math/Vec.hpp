#pragma once

#include <cmath>
#include <type_traits>

namespace genomes::math {

struct Vec2 { float x{0}; float y{0}; };
struct Vec3 { float x{0}; float y{0}; float z{0}; };
struct Vec4 { float x{0}; float y{0}; float z{0}; float w{0}; };

constexpr Vec2 operator+(Vec2 a, Vec2 b) noexcept { return {a.x+b.x,a.y+b.y}; }
constexpr Vec2 operator-(Vec2 a, Vec2 b) noexcept { return {a.x-b.x,a.y-b.y}; }
constexpr Vec2 operator*(Vec2 a, float s) noexcept { return {a.x*s,a.y*s}; }
constexpr Vec2 operator/(Vec2 a, float s) noexcept { return a*(1.0F/s); }
constexpr Vec3 operator+(Vec3 a, Vec3 b) noexcept { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
constexpr Vec3 operator-(Vec3 a, Vec3 b) noexcept { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
constexpr Vec3 operator-(Vec3 a) noexcept { return {-a.x,-a.y,-a.z}; }
constexpr Vec3 operator*(Vec3 a, float s) noexcept { return {a.x*s,a.y*s,a.z*s}; }
constexpr Vec3 operator*(float s, Vec3 a) noexcept { return a*s; }
constexpr Vec3 operator/(Vec3 a, float s) noexcept { return a*(1.0F/s); }
constexpr Vec4 operator+(Vec4 a, Vec4 b) noexcept { return {a.x+b.x,a.y+b.y,a.z+b.z,a.w+b.w}; }
constexpr Vec4 operator*(Vec4 a, float s) noexcept { return {a.x*s,a.y*s,a.z*s,a.w*s}; }
template <typename V> requires std::is_same_v<V, Vec2>
constexpr float dot(V a, V b) noexcept { return a.x*b.x+a.y*b.y; }
template <typename V> requires std::is_same_v<V, Vec3>
constexpr float dot(V a, V b) noexcept { return a.x*b.x+a.y*b.y+a.z*b.z; }
template <typename V> requires std::is_same_v<V, Vec4>
constexpr float dot(V a, V b) noexcept { return a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w; }
template <typename V> requires std::is_same_v<V, Vec3>
constexpr Vec3 cross(V a, V b) noexcept { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
template <typename V> requires std::is_same_v<V, Vec2>
constexpr float lengthSquared(V v) noexcept { return dot(v,v); }
template <typename V> requires std::is_same_v<V, Vec3>
constexpr float lengthSquared(V v) noexcept { return dot(v,v); }
template <typename V> requires (std::is_same_v<V, Vec2> || std::is_same_v<V, Vec3>)
inline float length(V v) noexcept { return std::sqrt(lengthSquared(v)); }
template <typename V> requires std::is_same_v<V, Vec2>
inline bool finite(V v) noexcept { return std::isfinite(v.x)&&std::isfinite(v.y); }
template <typename V> requires std::is_same_v<V, Vec3>
inline bool finite(V v) noexcept { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
template <typename V> requires std::is_same_v<V, Vec4>
inline bool finite(V v) noexcept { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.w); }
template <typename V> requires std::is_same_v<V, Vec2>
inline bool normalize(V& v, float epsilon=1e-8F) noexcept { const float n=length(v); if (!(n>epsilon)||!std::isfinite(n)) return false; v=v/n; return true; }
template <typename V> requires std::is_same_v<V, Vec3>
inline bool normalize(V& v, float epsilon=1e-8F) noexcept { const float n=length(v); if (!(n>epsilon)||!std::isfinite(n)) return false; v=v/n; return true; }
template <typename V> requires std::is_same_v<V, Vec2>
inline Vec2 normalized(V v, bool* ok=nullptr) noexcept { const bool valid=normalize(v); if(ok)*ok=valid; return valid?v:Vec2{}; }
template <typename V> requires std::is_same_v<V, Vec3>
inline Vec3 normalized(V v, bool* ok=nullptr) noexcept { const bool valid=normalize(v); if(ok)*ok=valid; return valid?v:Vec3{}; }

} // namespace genomes::math
