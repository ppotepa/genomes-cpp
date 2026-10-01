#include <genomes/buildings/BuildingModel.hpp>

#include <genomes/foundation/StableHash.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

namespace genomes::buildings {

namespace {

[[nodiscard]] foundation::StableId part_id(const BuildingSpec& spec,
                                           std::string_view kind,
                                           std::uint32_t index) noexcept {
    std::uint64_t value = foundation::stableHashCombine(spec.building_id,
                                                        foundation::stableHashString(kind));
    value = foundation::stableHashCombine(value, spec.seed);
    value = foundation::stableHashCombine(value, index);
    return value == 0 ? 1 : value;
}

[[nodiscard]] std::uint64_t hash_part(const BuildingPart& part) noexcept {
    std::uint64_t value = foundation::stableHashCombine(part.id,
                                                        static_cast<std::uint32_t>(part.kind));
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
    return value;
}

} // namespace

bool BuildingSpec::valid() const noexcept {
    if (building_id == 0 || seed == 0 || !std::isfinite(footprint.x) ||
        !std::isfinite(footprint.y) || !std::isfinite(footprint.z) || footprint.x < 2.0F ||
        footprint.z < 2.0F || floors == 0 || floors > 32 || !std::isfinite(floor_height) ||
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
    std::uint64_t content_hash = foundation::stableHashCombine(spec.building_id, spec.seed);
    const float floor_width = spec.footprint.x / static_cast<float>(spec.rooms_per_floor);
    const float room_width = floor_width - spec.wall_thickness;
    const float room_depth = spec.footprint.z - 2.0F * spec.wall_thickness;
    const float room_height = spec.floor_height - spec.wall_thickness;

    const auto append_part = [&](BuildingPartKind kind, foundation::Vec3 center,
                                 foundation::Vec3 extent, std::uint32_t floor,
                                 bool structural, std::string_view label) {
        BuildingPart part{};
        part.id = part_id(spec, label, static_cast<std::uint32_t>(plan.parts.size()));
        part.kind = kind;
        part.center = center;
        part.extent = extent;
        part.floor = floor;
        part.structural = structural;
        content_hash = foundation::stableHashCombine(content_hash, hash_part(part));
        plan.parts.push_back(part);
    };

    append_part(BuildingPartKind::Foundation, {0.0F, 0.0F, 0.0F},
                {spec.footprint.x, spec.wall_thickness, spec.footprint.z}, 0, true,
                "foundation");
    for (std::uint32_t floor = 0; floor < spec.floors; ++floor) {
        const float floor_y = static_cast<float>(floor) * spec.floor_height;
        append_part(BuildingPartKind::Floor, {0.0F, floor_y, 0.0F},
                    {spec.footprint.x, spec.wall_thickness, spec.footprint.z}, floor, true,
                    "floor");
        const float wall_y = floor_y + spec.floor_height * 0.5F;
        const float wall_height = spec.floor_height;
        append_part(BuildingPartKind::Wall,
                    {0.0F, wall_y, -spec.footprint.z * 0.5F},
                    {spec.footprint.x, wall_height, spec.wall_thickness}, floor, true, "wall-north");
        append_part(BuildingPartKind::Wall,
                    {0.0F, wall_y, spec.footprint.z * 0.5F},
                    {spec.footprint.x, wall_height, spec.wall_thickness}, floor, true, "wall-south");
        append_part(BuildingPartKind::Wall,
                    {-spec.footprint.x * 0.5F, wall_y, 0.0F},
                    {spec.wall_thickness, wall_height, spec.footprint.z}, floor, true, "wall-west");
        append_part(BuildingPartKind::Wall,
                    {spec.footprint.x * 0.5F, wall_y, 0.0F},
                    {spec.wall_thickness, wall_height, spec.footprint.z}, floor, true, "wall-east");
        for (std::uint32_t room_index = 0; room_index < spec.rooms_per_floor; ++room_index) {
            const float room_x = -spec.footprint.x * 0.5F + floor_width *
                (static_cast<float>(room_index) + 0.5F);
            const auto id = part_id(spec, "room", floor * spec.rooms_per_floor + room_index);
            plan.rooms.push_back({id, floor, {room_x, wall_y, 0.0F},
                                  {room_width, room_height, room_depth}});
            if (room_index + 1 < spec.rooms_per_floor) {
                append_part(BuildingPartKind::Wall,
                            {-spec.footprint.x * 0.5F + floor_width *
                                 static_cast<float>(room_index + 1),
                             wall_y, 0.0F},
                            {spec.wall_thickness, wall_height, room_depth}, floor, false,
                            "partition");
            }
        }
        append_part(BuildingPartKind::Door,
                    {-spec.footprint.x * 0.25F, floor_y + 1.0F, -spec.footprint.z * 0.5F},
                    {1.0F, 2.0F, spec.wall_thickness * 2.0F}, floor, false, "door");
    }
    append_part(BuildingPartKind::Roof,
                {0.0F, static_cast<float>(spec.floors) * spec.floor_height, 0.0F},
                {spec.footprint.x + spec.wall_thickness * 2.0F, spec.wall_thickness,
                 spec.footprint.z + spec.wall_thickness * 2.0F},
                spec.floors, true, "roof");
    plan.content_hash = content_hash == 0 ? 1 : content_hash;
    return foundation::Result<BuildingPlan, foundation::Error>::success(std::move(plan));
}

foundation::Result<BuildingGenerationResult, foundation::Error> BuildingGenerator::generateSite(
    const world::BuildingSiteRequest& request) {
    if (!request.valid()) {
        return foundation::Result<BuildingGenerationResult, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid building site request"});
    }

    float min_x = std::numeric_limits<float>::max();
    float min_z = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_z = std::numeric_limits<float>::lowest();
    const float cosine = std::cos(request.preferred_rotation);
    const float sine = std::sin(request.preferred_rotation);
    for (const foundation::Vec2 point : request.buildable_polygon) {
        const float dx = point.x - request.preferred_position.x;
        const float dz = point.y - request.preferred_position.z;
        const float local_x = dx * cosine + dz * sine;
        const float local_z = -dx * sine + dz * cosine;
        min_x = std::min(min_x, local_x);
        min_z = std::min(min_z, local_z);
        max_x = std::max(max_x, local_x);
        max_z = std::max(max_z, local_z);
    }
    const float available_x = max_x - min_x - request.clearance_m * 2.0F;
    const float available_z = max_z - min_z - request.clearance_m * 2.0F;
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
    spec.floor_height = 2.8F;
    spec.rooms_per_floor = std::clamp<std::uint32_t>(
        static_cast<std::uint32_t>(std::lround(spec.footprint.x / 6.0F)), 1U, 8U);
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
