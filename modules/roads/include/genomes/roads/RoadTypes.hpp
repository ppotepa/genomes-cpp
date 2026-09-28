#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/proc/Seed.hpp>

#include <cstdint>
#include <vector>

namespace genomes::roads {

enum class RoadClass : std::uint8_t {
    Motorway,
    Trunk,
    Arterial,
    Collector,
    Local,
    Service,
    Alley,
    Path,
};

enum class RoadSurface : std::uint8_t {
    Asphalt,
    Gravel,
    Dirt,
    Track,
};

enum class RoadNodeType : std::uint8_t {
    Endpoint,
    Junction,
    ParcelAccess,
};

enum class RoadCenterlineKind : std::uint8_t {
    Straight,
    Polyline,
};

// Endpoints are canonical node coordinates.  Intermediate points are only
// needed for curved/polyline roads and are never allowed to redefine either
// endpoint.
struct RoadCenterline final {
    RoadCenterlineKind kind{RoadCenterlineKind::Straight};
    foundation::Vec3 start{};
    foundation::Vec3 end{};
    std::vector<foundation::Vec3> points;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::vector<foundation::Vec3> samplePoints() const;
};

struct RoadNode final {
    foundation::StableId id{0};
    foundation::Vec3 position{};
    RoadNodeType type{RoadNodeType::Endpoint};
    proc::Seed lineage_seed{0};
};

struct RoadEdge final {
    foundation::StableId id{0};
    foundation::StableId from{0};
    foundation::StableId to{0};
    RoadClass road_class{RoadClass::Local};
    RoadSurface surface{RoadSurface::Asphalt};
    float width{4.8F};
    float speed_limit{30.0F};
    float capacity{450.0F};
    proc::Seed lineage_seed{0};
    RoadCenterline centerline{};

    [[nodiscard]] bool valid() const noexcept;
};

} // namespace genomes::roads
