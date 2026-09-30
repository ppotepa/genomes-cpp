#include <genomes/geometry/ParametricSurface.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

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

foundation::Result<MeshData, foundation::Error>
tessellateParametric(const ParametricSurfaceSpec& spec) {
    using Result = foundation::Result<MeshData, foundation::Error>;
    if (!spec.position || spec.u_segments == 0U || spec.v_segments == 0U ||
        !std::isfinite(spec.u_min) || !std::isfinite(spec.u_max) ||
        !std::isfinite(spec.v_min) || !std::isfinite(spec.v_max) ||
        spec.u_max <= spec.u_min || spec.v_max <= spec.v_min)
        return Result::failure({foundation::ErrorCode::InvalidArgument,
                                "parametric surface domain or sampling is invalid"});

    constexpr std::size_t max_vertices = 4'000'000U;
    constexpr std::size_t max_indices = 24'000'000U;
    const std::size_t width = static_cast<std::size_t>(spec.u_segments) + 1U;
    const std::size_t height = static_cast<std::size_t>(spec.v_segments) + 1U;
    if (width > max_vertices || height > max_vertices / width || width * height > max_vertices)
        return Result::failure({foundation::ErrorCode::OutOfRange,
                                "parametric sampling exceeds vertex limits"});
    if (static_cast<std::size_t>(spec.v_segments) > max_indices / 6U ||
        static_cast<std::size_t>(spec.u_segments) >
            max_indices / (6U * static_cast<std::size_t>(spec.v_segments)) ||
        width > std::numeric_limits<std::uint32_t>::max())
        return Result::failure({foundation::ErrorCode::OutOfRange,
                                "parametric sampling exceeds index limits"});
    const std::size_t cells = static_cast<std::size_t>(spec.u_segments) * spec.v_segments;

    MeshData mesh{};
    mesh.vertices.resize(width * height);
    const float du = (spec.u_max - spec.u_min) /
                     static_cast<float>(spec.u_segments) * 0.5F;
    const float dv = (spec.v_max - spec.v_min) /
                     static_cast<float>(spec.v_segments) * 0.5F;
    for (std::uint32_t v = 0U; v <= spec.v_segments; ++v) {
        for (std::uint32_t u = 0U; u <= spec.u_segments; ++u) {
            const float fu = static_cast<float>(u) / static_cast<float>(spec.u_segments);
            const float fv = static_cast<float>(v) / static_cast<float>(spec.v_segments);
            const float U = spec.u_min + fu * (spec.u_max - spec.u_min);
            const float V = spec.v_min + fv * (spec.v_max - spec.v_min);
            const auto p = spec.position(U, V);
            const auto pu = spec.position(std::min(spec.u_max, U + du), V) -
                            spec.position(std::max(spec.u_min, U - du), V);
            const auto pv = spec.position(U, std::min(spec.v_max, V + dv)) -
                            spec.position(U, std::max(spec.v_min, V - dv));
            math::Vec3 normal = math::cross(pu, pv);
            if (!math::finite(p) || !math::finite(pu) || !math::finite(pv) ||
                !math::normalize(normal) || !math::finite(normal))
                return Result::failure({foundation::ErrorCode::InvalidArgument,
                                        "parametric callback produced invalid or degenerate sample"});
            mesh.vertices[static_cast<std::size_t>(v) * width + u] =
                {{p.x, p.y, p.z}, {normal.x, normal.y, normal.z}, {fu, fv}};
        }
    }
    mesh.indices.reserve(cells * 6U);
    for (std::uint32_t v = 0U; v < spec.v_segments; ++v) {
        for (std::uint32_t u = 0U; u < spec.u_segments; ++u) {
            const auto base = static_cast<std::uint32_t>(static_cast<std::size_t>(v) * width + u);
            const auto row = static_cast<std::uint32_t>(width);
            mesh.indices.insert(mesh.indices.end(),
                                {base, base + 1U, base + row,
                                 base + 1U, base + row + 1U, base + row});
        }
    }
    mesh.rebuildStreams();
    mesh.submeshes.push_back({0U, static_cast<std::uint32_t>(mesh.indices.size()), 0U});
    if (!mesh.valid())
        return Result::failure({foundation::ErrorCode::InvalidState,
                                "parametric tessellation produced invalid mesh data"});
    return Result::success(std::move(mesh));
}

} // namespace genomes::geometry
