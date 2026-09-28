#include <genomes/world_render/WorldMeshCompiler.hpp>

#include <genomes/buildings/BuildingModel.hpp>
#include <genomes/foundation/StableHash.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

namespace genomes::world_render {

namespace {

void append_colored_box(render::RenderMesh& mesh,
                        foundation::Vec3 base,
                        foundation::Vec3 size,
                        foundation::Color color,
                        float rotation_y) {
    if (!std::isfinite(base.x) || !std::isfinite(base.y) || !std::isfinite(base.z) ||
        !std::isfinite(size.x) || !std::isfinite(size.y) || !std::isfinite(size.z) ||
        size.x <= 0.0F || size.y <= 0.0F || size.z <= 0.0F) {
        return;
    }
    const float half_x = size.x * 0.5F;
    const float half_z = size.z * 0.5F;
    const float cosine = std::cos(rotation_y);
    const float sine = std::sin(rotation_y);
    const auto rotate = [base, cosine, sine](float x, float y, float z) {
        return foundation::Vec3{base.x + x * cosine - z * sine, base.y + y,
                                base.z + x * sine + z * cosine};
    };
    const foundation::Vec3 corners[] = {
        rotate(-half_x, 0.0F, -half_z), rotate(half_x, 0.0F, -half_z),
        rotate(half_x, 0.0F, half_z),   rotate(-half_x, 0.0F, half_z),
        rotate(-half_x, size.y, -half_z), rotate(half_x, size.y, -half_z),
        rotate(half_x, size.y, half_z),   rotate(-half_x, size.y, half_z)};
    constexpr std::uint32_t faces[][4] = {
        {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6},
        {3, 0, 4, 7}, {4, 5, 6, 7}, {3, 2, 1, 0}};
    constexpr foundation::Vec3 normals[] = {
        {0.0F, 0.0F, -1.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F},
        {-1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}, {0.0F, -1.0F, 0.0F}};
    for (std::size_t face = 0; face < 6; ++face) {
        const std::uint32_t base_index = static_cast<std::uint32_t>(mesh.vertices.size());
        for (std::size_t corner = 0; corner < 4; ++corner) {
            mesh.vertices.push_back({corners[faces[face][corner]], normals[face],
                                     {corner == 1 || corner == 2 ? 1.0F : 0.0F,
                                      corner >= 2 ? 1.0F : 0.0F},
                                     color});
        }
        mesh.indices.insert(mesh.indices.end(),
                            {base_index, base_index + 1, base_index + 2,
                             base_index, base_index + 2, base_index + 3});
    }
}

void append_centered_box(render::RenderMesh& mesh,
                         foundation::Vec3 center,
                         foundation::Vec3 size,
                         foundation::Color color,
                         float rotation_y) {
    foundation::Vec3 base = center;
    base.y -= size.y * 0.5F;
    append_colored_box(mesh, base, size, color, rotation_y);
}

[[nodiscard]] foundation::Vec3 rotate_local(foundation::Vec3 origin,
                                             foundation::Vec3 local,
                                             float rotation_y) noexcept {
    const float cosine = std::cos(rotation_y);
    const float sine = std::sin(rotation_y);
    return {origin.x + local.x * cosine - local.z * sine,
            origin.y + local.y,
            origin.z + local.x * sine + local.z * cosine};
}

[[nodiscard]] foundation::Color building_part_color(buildings::BuildingPartKind kind) noexcept {
    switch (kind) {
    case buildings::BuildingPartKind::Foundation:
        return {0.25F, 0.22F, 0.20F, 1.0F};
    case buildings::BuildingPartKind::Floor:
        return {0.38F, 0.28F, 0.20F, 1.0F};
    case buildings::BuildingPartKind::Wall:
        return {0.66F, 0.38F, 0.22F, 1.0F};
    case buildings::BuildingPartKind::Roof:
        return {0.18F, 0.16F, 0.15F, 1.0F};
    case buildings::BuildingPartKind::Door:
        return {0.22F, 0.11F, 0.05F, 1.0F};
    }
    return {0.5F, 0.5F, 0.5F, 1.0F};
}

[[nodiscard]] foundation::Color road_color(roads::RoadClass road_class) noexcept {
    switch (road_class) {
    case roads::RoadClass::Motorway:
    case roads::RoadClass::Trunk:
        return {0.18F, 0.19F, 0.18F, 1.0F};
    case roads::RoadClass::Arterial:
    case roads::RoadClass::Collector:
        return {0.25F, 0.26F, 0.24F, 1.0F};
    case roads::RoadClass::Local:
    case roads::RoadClass::Service:
        return {0.34F, 0.34F, 0.30F, 1.0F};
    case roads::RoadClass::Alley:
    case roads::RoadClass::Path:
        return {0.42F, 0.36F, 0.25F, 1.0F};
    }
    return {0.3F, 0.3F, 0.3F, 1.0F};
}

void append_road_edge(render::RenderMesh& mesh,
                      const roads::RoadEdge& edge,
                      const terrain::HeightField& terrain) {
    const std::vector<foundation::Vec3> points = edge.centerline.samplePoints();
    if (points.size() < 2 || !std::isfinite(edge.width) || edge.width <= 0.0F) {
        return;
    }
    const foundation::Color color = road_color(edge.road_class);
    for (std::size_t index = 1; index < points.size(); ++index) {
        const foundation::Vec3 from = points[index - 1];
        const foundation::Vec3 to = points[index];
        const float dx = to.x - from.x;
        const float dz = to.z - from.z;
        const float length = std::sqrt(dx * dx + dz * dz);
        if (!std::isfinite(length) || length <= 0.001F) {
            continue;
        }
        const float half_width = edge.width * 0.5F;
        const foundation::Vec3 perpendicular{-dz / length * half_width, 0.0F,
                                             dx / length * half_width};
        const auto on_terrain = [&terrain](foundation::Vec3 point) {
            point.y = terrain.sampleBilinear(point.x, point.z) + 0.06F;
            return point;
        };
        const foundation::Vec3 vertices[] = {
            on_terrain({from.x + perpendicular.x, from.y, from.z + perpendicular.z}),
            on_terrain({from.x - perpendicular.x, from.y, from.z - perpendicular.z}),
            on_terrain({to.x - perpendicular.x, to.y, to.z - perpendicular.z}),
            on_terrain({to.x + perpendicular.x, to.y, to.z + perpendicular.z}),
        };
        const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
        for (std::size_t corner = 0; corner < 4; ++corner) {
            mesh.vertices.push_back({vertices[corner], {0.0F, 1.0F, 0.0F},
                                     {corner == 1 || corner == 2 ? 1.0F : 0.0F,
                                      corner >= 2 ? 1.0F : 0.0F},
                                     color});
        }
        mesh.indices.insert(mesh.indices.end(),
                            {base, base + 1, base + 2, base, base + 2, base + 3});
    }
}

void append_river(render::RenderMesh& mesh,
                  const hydrology::HydrologyArtifact& hydrology,
                  const hydrology::RiverPath& river,
                  const terrain::HeightField& terrain) {
    if (river.point_count < 2 ||
        static_cast<std::size_t>(river.point_offset) + river.point_count >
            hydrology.river_points.size()) {
        return;
    }
    const foundation::Color color{0.08F, 0.28F, 0.52F, 0.88F};
    const float half_width = std::max(0.5F, river.width_m * 0.5F);
    const std::size_t begin = river.point_offset;
    const std::size_t end = begin + river.point_count;
    for (std::size_t index = begin + 1; index < end; ++index) {
        const foundation::Vec3 from = hydrology.river_points[index - 1];
        const foundation::Vec3 to = hydrology.river_points[index];
        const float dx = to.x - from.x;
        const float dz = to.z - from.z;
        const float length = std::sqrt(dx * dx + dz * dz);
        if (!std::isfinite(length) || length <= 0.001F) {
            continue;
        }
        const foundation::Vec3 perpendicular{-dz / length * half_width, 0.0F,
                                             dx / length * half_width};
        const auto water_surface = [&terrain](foundation::Vec3 point) {
            point.y = terrain.sampleBilinear(point.x, point.z) + 0.10F;
            return point;
        };
        const foundation::Vec3 vertices[] = {
            water_surface({from.x + perpendicular.x, from.y, from.z + perpendicular.z}),
            water_surface({from.x - perpendicular.x, from.y, from.z - perpendicular.z}),
            water_surface({to.x - perpendicular.x, to.y, to.z - perpendicular.z}),
            water_surface({to.x + perpendicular.x, to.y, to.z + perpendicular.z}),
        };
        const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
        for (std::size_t corner = 0; corner < 4; ++corner) {
            mesh.vertices.push_back({vertices[corner], {0.0F, 1.0F, 0.0F},
                                     {corner == 1 || corner == 2 ? 1.0F : 0.0F,
                                      corner >= 2 ? 1.0F : 0.0F},
                                     color});
        }
        mesh.indices.insert(mesh.indices.end(),
                            {base, base + 1, base + 2, base, base + 2, base + 3});
    }
}

} // namespace

foundation::Result<std::shared_ptr<const render::RenderMesh>, foundation::Error>
WorldMeshCompiler::compile(const world::WorldPlan& plan, const terrain::HeightField& terrain) {
    if (plan.map_size_m == 0 || plan.features.empty() || terrain.width() < 2 ||
        terrain.height() < 2 || !std::isfinite(terrain.cellSize()) || terrain.cellSize() <= 0.0F) {
        return foundation::Result<std::shared_ptr<const render::RenderMesh>, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid world presentation inputs"});
    }

    auto mesh = std::make_shared<render::RenderMesh>();
    mesh->mesh_id = foundation::stable_id("mesh.world.semantic");
    mesh->vertices.reserve(plan.features.size() * 24);
    mesh->indices.reserve(plan.features.size() * 36);
    for (const world::WorldFeature& feature : plan.features) {
        foundation::Color color{};
        foundation::Vec3 size = feature.scale;
        switch (feature.kind) {
        case world::WorldFeatureKind::TerrainPatch:
            continue;
        case world::WorldFeatureKind::Road:
            if (!plan.city.road_graph.empty()) {
                continue;
            }
            color = {0.34F, 0.35F, 0.33F, 1.0F};
            size.y = 0.12F;
            break;
        case world::WorldFeatureKind::Parcel:
            color = {0.20F, 0.30F, 0.16F, 1.0F};
            size.y = 0.05F;
            break;
        case world::WorldFeatureKind::Building:
            if (!plan.building_sites.empty()) {
                continue;
            }
            color = {0.55F + static_cast<float>(feature.variant % 3) * 0.08F,
                     0.30F, 0.18F, 1.0F};
            size.y = 5.0F + static_cast<float>(feature.variant) * 1.25F;
            break;
        case world::WorldFeatureKind::Vegetation:
            color = {0.10F, 0.40F, 0.16F, 1.0F};
            size = {feature.scale.x * 1.8F, feature.scale.y * 5.0F,
                    feature.scale.z * 1.8F};
            break;
        case world::WorldFeatureKind::Fence:
            color = {0.62F, 0.43F, 0.20F, 1.0F};
            size.y = 1.4F;
            break;
        }
        foundation::Vec3 base = feature.position;
        base.y += terrain.sampleBilinear(feature.position.x, feature.position.z);
        append_colored_box(*mesh, base, size, color, feature.rotation_y);
    }

    // Roads are compiled from the semantic graph, not from feature-scale
    // boxes.  Centerline endpoint identity and terrain sampling are preserved
    // in this representation compiler.
    for (const roads::RoadEdge& edge : plan.city.road_graph.edges()) {
        append_road_edge(*mesh, edge, terrain);
    }
    for (const hydrology::RiverPath& river : plan.hydrology.rivers) {
        append_river(*mesh, plan.hydrology, river, terrain);
    }

    // Compile the semantic building descriptors through the building module.
    // This keeps rooms, structural parts and stable part IDs available to
    // future collision/destruction compilers instead of collapsing buildings
    // into one opaque box at the world-generator boundary.
    for (const world::BuildingSiteRequest& site : plan.building_sites) {
        const auto building_result = buildings::BuildingGenerator::generateSite(site);
        if (!building_result) {
            return foundation::Result<std::shared_ptr<const render::RenderMesh>,
                                      foundation::Error>::failure(building_result.error());
        }
        const buildings::BuildingGenerationResult& generated = building_result.value();
        const float terrain_height =
            terrain.sampleBilinear(generated.resolution.world_position.x,
                                   generated.resolution.world_position.z);
        for (const buildings::BuildingPart& part : generated.plan.parts) {
            foundation::Vec3 center = rotate_local(generated.resolution.world_position,
                                                    part.center,
                                                    generated.resolution.rotation_y);
            center.y += terrain_height;
            append_centered_box(*mesh, center, part.extent, building_part_color(part.kind),
                                generated.resolution.rotation_y);
        }
    }

    if (mesh->vertices.empty() || mesh->indices.empty()) {
        return foundation::Result<std::shared_ptr<const render::RenderMesh>, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "world plan compiled to an empty mesh"});
    }
    return foundation::Result<std::shared_ptr<const render::RenderMesh>, foundation::Error>::success(
        std::move(mesh));
}

} // namespace genomes::world_render
