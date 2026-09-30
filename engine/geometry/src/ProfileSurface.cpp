#include <genomes/geometry/ProfileSurface.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace genomes::geometry {
namespace {

[[nodiscard]] foundation::Error invalid(const char* message) {
    return {foundation::ErrorCode::InvalidArgument, message};
}

[[nodiscard]] bool normalize_checked(foundation::Vec3 value,
                                     foundation::Vec3& result) noexcept {
    result = value;
    return math::normalize(result);
}

} // namespace

foundation::Result<MeshData, foundation::Error> sweepProfile(const SweepSpec& spec) {
    using Result = foundation::Result<MeshData, foundation::Error>;
    if (spec.centers.size() < 2U || spec.centers.size() != spec.radii.size() ||
        spec.profile_segments < 3U)
        return Result::failure(invalid("sweep requires matching path/radius samples and three profile segments"));
    if (spec.closed && spec.centers.size() < 3U)
        return Result::failure(invalid("closed sweep requires three path samples"));
    for (std::size_t index = 0U; index < spec.centers.size(); ++index) {
        if (!math::finite(spec.centers[index]) || !math::finite(spec.radii[index]) ||
            spec.radii[index].x <= 0.0F || spec.radii[index].y <= 0.0F)
            return Result::failure(invalid("sweep contains invalid path or radius"));
    }

    const std::size_t count = spec.centers.size();
    const auto tangent_at = [&](std::size_t path, foundation::Vec3& tangent) {
        const std::size_t previous = path == 0U ? (spec.closed ? count - 1U : 0U) : path - 1U;
        const std::size_t next = path + 1U == count ? (spec.closed ? 0U : count - 1U) : path + 1U;
        foundation::Vec3 direction{};
        if (!spec.closed && path == 0U)
            direction = spec.centers[1U] - spec.centers[0U];
        else if (!spec.closed && path + 1U == count)
            direction = spec.centers[count - 1U] - spec.centers[count - 2U];
        else
            direction = spec.centers[next] - spec.centers[previous];
        return normalize_checked(direction, tangent);
    };

    std::vector<foundation::Vec3> tangents(count);
    for (std::size_t path = 0U; path < count; ++path) {
        if (!tangent_at(path, tangents[path]))
            return Result::failure(invalid("sweep path contains a zero-length tangent"));
    }

    foundation::Vec3 axis_u{};
    const foundation::Vec3 world_up{0.0F, 1.0F, 0.0F};
    const foundation::Vec3 world_side{1.0F, 0.0F, 0.0F};
    const auto choose_axis = [&](foundation::Vec3 tangent, foundation::Vec3& axis) {
        const foundation::Vec3 reference = std::fabs(math::dot(world_up, tangent)) < 0.9F
                                               ? world_up
                                               : world_side;
        return normalize_checked(math::cross(reference, tangent), axis);
    };
    if (!choose_axis(tangents.front(), axis_u))
        return Result::failure(invalid("sweep initial frame is degenerate"));

    MeshBuilder builder;
    std::vector<Ring> rings;
    std::vector<foundation::Vec3> frame_axis_u;
    rings.reserve(count);
    frame_axis_u.reserve(count);
    for (std::size_t path = 0U; path < count; ++path) {
        if (path > 0U) {
            foundation::Vec3 transported = axis_u -
                tangents[path] * math::dot(axis_u, tangents[path]);
            if (!math::normalize(transported) && !choose_axis(tangents[path], transported))
                return Result::failure(invalid("sweep parallel-transport frame is degenerate"));
            axis_u = transported;
        }
        foundation::Vec3 axis_v{};
        if (!normalize_checked(math::cross(tangents[path], axis_u), axis_v))
            return Result::failure(invalid("sweep frame is degenerate"));
        frame_axis_u.push_back(axis_u);
        rings.push_back(appendProfileRing(
            builder,
            {spec.centers[path], axis_u, axis_v, spec.radii[path].x,
             spec.radii[path].y,
             spec.profile_segments,
             static_cast<float>(path) /
                 static_cast<float>(std::max<std::size_t>(1U, count - 1U))},
            [](MeshBuilder& target, foundation::Vec3 position, foundation::Vec3 normal,
               foundation::Vec2 uv) { return target.appendVertex({position, normal, uv}); }));
    }

    for (std::size_t path = 0U; path + 1U < count; ++path)
        bridgeProfileRings(builder, rings[path], rings[path + 1U], {true, true});
    if (spec.closed)
        bridgeProfileRings(builder, rings.back(), rings.front(), {true, true});

    if (spec.cap && !spec.closed) {
        const auto append_cap = [&](std::size_t path, foundation::Vec3 direction,
                                    bool start) {
            const foundation::Vec3 tangent = tangents[path];
            foundation::Vec3 cap_axis_u = frame_axis_u[path] -
                tangent * math::dot(frame_axis_u[path], tangent);
            if (!math::normalize(cap_axis_u)) cap_axis_u = frame_axis_u[path];
            foundation::Vec3 cap_axis_v = math::normalized(math::cross(tangent, cap_axis_u));
            const auto center = builder.appendVertex({spec.centers[path], direction, {0.5F, 0.5F}});
            std::vector<MeshBuilder::VertexIndex> cap_ring;
            cap_ring.reserve(spec.profile_segments);
            for (std::uint32_t segment = 0U; segment < spec.profile_segments; ++segment) {
                const float u = static_cast<float>(segment) /
                                static_cast<float>(spec.profile_segments);
                const float theta = u * 6.2831853071795864769F;
                const foundation::Vec3 position = spec.centers[path] +
                    cap_axis_u * (spec.radii[path].x * std::cos(theta)) +
                    cap_axis_v * (spec.radii[path].y * std::sin(theta));
                cap_ring.push_back(builder.appendVertex({position, direction, {0.5F + 0.5F * std::cos(theta),
                                                                                0.5F + 0.5F * std::sin(theta)}}));
            }
            for (std::uint32_t segment = 0U; segment < spec.profile_segments; ++segment) {
                const auto next = (segment + 1U) % spec.profile_segments;
                if (start)
                    builder.appendTriangle(center, cap_ring[next], cap_ring[segment]);
                else
                    builder.appendTriangle(center, cap_ring[segment], cap_ring[next]);
            }
        };
        append_cap(0U, -tangents.front(), true);
        append_cap(count - 1U, tangents.back(), false);
    }

    auto result = builder.build();
    if (!result) return result;
    return Result::success(std::move(result.value()));
}

} // namespace genomes::geometry
