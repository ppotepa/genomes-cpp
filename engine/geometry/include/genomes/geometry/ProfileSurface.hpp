#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/geometry/MeshBuilder.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

namespace genomes::geometry {

struct ProfileFrame final {
    float s{0.0F};
    foundation::Vec3 center{};
    foundation::Vec3 axis_u{1.0F, 0.0F, 0.0F};
    foundation::Vec3 axis_v{0.0F, 0.0F, 1.0F};
    float radius_u{0.0F};
    float radius_v{0.0F};
};

struct ProfileRingSpec final {
    foundation::Vec3 center{};
    foundation::Vec3 axis_u{1.0F, 0.0F, 0.0F};
    foundation::Vec3 axis_v{0.0F, 0.0F, 1.0F};
    float radius_u{0.0F};
    float radius_v{0.0F};
    std::uint32_t segments{8U};
    float uv_v{0.0F};
};

struct SweepSpec final {
    std::vector<foundation::Vec3> centers;
    std::vector<foundation::Vec2> radii;
    std::uint32_t profile_segments{16U};
    bool closed{false};
    bool cap{false};
};

[[nodiscard]] foundation::Result<MeshData, foundation::Error>
sweepProfile(const SweepSpec&);

template <class VertexFactory>
[[nodiscard]] Ring appendProfileRing(MeshBuilder& topology,
                                     const ProfileRingSpec& spec,
                                     VertexFactory&& vertex_factory) {
    Ring ring{};
    if (spec.segments < 3U || spec.radius_u <= 0.0F || spec.radius_v <= 0.0F) {
        return ring;
    }
    ring.indices.reserve(spec.segments);
    constexpr float kTau = 6.2831853071795864769F;
    for (std::uint32_t index = 0U; index < spec.segments; ++index) {
        const float theta = kTau * static_cast<float>(index) /
                            static_cast<float>(spec.segments);
        const float c = std::cos(theta);
        const float s = std::sin(theta);
        const foundation::Vec3 position{
            spec.center.x + spec.axis_u.x * spec.radius_u * c +
                spec.axis_v.x * spec.radius_v * s,
            spec.center.y + spec.axis_u.y * spec.radius_u * c +
                spec.axis_v.y * spec.radius_v * s,
            spec.center.z + spec.axis_u.z * spec.radius_u * c +
                spec.axis_v.z * spec.radius_v * s};
        foundation::Vec3 normal = math::normalized(
            spec.axis_u * (c / spec.radius_u) + spec.axis_v * (s / spec.radius_v));
        const foundation::Vec2 uv{
            static_cast<float>(index) / static_cast<float>(spec.segments), spec.uv_v};
        ring.indices.push_back(vertex_factory(topology, position, normal, uv));
    }
    return ring;
}

inline void bridgeProfileRings(MeshBuilder& topology, const Ring& first,
                               const Ring& second, BridgeOptions options = {}) {
    topology.bridgeLoops(first.indices, second.indices, options);
}

} // namespace genomes::geometry
