#include <genomes/world/CityPlan.hpp>

#include <genomes/foundation/StableHash.hpp>
#include <genomes/proc/RandomStream.hpp>
#include <genomes/proc/SeedPath.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <utility>

namespace genomes::world {

namespace {

constexpr float kPi = 3.14159265358979323846F;

struct GeneratedRoad final {
    foundation::StableId id{0};
    foundation::Vec3 position{};
    foundation::Vec3 scale{1.0F, 1.0F, 1.0F};
    float rotation_y{0.0F};
    std::uint32_t variant{0};
};

[[nodiscard]] std::uint32_t densityCount(double area_m2,
                                         float density,
                                         double units_per_object,
                                         std::uint32_t maximum) noexcept {
    const double requested = area_m2 * static_cast<double>(density) / units_per_object;
    const double clamped = std::clamp(requested, 0.0, static_cast<double>(maximum));
    return static_cast<std::uint32_t>(std::floor(clamped));
}

[[nodiscard]] foundation::StableId cityElementId(proc::Seed seed,
                                                  std::string_view label,
                                                  std::uint32_t index) noexcept {
    std::uint64_t hash = foundation::stableHashU64(seed);
    hash = foundation::stableHashCombine(hash, foundation::stableHashString(label));
    hash = foundation::stableHashCombine(hash, index);
    return hash == 0 ? 1 : hash;
}

[[nodiscard]] std::uint64_t roadHash(const GeneratedRoad& road) noexcept {
    std::uint64_t hash = foundation::stableHashCombine(road.id, road.variant);
    hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(
                                                   std::bit_cast<std::uint32_t>(road.position.x)));
    hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(
                                                   std::bit_cast<std::uint32_t>(road.position.z)));
    hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(
                                                   std::bit_cast<std::uint32_t>(road.scale.x)));
    hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(
                                                   std::bit_cast<std::uint32_t>(road.scale.z)));
    return hash;
}

[[nodiscard]] std::uint64_t parcelHash(const CityParcel& parcel) noexcept {
    std::uint64_t hash = foundation::stableHashCombine(parcel.id, parcel.parcel_variant);
    hash = foundation::stableHashCombine(hash, parcel.building_variant);
    hash = foundation::stableHashCombine(hash, parcel.fenced ? 1 : 0);
    hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(
                                                   std::bit_cast<std::uint32_t>(parcel.position.x)));
    hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(
                                                   std::bit_cast<std::uint32_t>(parcel.position.z)));
    hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(
                                                   std::bit_cast<std::uint32_t>(parcel.building_scale.x)));
    hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(
                                                   std::bit_cast<std::uint32_t>(parcel.building_scale.z)));
    return hash;
}

} // namespace

