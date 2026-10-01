#include <genomes/world/WorldPlan.hpp>

#include <genomes/world/CityPlan.hpp>

#include <genomes/foundation/StableHash.hpp>
#include <genomes/proc/RandomStream.hpp>
#include <genomes/proc/SeedPath.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <utility>

namespace genomes::world {

namespace {

constexpr float kPi = 3.14159265358979323846F;

[[nodiscard]] std::uint64_t featureHash(const WorldFeature& feature) noexcept {
    std::uint64_t hash = foundation::stableHashCombine(
        static_cast<std::uint64_t>(feature.kind), feature.id);
    const auto combine_float = [&hash](float value) {
        hash = foundation::stableHashCombine(hash, std::bit_cast<std::uint32_t>(value));
    };
    combine_float(feature.position.x);
    combine_float(feature.position.y);
    combine_float(feature.position.z);
    combine_float(feature.scale.x);
    combine_float(feature.scale.y);
    combine_float(feature.scale.z);
    combine_float(feature.rotation_y);
    hash = foundation::stableHashCombine(hash, feature.variant);
    return hash;
}

[[nodiscard]] foundation::StableId makeFeatureId(proc::Seed seed,
                                                  WorldFeatureKind kind,
                                                  std::uint32_t index) noexcept {
    std::uint64_t hash = foundation::stableHashU64(seed);
    hash = foundation::stableHashCombine(hash, static_cast<std::uint64_t>(kind));
    hash = foundation::stableHashCombine(hash, index);
    return hash == 0 ? 1 : hash;
}

[[nodiscard]] std::uint32_t densityCount(double area_m2,
                                         float density,
                                         double units_per_object,
                                         std::uint32_t maximum) noexcept {
    const double requested = area_m2 * static_cast<double>(density) / units_per_object;
    const double clamped = std::clamp(requested, 0.0, static_cast<double>(maximum));
    return static_cast<std::uint32_t>(std::floor(clamped));
}

} // namespace

foundation::Result<WorldPlan, foundation::Error> WorldGenerator::generate(
    const WorldGenerationRequest& request) {
    if (!request.valid()) {
        return foundation::Result<WorldPlan, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid world generation request"});
    }

    WorldPlan plan{};
    plan.seed = request.seed;
    plan.map_size_m = request.map_size_m;
    const float map_size = static_cast<float>(request.map_size_m);
    const float half_size = map_size * 0.5F;
    const double area_m2 = static_cast<double>(request.map_size_m) * request.map_size_m;
    const proc::SeedPath root_path(request.seed);
    const std::uint32_t vegetation_count =
        densityCount(area_m2, request.vegetation, 500.0, 4096);
    plan.features.reserve(1 + 4 + vegetation_count +
                          densityCount(area_m2, request.buildings, 16'000.0, 256) * 3);
    plan.building_sites.reserve(densityCount(area_m2, request.buildings, 16'000.0, 256));

    std::array<std::uint32_t, 6> next_feature_indices{};
    std::uint64_t content_hash = foundation::stableHashU64(plan.generator_version);
    content_hash = foundation::stableHashCombine(content_hash, plan.seed);
    content_hash = foundation::stableHashCombine(content_hash, plan.map_size_m);

    hydrology::HydrologySpec hydrology_spec{};
    hydrology_spec.seed = request.seed;
    hydrology_spec.map_size_m = request.map_size_m;
    const GridLayout layout = GridLayout::forMap(request.map_size_m);
    const auto nonzero = [](std::uint64_t value) noexcept {
        return value == 0U ? std::uint64_t{1U} : value;
    };
    std::uint64_t layout_fingerprint = foundation::stableHashU64(layout.cell_count);
    layout_fingerprint = foundation::stableHashCombine(layout_fingerprint, layout.sample_count);
    layout_fingerprint = foundation::stableHashCombine(
        layout_fingerprint, std::bit_cast<std::uint32_t>(layout.spacing_m));
    layout_fingerprint = foundation::stableHashCombine(
        layout_fingerprint, std::bit_cast<std::uint32_t>(layout.extent_m));
    plan.stage_fingerprints[static_cast<std::size_t>(WorldGenerationStage::Terrain)] = {
        WorldGenerationStage::Terrain, root_path.child("terrain", 0).seed(),
        WorldStageFingerprintVersion, nonzero(layout_fingerprint)};
    hydrology_spec.cells_x = layout.cell_count;
    hydrology_spec.cells_z = hydrology_spec.cells_x;
    hydrology_spec.cell_size_m = 8.0F;
    hydrology_spec.mode = request.hydrology_mode;
    hydrology_spec.river_probability = request.river_probability;
    const auto hydrology_result = hydrology::HydrologyGenerator::generate(hydrology_spec);
    if (!hydrology_result) {
        return foundation::Result<WorldPlan, foundation::Error>::failure(
            hydrology_result.error());
    }
    plan.hydrology = std::move(hydrology_result.value());
    plan.stage_fingerprints[static_cast<std::size_t>(WorldGenerationStage::Hydrology)] = {
        WorldGenerationStage::Hydrology, root_path.child("river", 0).seed(),
        hydrology::HydrologyGeneratorVersion, nonzero(plan.hydrology.content_hash)};
    content_hash = foundation::stableHashCombine(content_hash, plan.hydrology.content_hash);

    auto append = [&](WorldFeatureKind kind,
                      foundation::Vec3 position,
                      foundation::Vec3 scale,
                      float rotation_y,
                      std::uint32_t variant,
                      foundation::StableId semantic_id = 0) {
        WorldFeature feature{};
        const auto kind_index = static_cast<std::size_t>(kind);
        const std::uint32_t object_index = next_feature_indices[kind_index]++;
        feature.id = semantic_id == 0 ? makeFeatureId(plan.seed, kind, object_index) : semantic_id;
        feature.kind = kind;
        feature.position = position;
        feature.scale = scale;
        feature.rotation_y = rotation_y;
        feature.variant = variant;
        content_hash = foundation::stableHashCombine(content_hash, featureHash(feature));
        plan.features.push_back(feature);
    };

    append(WorldFeatureKind::TerrainPatch, {0.0F, 0.0F, 0.0F},
           {map_size, 1.0F, map_size}, 0.0F, 0);

    auto city_result = CityGenerator::generate({request.seed, request.map_size_m,
                                                 request.buildings,
                                                 request.fenced_parcels});
    if (!city_result) {
        return foundation::Result<WorldPlan, foundation::Error>::failure(city_result.error());
    }
    CityPlan city = std::move(city_result.value());
    plan.stage_fingerprints[static_cast<std::size_t>(WorldGenerationStage::Roads)] = {
        WorldGenerationStage::Roads, root_path.child("roads", 0).seed(),
        city.generator_version, nonzero(city.road_graph.contentHash())};
    plan.stage_fingerprints[static_cast<std::size_t>(WorldGenerationStage::Buildings)] = {
        WorldGenerationStage::Buildings, root_path.child("buildings", 0).seed(),
        WorldStageFingerprintVersion, nonzero(city.content_hash)};
    content_hash = foundation::stableHashCombine(content_hash, city.content_hash);
    for (const roads::RoadEdge& edge : city.road_graph.edges()) {
        const foundation::Vec3 start = edge.centerline.start;
        const foundation::Vec3 end = edge.centerline.end;
        const float dx = end.x - start.x;
        const float dz = end.z - start.z;
        const float length = std::sqrt(dx * dx + dz * dz);
        const foundation::Vec3 center{(start.x + end.x) * 0.5F,
                                      (start.y + end.y) * 0.5F,
                                      (start.z + end.z) * 0.5F};
        append(WorldFeatureKind::Road, center, {length, 0.1F, edge.width},
               std::atan2(dz, dx), static_cast<std::uint32_t>(edge.road_class), edge.id);
    }
    for (const CityParcel& parcel : city.parcels) {
        append(WorldFeatureKind::Parcel, parcel.position, parcel.parcel_scale, parcel.rotation_y,
               parcel.parcel_variant, parcel.id);
        const foundation::StableId building_id = foundation::stableHashCombine(
            parcel.id, foundation::stableHashString("city-building"));
        append(WorldFeatureKind::Building, parcel.position, parcel.building_scale,
               parcel.rotation_y, parcel.building_variant, building_id == 0 ? 1 : building_id);
        const std::uint64_t building_seed_hash = foundation::stableHashCombine(
            request.seed, foundation::stableHashCombine(parcel.id,
                                                        foundation::stableHashString("building")));
        const foundation::StableId resolved_building_id = building_id == 0 ? 1 : building_id;
        const proc::Seed building_seed = building_seed_hash == 0 ? 1 : building_seed_hash;
        const float half_x = parcel.parcel_scale.x * 0.5F;
        const float half_z = parcel.parcel_scale.z * 0.5F;
        const float cosine = std::cos(parcel.rotation_y);
        const float sine = std::sin(parcel.rotation_y);
        const auto rotate_site_point = [&](float x, float z) {
            return foundation::Vec2{parcel.position.x + x * cosine - z * sine,
                                    parcel.position.z + x * sine + z * cosine};
        };
        BuildingSiteRequest site{};
        site.request_id = resolved_building_id;
        site.parcel_id = parcel.id;
        site.seed = building_seed;
        site.buildable_polygon = {rotate_site_point(-half_x, -half_z),
                                  rotate_site_point(half_x, -half_z),
                                  rotate_site_point(half_x, half_z),
                                  rotate_site_point(-half_x, half_z)};
        site.preferred_position = parcel.position;
        site.preferred_footprint = parcel.building_scale;
        site.preferred_rotation = parcel.rotation_y;
        site.floors_min = 1;
        site.floors_max = 1U + parcel.building_variant % 3U;
        const foundation::Vec2 access = rotate_site_point(0.0F, -half_z);
        site.access_point = access;
        site.access_width = 1.2F;
        site.clearance_m = 1.0F;
        site.access_class = SiteAccessClass::Service;
        site.access_surface = SiteAccessSurface::Track;
        if (!site.valid()) {
            return foundation::Result<WorldPlan, foundation::Error>::failure(
                {foundation::ErrorCode::Internal, "city produced an invalid building site"});
        }
        plan.building_sites.push_back(std::move(site));
        content_hash = foundation::stableHashCombine(content_hash, building_seed_hash);
        if (parcel.fenced) {
            const foundation::StableId fence_id = foundation::stableHashCombine(
                parcel.id, foundation::stableHashString("city-fence"));
            append(WorldFeatureKind::Fence, parcel.position,
                   {parcel.parcel_scale.x, 0.7F, parcel.parcel_scale.z}, parcel.rotation_y, 0,
                   fence_id == 0 ? 1 : fence_id);
        }
    }

    proc::RandomStream vegetation_random(root_path.child("vegetation", 0));
    for (std::uint32_t index = 0; index < vegetation_count; ++index) {
        const float x = static_cast<float>(vegetation_random.uniformRange(-half_size + 8.0,
                                                                           half_size - 8.0));
        const float z = static_cast<float>(vegetation_random.uniformRange(-half_size + 8.0,
                                                                           half_size - 8.0));
        const float height = static_cast<float>(vegetation_random.uniformRange(0.75, 1.45));
        append(WorldFeatureKind::Vegetation, {x, 0.0F, z}, {height, height, height},
               static_cast<float>(vegetation_random.uniformRange(-kPi, kPi)),
               vegetation_random.bounded(5));
    }

    plan.city = std::move(city);
    std::uint64_t vegetation_fingerprint = foundation::stableHashU64(vegetation_count);
    for (const auto& feature : plan.features) {
        if (feature.kind == WorldFeatureKind::Vegetation) {
            vegetation_fingerprint = foundation::stableHashCombine(
                vegetation_fingerprint, featureHash(feature));
        }
    }
    plan.stage_fingerprints[static_cast<std::size_t>(WorldGenerationStage::Vegetation)] = {
        WorldGenerationStage::Vegetation, root_path.child("vegetation", 0).seed(),
        WorldStageFingerprintVersion, nonzero(vegetation_fingerprint)};
    plan.content_hash = content_hash == 0 ? 1 : content_hash;
    return foundation::Result<WorldPlan, foundation::Error>::success(std::move(plan));
}

} // namespace genomes::world
