#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/proc/Seed.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

namespace genomes::world {

enum class SiteAccessClass : std::uint8_t {
    Service,
    Local,
};

enum class SiteAccessSurface : std::uint8_t {
    Track,
    Gravel,
    Asphalt,
};

// City-owned boundary DTO. Polygon points use world X/Y where Y represents
// the world Z coordinate; the building module never owns the parcel itself.
struct BuildingSiteRequest final {
    foundation::StableId request_id{0};
    foundation::StableId parcel_id{0};
    proc::Seed seed{0};
    std::vector<foundation::Vec2> buildable_polygon;
    foundation::Vec3 preferred_position{};
    foundation::Vec3 preferred_footprint{12.0F, 1.0F, 10.0F};
    float preferred_rotation{0.0F};
    std::uint32_t floors_min{1};
    std::uint32_t floors_max{3};
    foundation::Vec2 access_point{};
    float access_width{1.2F};
    float clearance_m{1.0F};
    SiteAccessClass access_class{SiteAccessClass::Service};
    SiteAccessSurface access_surface{SiteAccessSurface::Track};

    [[nodiscard]] bool valid() const noexcept {
        if (request_id == 0 || parcel_id == 0 || seed == 0 || buildable_polygon.size() < 4 ||
            floors_min == 0 || floors_min > floors_max || floors_max > 32 ||
            !std::isfinite(preferred_position.x) || !std::isfinite(preferred_position.y) ||
            !std::isfinite(preferred_position.z) || !std::isfinite(preferred_footprint.x) ||
            !std::isfinite(preferred_footprint.z) || preferred_footprint.x < 2.0F ||
            preferred_footprint.z < 2.0F || !std::isfinite(preferred_rotation) ||
            !std::isfinite(access_point.x) || !std::isfinite(access_point.y) ||
            !std::isfinite(access_width) || access_width <= 0.0F ||
            !std::isfinite(clearance_m) || clearance_m < 0.0F) {
            return false;
        }
        for (const foundation::Vec2 point : buildable_polygon) {
            if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] BuildingSiteRequest translated(foundation::Vec3 offset) const {
        BuildingSiteRequest result = *this;
        result.preferred_position.x += offset.x;
        result.preferred_position.y += offset.y;
        result.preferred_position.z += offset.z;
        result.access_point.x += offset.x;
        result.access_point.y += offset.z;
        for (foundation::Vec2& point : result.buildable_polygon) {
            point.x += offset.x;
            point.y += offset.z;
        }
        return result;
    }
};

struct BuildingSiteResolution final {
    foundation::StableId request_id{0};
    foundation::StableId parcel_id{0};
    foundation::StableId building_id{0};
    foundation::Vec3 world_position{};
    foundation::Vec3 resolved_footprint{};
    float rotation_y{0.0F};
    foundation::Vec2 entrance{};
    foundation::Vec2 approach{};
    foundation::Vec3 clearance_envelope{};
    float entrance_width{1.2F};

    [[nodiscard]] bool valid() const noexcept {
        return request_id != 0 && parcel_id != 0 && building_id != 0 &&
               std::isfinite(world_position.x) && std::isfinite(world_position.y) &&
               std::isfinite(world_position.z) && std::isfinite(resolved_footprint.x) &&
               std::isfinite(resolved_footprint.z) && resolved_footprint.x >= 2.0F &&
               resolved_footprint.z >= 2.0F && std::isfinite(rotation_y) &&
               std::isfinite(entrance.x) && std::isfinite(entrance.y) &&
               std::isfinite(approach.x) && std::isfinite(approach.y) &&
               std::isfinite(clearance_envelope.x) && std::isfinite(clearance_envelope.z) &&
               entrance_width > 0.0F;
    }
};

} // namespace genomes::world
