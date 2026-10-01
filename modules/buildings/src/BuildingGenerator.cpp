#include <genomes/buildings/BuildingModel.hpp>
#include <genomes/buildings/BuildingProfile.hpp>

#include <genomes/foundation/StableHash.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

namespace genomes::buildings {

namespace {

struct SiteBounds final {
    float min_x{0.0F};
    float min_z{0.0F};
    float max_x{0.0F};
    float max_z{0.0F};
};

[[nodiscard]] bool isSupportedSiteRectangle(const world::BuildingSiteRequest& request,
                                            SiteBounds& bounds) noexcept {
    constexpr float epsilon = 1.0e-4F;
    foundation::Vec2 local[4]{};
    bounds.min_x = std::numeric_limits<float>::max();
    bounds.min_z = std::numeric_limits<float>::max();
    bounds.max_x = std::numeric_limits<float>::lowest();
    bounds.max_z = std::numeric_limits<float>::lowest();
    const float cosine = std::cos(request.preferred_rotation);
    const float sine = std::sin(request.preferred_rotation);
    for (std::size_t index = 0; index < request.buildable_polygon.size(); ++index) {
        const foundation::Vec2 point = request.buildable_polygon[index];
        const float dx = point.x - request.preferred_position.x;
        const float dz = point.y - request.preferred_position.z;
        local[index] = {dx * cosine + dz * sine, -dx * sine + dz * cosine};
        bounds.min_x = std::min(bounds.min_x, local[index].x);
        bounds.min_z = std::min(bounds.min_z, local[index].y);
        bounds.max_x = std::max(bounds.max_x, local[index].x);
        bounds.max_z = std::max(bounds.max_z, local[index].y);
    }
    if (bounds.max_x - bounds.min_x <= epsilon || bounds.max_z - bounds.min_z <= epsilon ||
        std::abs(bounds.min_x + bounds.max_x) > epsilon ||
        std::abs(bounds.min_z + bounds.max_z) > epsilon) {
        return false;
    }
    for (std::size_t index = 0; index < 4; ++index) {
        const foundation::Vec2 point = local[index];
        const bool x_corner = std::abs(point.x - bounds.min_x) <= epsilon ||
                              std::abs(point.x - bounds.max_x) <= epsilon;
        const bool z_corner = std::abs(point.y - bounds.min_z) <= epsilon ||
                              std::abs(point.y - bounds.max_z) <= epsilon;
        if (!x_corner || !z_corner) {
            return false;
        }
        for (std::size_t other = 0; other < index; ++other) {
            if (std::abs(point.x - local[other].x) <= epsilon &&
                std::abs(point.y - local[other].y) <= epsilon) {
                return false;
            }
        }
        const foundation::Vec2 next = local[(index + 1U) % 4U];
        const bool horizontal = std::abs(point.y - next.y) <= epsilon &&
                                std::abs(point.x - next.x) > epsilon;
        const bool vertical = std::abs(point.x - next.x) <= epsilon &&
                              std::abs(point.y - next.y) > epsilon;
        if (!horizontal && !vertical) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] foundation::StableId part_id(const BuildingPartKey& key) noexcept {
    std::uint64_t value = foundation::stableHashCombine(key.building_id,
                                                        static_cast<std::uint32_t>(key.kind));
    value = foundation::stableHashCombine(value, static_cast<std::uint32_t>(key.role));
    value = foundation::stableHashCombine(value, key.floor);
    value = foundation::stableHashCombine(value, key.ordinal);
    return value == 0 ? 1 : value;
}

[[nodiscard]] foundation::StableId room_id(const BuildingSpec& spec, std::uint32_t floor,
                                           std::uint32_t ordinal) noexcept {
    std::uint64_t value = foundation::stableHashCombine(spec.building_id,
                                                        foundation::stableHashString("room"));
    value = foundation::stableHashCombine(value, floor);
    value = foundation::stableHashCombine(value, ordinal);
    return value == 0 ? 1 : value;
}

[[nodiscard]] std::uint64_t hash_spec(const BuildingSpec& spec) noexcept {
    std::uint64_t value = foundation::stableHashCombine(BuildingGeneratorVersion, spec.building_id);
    value = foundation::stableHashCombine(value, spec.seed);
    value = foundation::stableHashCombine(value, foundation::stableHashFloat(spec.footprint.x));
    value = foundation::stableHashCombine(value, foundation::stableHashFloat(spec.footprint.y));
    value = foundation::stableHashCombine(value, foundation::stableHashFloat(spec.footprint.z));
    value = foundation::stableHashCombine(value, spec.floors);
    value = foundation::stableHashCombine(value, foundation::stableHashFloat(spec.floor_height));
    value = foundation::stableHashCombine(value, foundation::stableHashFloat(spec.wall_thickness));
    return foundation::stableHashCombine(value, spec.rooms_per_floor);
}

[[nodiscard]] bool part_key_less(const BuildingPart& left, const BuildingPart& right) noexcept {
    if (left.key.floor != right.key.floor) {
        return left.key.floor < right.key.floor;
    }
    if (left.key.kind != right.key.kind) {
        return left.key.kind < right.key.kind;
    }
    if (left.key.role != right.key.role) {
        return left.key.role < right.key.role;
    }
    return left.key.ordinal < right.key.ordinal;
}

[[nodiscard]] std::uint64_t hash_room(const BuildingRoom& room) noexcept {
    std::uint64_t value = foundation::stableHashCombine(room.id, room.floor);
    value = foundation::stableHashCombine(value, foundation::stableHashFloat(room.center.x));
    value = foundation::stableHashCombine(value, foundation::stableHashFloat(room.center.y));
    value = foundation::stableHashCombine(value, foundation::stableHashFloat(room.center.z));
    value = foundation::stableHashCombine(value, foundation::stableHashFloat(room.extent.x));
    value = foundation::stableHashCombine(value, foundation::stableHashFloat(room.extent.y));
    return foundation::stableHashCombine(value, foundation::stableHashFloat(room.extent.z));
}

[[nodiscard]] std::uint64_t hash_part(const BuildingPart& part) noexcept {
    std::uint64_t value = foundation::stableHashCombine(part.id,
                                                        static_cast<std::uint32_t>(part.kind));
    value = foundation::stableHashCombine(value, part.key.building_id);
    value = foundation::stableHashCombine(value, part.key.floor);
    value = foundation::stableHashCombine(value, static_cast<std::uint32_t>(part.key.kind));
    value = foundation::stableHashCombine(value, static_cast<std::uint32_t>(part.key.role));
    value = foundation::stableHashCombine(value, part.key.ordinal);
    value = foundation::stableHashCombine(value, part.floor);
    value = foundation::stableHashCombine(value,
                                          foundation::stableHashFloat(part.center.x));
    value = foundation::stableHashCombine(value,
                                          foundation::stableHashFloat(part.center.y));
    value = foundation::stableHashCombine(value,
                                          foundation::stableHashFloat(part.center.z));
    value = foundation::stableHashCombine(value,
                                          foundation::stableHashFloat(part.extent.x));
    value = foundation::stableHashCombine(value,
                                          foundation::stableHashFloat(part.extent.y));
    value = foundation::stableHashCombine(value,
                                          foundation::stableHashFloat(part.extent.z));
    return foundation::stableHashCombine(value, part.structural ? 1U : 0U);
}

} // namespace

bool BuildingSpec::valid() const noexcept {
    if (building_id == 0 || seed == 0 || !std::isfinite(footprint.x) ||
        !std::isfinite(footprint.y) || !std::isfinite(footprint.z) || footprint.x < 2.0F ||
        footprint.y <= 0.0F || footprint.z < 2.0F || floors == 0 || floors > 32 ||
        !std::isfinite(floor_height) ||
        floor_height <= 1.5F || floor_height > 10.0F || !std::isfinite(wall_thickness) ||
        wall_thickness <= 0.05F ||
        wall_thickness >= std::min(footprint.x, footprint.z) * 0.25F || rooms_per_floor == 0 ||
        rooms_per_floor > 16) {
        return false;
    }
    const float room_width = footprint.x / static_cast<float>(rooms_per_floor) - wall_thickness;
    const float room_depth = footprint.z - 2.0F * wall_thickness;
    const float room_height = floor_height - wall_thickness;
    if (!std::isfinite(room_width) || !std::isfinite(room_depth) || !std::isfinite(room_height) ||
        room_width <= 0.0F || room_depth <= 0.0F || room_height <= 0.0F) {
        return false;
    }
    constexpr std::size_t maximum = std::numeric_limits<std::size_t>::max();
    return static_cast<std::size_t>(floors) <= maximum / rooms_per_floor &&
           static_cast<std::size_t>(floors) <= maximum / (rooms_per_floor + 8U);
}

foundation::Result<BuildingPlan, foundation::Error> BuildingGenerator::generate(
    const BuildingSpec& spec) {
    if (!spec.valid()) {
        return foundation::Result<BuildingPlan, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid building specification"});
    }

    BuildingPlan plan{};
    plan.building_id = spec.building_id;
    plan.seed = spec.seed;
    plan.footprint = spec.footprint;
    plan.rooms.reserve(static_cast<std::size_t>(spec.floors) * spec.rooms_per_floor);
    plan.parts.reserve(static_cast<std::size_t>(spec.floors) * (spec.rooms_per_floor + 8));
    const float floor_width = spec.footprint.x / static_cast<float>(spec.rooms_per_floor);
    const float room_width = floor_width - spec.wall_thickness;
    const float room_depth = spec.footprint.z - 2.0F * spec.wall_thickness;
    const float room_height = spec.floor_height - spec.wall_thickness;

    const auto append_part = [&](BuildingPartKind kind, foundation::Vec3 center,
                                 foundation::Vec3 extent, std::uint32_t floor,
                                 bool structural, BuildingPartRole role, std::uint32_t ordinal) {
        BuildingPart part{};
        part.key = {spec.building_id, floor, kind, role, ordinal};
        part.id = part_id(part.key);
        part.kind = kind;
        part.center = center;
        part.extent = extent;
        part.floor = floor;
        part.structural = structural;
        plan.parts.push_back(part);
    };

    append_part(BuildingPartKind::Foundation, {0.0F, 0.0F, 0.0F},
                {spec.footprint.x, spec.wall_thickness, spec.footprint.z}, 0, true,
                BuildingPartRole::Foundation, 0);
    for (std::uint32_t floor = 0; floor < spec.floors; ++floor) {
        const float floor_y = static_cast<float>(floor) * spec.floor_height;
        append_part(BuildingPartKind::Floor, {0.0F, floor_y, 0.0F},
                    {spec.footprint.x, spec.wall_thickness, spec.footprint.z}, floor, true,
                    BuildingPartRole::FloorSlab, 0);
        const float wall_y = floor_y + spec.floor_height * 0.5F;
        const float wall_height = spec.floor_height;
        append_part(BuildingPartKind::Wall,
                    {0.0F, wall_y, -spec.footprint.z * 0.5F},
                    {spec.footprint.x, wall_height, spec.wall_thickness}, floor, true,
                    BuildingPartRole::ExteriorNorthWall, 0);
        append_part(BuildingPartKind::Wall,
                    {0.0F, wall_y, spec.footprint.z * 0.5F},
                    {spec.footprint.x, wall_height, spec.wall_thickness}, floor, true,
                    BuildingPartRole::ExteriorSouthWall, 0);
        append_part(BuildingPartKind::Wall,
                    {-spec.footprint.x * 0.5F, wall_y, 0.0F},
                    {spec.wall_thickness, wall_height, spec.footprint.z}, floor, true,
                    BuildingPartRole::ExteriorWestWall, 0);
        append_part(BuildingPartKind::Wall,
                    {spec.footprint.x * 0.5F, wall_y, 0.0F},
                    {spec.wall_thickness, wall_height, spec.footprint.z}, floor, true,
                    BuildingPartRole::ExteriorEastWall, 0);
        for (std::uint32_t room_index = 0; room_index < spec.rooms_per_floor; ++room_index) {
            const float room_x = -spec.footprint.x * 0.5F + floor_width *
                (static_cast<float>(room_index) + 0.5F);
            const auto id = room_id(spec, floor, room_index);
            plan.rooms.push_back({id, floor, {room_x, wall_y, 0.0F},
                                  {room_width, room_height, room_depth}});
            if (room_index + 1 < spec.rooms_per_floor) {
                append_part(BuildingPartKind::Wall,
                            {-spec.footprint.x * 0.5F + floor_width *
                                 static_cast<float>(room_index + 1),
                             wall_y, 0.0F},
                            {spec.wall_thickness, wall_height, room_depth}, floor, false,
                            BuildingPartRole::InteriorPartition, room_index);
            }
        }
        append_part(BuildingPartKind::Door,
                    {-spec.footprint.x * 0.25F, floor_y + 1.0F, -spec.footprint.z * 0.5F},
                    {1.0F, 2.0F, spec.wall_thickness * 2.0F}, floor, false,
                    BuildingPartRole::EntranceDoor, 0);
    }
    append_part(BuildingPartKind::Roof,
                {0.0F, static_cast<float>(spec.floors) * spec.floor_height, 0.0F},
                {spec.footprint.x + spec.wall_thickness * 2.0F, spec.wall_thickness,
                 spec.footprint.z + spec.wall_thickness * 2.0F},
                spec.floors, true, BuildingPartRole::Roof, 0);
    std::vector<BuildingRoom> canonical_rooms = plan.rooms;
    std::sort(canonical_rooms.begin(), canonical_rooms.end(),
              [](const BuildingRoom& left, const BuildingRoom& right) {
                  return left.floor != right.floor ? left.floor < right.floor : left.id < right.id;
              });
    std::vector<BuildingPart> canonical_parts = plan.parts;
    std::sort(canonical_parts.begin(), canonical_parts.end(), part_key_less);
    std::uint64_t content_hash = hash_spec(spec);
    for (const BuildingRoom& room : canonical_rooms) {
        content_hash = foundation::stableHashCombine(content_hash, hash_room(room));
    }
    for (const BuildingPart& part : canonical_parts) {
        content_hash = foundation::stableHashCombine(content_hash, hash_part(part));
    }
    plan.content_hash = content_hash == 0 ? 1 : content_hash;
    return foundation::Result<BuildingPlan, foundation::Error>::success(std::move(plan));
}

foundation::Result<BuildingGenerationResult, foundation::Error> BuildingGenerator::generateSite(
    const world::BuildingSiteRequest& request, const BuildingSiteGenerationProfile& profile) {
    if (!request.valid() || !profile.valid()) {
        return foundation::Result<BuildingGenerationResult, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid building site request or profile"});
    }

    SiteBounds bounds{};
    if (!isSupportedSiteRectangle(request, bounds)) {
        return foundation::Result<BuildingGenerationResult, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument,
             "building site must be a centered, rotation-aligned rectangle"});
    }
    const float available_x = bounds.max_x - bounds.min_x - request.clearance_m * 2.0F;
    const float available_z = bounds.max_z - bounds.min_z - request.clearance_m * 2.0F;
    if (!std::isfinite(available_x) || !std::isfinite(available_z) || available_x < 2.0F ||
        available_z < 2.0F) {
        return foundation::Result<BuildingGenerationResult, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "building site has no buildable envelope"});
    }

    const foundation::StableId building_id =
        request.request_id == 0
            ? 1
            : foundation::stableHashCombine(request.request_id,
                                            foundation::stableHashString("building-plan"));
    const std::uint32_t floor_span = request.floors_max - request.floors_min + 1U;
    const std::uint32_t floors =
        request.floors_min + static_cast<std::uint32_t>(request.seed % floor_span);
    BuildingSpec spec{};
    spec.building_id = building_id == 0 ? 1 : building_id;
    spec.seed = request.seed;
    spec.footprint = {std::min(request.preferred_footprint.x, available_x), 1.0F,
                      std::min(request.preferred_footprint.z, available_z)};
    spec.floors = floors;
    spec.floor_height = profile.floor_height;
    spec.wall_thickness = profile.wall_thickness;
    spec.rooms_per_floor = std::clamp<std::uint32_t>(
        static_cast<std::uint32_t>(std::lround(spec.footprint.x / profile.target_room_width)),
        profile.minimum_rooms_per_floor, profile.maximum_rooms_per_floor);
    const auto generated = generate(spec);
    if (!generated) {
        return foundation::Result<BuildingGenerationResult, foundation::Error>::failure(
            generated.error());
    }

    BuildingGenerationResult result{};
    result.plan = generated.value();
    result.resolution.request_id = request.request_id;
    result.resolution.parcel_id = request.parcel_id;
    result.resolution.building_id = spec.building_id;
    result.resolution.world_position = request.preferred_position;
    result.resolution.resolved_footprint = spec.footprint;
    result.resolution.rotation_y = request.preferred_rotation;
    result.resolution.entrance = request.access_point;
    result.resolution.approach = request.access_point;
    result.resolution.clearance_envelope = {
        spec.footprint.x + request.clearance_m * 2.0F, 1.0F,
        spec.footprint.z + request.clearance_m * 2.0F};
    result.resolution.entrance_width = request.access_width;
    if (!result.resolution.valid()) {
        return foundation::Result<BuildingGenerationResult, foundation::Error>::failure(
            {foundation::ErrorCode::Internal, "building site resolution is invalid"});
    }
    return foundation::Result<BuildingGenerationResult, foundation::Error>::success(
        std::move(result));
}

} // namespace genomes::buildings