foundation::Result<CityPlan, foundation::Error> CityGenerator::generate(
    const CityGenerationRequest& request) {
    if (!request.valid()) {
        return foundation::Result<CityPlan, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid city generation request"});
    }

    CityPlan plan{};
    plan.seed = request.seed;
    plan.map_size_m = request.map_size_m;
    const float map_size = static_cast<float>(request.map_size_m);
    const float half_size = map_size * 0.5F;
    const double area_m2 = static_cast<double>(request.map_size_m) * request.map_size_m;
    const std::uint32_t building_count = densityCount(area_m2, request.buildings, 16'000.0,
                                                      256);
    std::vector<GeneratedRoad> generated_roads;
    generated_roads.reserve(4);
    plan.parcels.reserve(building_count);

    std::uint64_t content_hash = foundation::stableHashU64(plan.generator_version);
    content_hash = foundation::stableHashCombine(content_hash, plan.seed);
    content_hash = foundation::stableHashCombine(content_hash, plan.map_size_m);

    const proc::SeedPath root_path(plan.seed);
    proc::RandomStream road_random(root_path.child("roads", 0));
    auto append_road = [&plan, &generated_roads, &content_hash](foundation::Vec3 position,
                                                                 foundation::Vec3 scale,
                                                                 float rotation_y,
                                                                 std::uint32_t variant) {
        GeneratedRoad road{};
        road.id = cityElementId(plan.seed, "city-road", static_cast<std::uint32_t>(
                                                              generated_roads.size()));
        road.position = position;
        road.scale = scale;
        road.rotation_y = rotation_y;
        road.variant = variant;
        content_hash = foundation::stableHashCombine(content_hash, roadHash(road));
        generated_roads.push_back(road);
    };
    append_road({0.0F, 0.02F, 0.0F}, {map_size * 0.92F, 0.05F, 8.0F}, 0.0F, 0);
    append_road({0.0F, 0.025F, 0.0F}, {8.0F, 0.05F, map_size * 0.92F}, 0.0F, 1);
    for (std::uint32_t index = 0; index < 2; ++index) {
        const float angle = static_cast<float>(road_random.uniformRange(-0.35, 0.35));
        const float offset = static_cast<float>(road_random.uniformRange(-half_size * 0.22,
                                                                           half_size * 0.22));
        append_road({offset, 0.03F, 0.0F}, {map_size * 0.58F, 0.04F, 5.0F}, angle, 2 + index);
    }

    // Publish a semantic road graph alongside the compact presentation
    // records above.  The graph owns canonical node coordinates and exact
    // centerline endpoints; the temporary generation records below never cross
    // the public world contract.
    roads::RoadGraphBuilder road_graph_builder;
    const foundation::StableId center_node_id = cityElementId(plan.seed, "road-node", 0);
    if (!road_graph_builder
             .addNode({center_node_id, {0.0F, 0.0F, 0.0F}, roads::RoadNodeType::Junction,
                       plan.seed})) {
        return foundation::Result<CityPlan, foundation::Error>::failure(
            {foundation::ErrorCode::Internal, "could not create city road junction"});
    }

    const auto endpoint = [](const GeneratedRoad& road, float normalized) {
        const bool along_x = road.scale.x >= road.scale.z;
        const float length = along_x ? road.scale.x : road.scale.z;
        const float local_x = along_x ? normalized * length : 0.0F;
        const float local_z = along_x ? 0.0F : normalized * length;
        const float cosine = std::cos(road.rotation_y);
        const float sine = std::sin(road.rotation_y);
        return foundation::Vec3{road.position.x + local_x * cosine - local_z * sine,
                                0.0F,
                                road.position.z + local_x * sine + local_z * cosine};
    };
    const auto add_graph_node = [&road_graph_builder, &plan](foundation::StableId id,
                                                               foundation::Vec3 position,
                                                               roads::RoadNodeType type) {
        return road_graph_builder.addNode({id, position, type, plan.seed});
    };
    const auto add_graph_edge = [&road_graph_builder, &plan](foundation::StableId id,
                                                               foundation::StableId from,
                                                               foundation::StableId to,
                                                               foundation::Vec3 start,
                                                               foundation::Vec3 end,
                                                               float width,
                                                               roads::RoadClass road_class) {
        roads::RoadEdge edge{};
        edge.id = id;
        edge.from = from;
        edge.to = to;
        edge.road_class = road_class;
        edge.surface = roads::RoadSurface::Asphalt;
        edge.width = std::max(1.0F, width);
        edge.speed_limit = road_class == roads::RoadClass::Collector ? 42.0F : 30.0F;
        edge.capacity = road_class == roads::RoadClass::Collector ? 900.0F : 450.0F;
        edge.lineage_seed = plan.seed;
        edge.centerline = {roads::RoadCenterlineKind::Straight, start, end, {}};
        return road_graph_builder.addEdge(std::move(edge));
    };

    std::uint32_t node_index = 1;
    std::uint32_t edge_index = 0;
    for (std::size_t road_index = 0; road_index < generated_roads.size(); ++road_index) {
        const GeneratedRoad& road = generated_roads[road_index];
        const bool is_main_axis = road_index < 2;
        const float width = std::min(std::abs(road.scale.x), std::abs(road.scale.z));
        const foundation::Vec3 start = endpoint(road, -0.5F);
        const foundation::Vec3 end = endpoint(road, 0.5F);
        if (is_main_axis) {
            const foundation::StableId outer_start =
                cityElementId(plan.seed, "road-node", node_index++);
            const foundation::StableId outer_end =
                cityElementId(plan.seed, "road-node", node_index++);
            if (!add_graph_node(outer_start, start, roads::RoadNodeType::Endpoint) ||
                !add_graph_node(outer_end, end, roads::RoadNodeType::Endpoint) ||
                !add_graph_edge(cityElementId(plan.seed, "road-edge", edge_index++), outer_start,
                                center_node_id, start, {0.0F, 0.0F, 0.0F}, width,
                                roads::RoadClass::Collector) ||
                !add_graph_edge(cityElementId(plan.seed, "road-edge", edge_index++),
                                center_node_id, outer_end, {0.0F, 0.0F, 0.0F}, end, width,
                                roads::RoadClass::Collector)) {
                return foundation::Result<CityPlan, foundation::Error>::failure(
                    {foundation::ErrorCode::Internal, "could not build city road graph"});
            }
        } else {
            const foundation::StableId from =
                cityElementId(plan.seed, "road-node", node_index++);
            const foundation::StableId to = cityElementId(plan.seed, "road-node", node_index++);
            if (!add_graph_node(from, start, roads::RoadNodeType::Endpoint) ||
                !add_graph_node(to, end, roads::RoadNodeType::Endpoint) ||
                !add_graph_edge(cityElementId(plan.seed, "road-edge", edge_index++), from, to,
                                start, end, width, roads::RoadClass::Local)) {
                return foundation::Result<CityPlan, foundation::Error>::failure(
                    {foundation::ErrorCode::Internal, "could not build city road graph"});
            }
        }
    }
    auto road_graph = std::move(road_graph_builder).freeze();
    if (!road_graph) {
        return foundation::Result<CityPlan, foundation::Error>::failure(road_graph.error());
    }
    plan.road_graph = std::move(road_graph.value());
    content_hash = foundation::stableHashCombine(content_hash, plan.road_graph.contentHash());

    const std::uint32_t grid_side =
        std::max<std::uint32_t>(1, static_cast<std::uint32_t>(std::ceil(
            std::sqrt(static_cast<double>(std::max<std::uint32_t>(1, building_count))))));
    const float border = std::min(48.0F, half_size * 0.18F);
    const float usable_size = std::max(1.0F, map_size - 2.0F * border);
    proc::RandomStream building_random(root_path.child("buildings", 0));
    proc::RandomStream fence_random(root_path.child("fences", 0));
    for (std::uint32_t index = 0; index < building_count; ++index) {
        const std::uint32_t gx = index % grid_side;
        const std::uint32_t gz = index / grid_side;
        const float cell_x = (static_cast<float>(gx) + 0.5F) /
                             static_cast<float>(grid_side);
        const float cell_z = (static_cast<float>(gz) + 0.5F) /
                             static_cast<float>(grid_side);
        const float jitter_x = static_cast<float>(building_random.uniformRange(-0.25, 0.25));
        const float jitter_z = static_cast<float>(building_random.uniformRange(-0.25, 0.25));
        const float x = -half_size + border + std::clamp(cell_x + jitter_x /
                                                              static_cast<float>(grid_side),
                                                          0.05F, 0.95F) * usable_size;
        const float z = -half_size + border + std::clamp(cell_z + jitter_z /
                                                              static_cast<float>(grid_side),
                                                          0.05F, 0.95F) * usable_size;
        const float width = static_cast<float>(building_random.uniformRange(10.0, 24.0));
        const float depth = static_cast<float>(building_random.uniformRange(8.0, 20.0));
        CityParcel parcel{};
        parcel.id = cityElementId(plan.seed, "city-parcel", index);
        parcel.position = {x, 0.12F, z};
        parcel.building_scale = {width, 1.0F, depth};
        parcel.parcel_scale = {width * 1.9F, 0.01F, depth * 1.9F};
        parcel.rotation_y = static_cast<float>(building_random.uniformRange(-kPi, kPi));
        parcel.parcel_variant = index % 3;
        parcel.building_variant = building_random.bounded(4);
        parcel.fenced = fence_random.uniform01() <= static_cast<double>(request.fenced_parcels);
        content_hash = foundation::stableHashCombine(content_hash, parcelHash(parcel));
        plan.parcels.push_back(parcel);
    }

    plan.content_hash = content_hash == 0 ? 1 : content_hash;
    return foundation::Result<CityPlan, foundation::Error>::success(std::move(plan));
}

} // namespace genomes::world
