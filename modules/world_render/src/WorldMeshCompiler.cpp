#include <genomes/world_render/WorldMeshCompiler.hpp>

#include <genomes/buildings/BuildingModel.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/render/ProceduralMeshes.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

namespace genomes::world_render {

namespace {

foundation::Result<void, foundation::Error> append_colored_box(render::RenderMesh& mesh,
                                                               foundation::Vec3 base,
                                                               foundation::Vec3 size,
                                                               foundation::Color color,
                                                               float rotation_y) {
    return render::procedural::append_box(
        mesh, {base.x, base.y + size.y * 0.5F, base.z}, size, color, rotation_y);
}

foundation::Result<void, foundation::Error> append_centered_box(render::RenderMesh& mesh,
                                                                foundation::Vec3 center,
                                                                foundation::Vec3 size,
                                                                foundation::Color color,
                                                                float rotation_y) {
    foundation::Vec3 base = center;
    base.y -= size.y * 0.5F;
    return append_colored_box(mesh, base, size, color, rotation_y);
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
    // Keep water visibly distinct under the neutral world lighting.  This is
    // vertex colour only; the material remains renderer-independent.
    const foundation::Color color{0.035F, 0.72F, 0.98F, 1.0F};
    const float half_width = std::max(0.5F, river.width_m * 0.5F);
    const std::size_t begin = river.point_offset;
    const std::size_t end = begin + river.point_count;
    // Hydrology paths intentionally stay compact.  The water ribbon needs a
    // cross section at least every terrain cell, otherwise interpolated water
    // can disappear below a hill between two valid river control points.
    std::vector<foundation::Vec3> centerline;
    centerline.reserve(river.point_count * 3U);
    centerline.push_back(hydrology.river_points[begin]);
    const float presentation_step_m = std::max(2.0F, terrain.cellSize() * 0.5F);
    for (std::size_t index = begin + 1U; index < end; ++index) {
        const foundation::Vec3 from = hydrology.river_points[index - 1U];
        const foundation::Vec3 to = hydrology.river_points[index];
        const std::uint32_t subdivisions = std::max(
            1U, static_cast<std::uint32_t>(std::ceil(
                    std::hypot(to.x - from.x, to.z - from.z) / presentation_step_m)));
        for (std::uint32_t subdivision = 1U; subdivision <= subdivisions; ++subdivision) {
            const float t = static_cast<float>(subdivision) / static_cast<float>(subdivisions);
            centerline.push_back({std::lerp(from.x, to.x, t), std::lerp(from.y, to.y, t),
                                  std::lerp(from.z, to.z, t)});
        }
    }
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.reserve(mesh.vertices.size() + centerline.size() * 2U);
    mesh.indices.reserve(mesh.indices.size() + (centerline.size() - 1U) * 6U);
    float travelled_m = 0.0F;
    const auto water_surface = [&terrain](foundation::Vec3 point) {
            // The visual surface must remain above the final carved terrain,
            // including the interpolated bank samples between height-field
            // vertices. Otherwise a correct hydrology artifact can render as
            // an apparently dry canyon.
            point.y = std::max(point.y + 0.12F,
                               terrain.sampleBilinear(point.x, point.z) + 0.10F);
            return point;
    };
    for (std::size_t index = 0U; index < centerline.size(); ++index) {
        const foundation::Vec3 point = centerline[index];
        const foundation::Vec3& before = centerline[index == 0U ? index : index - 1U];
        const foundation::Vec3& after = centerline[index + 1U == centerline.size() ? index : index + 1U];
        const float dx = after.x - before.x;
        const float dz = after.z - before.z;
        const float length = std::hypot(dx, dz);
        if (!std::isfinite(length) || length <= 0.001F) {
            return;
        }
        if (index > 0U) {
            travelled_m += std::hypot(point.x - before.x, point.z - before.z);
        }
        const foundation::Vec3 perpendicular{-dz / length * half_width, 0.0F,
                                              dx / length * half_width};
        // A river has one water level across a cross section. Sampling the
        // two banks independently made every terrain-cell difference twist
        // the quad, turning a calm river into a chain of dark shards.
        const foundation::Vec3 center = water_surface(point);
        mesh.vertices.push_back({{center.x + perpendicular.x, center.y,
                                 center.z + perpendicular.z},
                                 {0.0F, 1.0F, 0.0F}, {0.0F, travelled_m / half_width}, color});
        mesh.vertices.push_back({{center.x - perpendicular.x, center.y,
                                 center.z - perpendicular.z},
                                 {0.0F, 1.0F, 0.0F}, {1.0F, travelled_m / half_width}, color});
    }
    for (std::size_t index = 1U; index < centerline.size(); ++index) {
        const std::uint32_t previous = base + static_cast<std::uint32_t>((index - 1U) * 2U);
        const std::uint32_t current = base + static_cast<std::uint32_t>(index * 2U);
        mesh.indices.insert(mesh.indices.end(),
                            {previous, previous + 1U, current + 1U,
                             previous, current + 1U, current});
    }
}

} // namespace

foundation::Result<WorldMeshArtifact, foundation::Error>
WorldMeshCompiler::compile(const world::WorldPlan& plan, const terrain::HeightField& terrain,
                           std::span<const buildings::BuildingGenerationResult> resolved_buildings,
                           world::WorldArtifactRevision source_revision,
                           const terrain::TerrainMesh* source_water_mesh) {
    if (plan.map_size_m == 0 || plan.features.empty() || terrain.width() < 2 ||
        terrain.height() < 2 || !std::isfinite(terrain.cellSize()) || terrain.cellSize() <= 0.0F) {
        return foundation::Result<WorldMeshArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid world presentation inputs"});
    }
    if (source_revision == 0U || source_revision != world::artifactRevision(plan)) {
        return foundation::Result<WorldMeshArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "mixed world artifact revision"});
    }

    auto mesh = std::make_shared<render::RenderMesh>();
    render::RenderMesh water_mesh{};
    std::unordered_map<foundation::StableId, WorldMeshDrawRange> part_draw_ranges;
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
        const auto append_result = append_colored_box(*mesh, base, size, color, feature.rotation_y);
        if (!append_result) {
            return foundation::Result<WorldMeshArtifact, foundation::Error>::failure(append_result.error());
        }
    }

    // Roads are compiled from the semantic graph, not from feature-scale
    // boxes.  Centerline endpoint identity and terrain sampling are preserved
    // in this representation compiler.
    for (const roads::RoadEdge& edge : plan.city.road_graph.edges()) {
        append_road_edge(*mesh, edge, terrain);
    }
    for (const hydrology::RiverCrossing& crossing : plan.hydrology.crossings) {
        const float height = crossing.type == hydrology::CrossingType::Bridge ? 0.35F : 0.12F;
        const foundation::Color color = crossing.type == hydrology::CrossingType::Bridge
                                             ? foundation::Color{0.28F, 0.29F, 0.27F, 1.0F}
                                             : foundation::Color{0.40F, 0.34F, 0.24F, 1.0F};
        foundation::Vec3 center = crossing.position;
        center.y += height * 0.5F + 0.08F;
        const auto crossing_result = append_centered_box(
            *mesh, center, {crossing.deck_width_m, height, crossing.deck_width_m}, color, 0.0F);
        if (!crossing_result) {
            return foundation::Result<WorldMeshArtifact, foundation::Error>::failure(
                crossing_result.error());
        }
    }

    // Presentation consumes plans resolved by world orchestration. It must
    // never re-run the building generator: collision, navigation and render
    // need the same semantic BuildingPart IDs and resolutions.
    if (resolved_buildings.size() != plan.building_sites.size()) {
        return foundation::Result<WorldMeshArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "building resolution count does not match world plan"});
    }
    for (const buildings::BuildingGenerationResult& generated : resolved_buildings) {
        const float terrain_height =
            terrain.sampleBilinear(generated.resolution.world_position.x,
                                   generated.resolution.world_position.z);
        for (const buildings::BuildingPart& part : generated.plan.parts) {
            foundation::Vec3 center = rotate_local(generated.resolution.world_position,
                                                    part.center,
                                                    generated.resolution.rotation_y);
            center.y += terrain_height;
            const std::size_t first_index = mesh->indices.size();
            const auto append_result = append_centered_box(
                *mesh, center, part.extent, building_part_color(part.kind),
                generated.resolution.rotation_y);
            if (!append_result) {
                return foundation::Result<WorldMeshArtifact, foundation::Error>::failure(append_result.error());
            }
            const std::size_t index_count = mesh->indices.size() - first_index;
            if (first_index > std::numeric_limits<std::uint32_t>::max() ||
                index_count > std::numeric_limits<std::uint32_t>::max() ||
                !part_draw_ranges.emplace(part.id, WorldMeshDrawRange{
                    static_cast<std::uint32_t>(first_index), static_cast<std::uint32_t>(index_count)}).second) {
                return foundation::Result<WorldMeshArtifact, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState, "duplicate or oversized building part draw range"});
            }
        }
    }

    // Water has its own material range. Prefer the immutable surface already
    // resolved by the world transaction; the hydrology path remains a
    // compatibility fallback for older callers of this presentation compiler.
    if (source_water_mesh != nullptr) {
        const foundation::Color water_color{0.035F, 0.72F, 0.98F, 1.0F};
        water_mesh.vertices.reserve(source_water_mesh->vertices.size());
        water_mesh.indices = source_water_mesh->indices;
        for (const terrain::TerrainMeshVertex& vertex : source_water_mesh->vertices) {
            water_mesh.vertices.push_back(
                {vertex.position, vertex.normal, vertex.uv, water_color});
        }
    } else {
    // Water has its own material range. Appending it after every road,
    // crossing, building and vegetation triangle keeps future blended-water
    // passes isolated from the opaque semantic world geometry.
    for (const hydrology::RiverPath& river : plan.hydrology.rivers) {
        append_river(water_mesh, plan.hydrology, river, terrain);
    }
    }
    const std::size_t opaque_index_count = mesh->indices.size();
    if (!water_mesh.vertices.empty() && !water_mesh.indices.empty()) {
        const std::size_t vertex_offset = mesh->vertices.size();
        if (vertex_offset > std::numeric_limits<std::uint32_t>::max() ||
            water_mesh.vertices.size() > std::numeric_limits<std::uint32_t>::max() -
                                           vertex_offset) {
            return foundation::Result<WorldMeshArtifact, foundation::Error>::failure(
                {foundation::ErrorCode::OutOfRange, "water mesh exceeds index range"});
        }
        mesh->vertices.insert(mesh->vertices.end(), water_mesh.vertices.begin(),
                              water_mesh.vertices.end());
        mesh->indices.reserve(mesh->indices.size() + water_mesh.indices.size());
        for (const std::uint32_t index : water_mesh.indices) {
            mesh->indices.push_back(index + static_cast<std::uint32_t>(vertex_offset));
        }
    }

    if (mesh->vertices.empty() || mesh->indices.empty()) {
        return foundation::Result<WorldMeshArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "world plan compiled to an empty mesh"});
    }
    render::MaterialDescriptor opaque{};
    opaque.material_id = foundation::stable_id("material.world.opaque");
    opaque.revision = 1U;
    opaque.roughness = 0.92F;
    opaque.vertex_color = true;
    render::MaterialDescriptor water{};
    water.material_id = foundation::stable_id("material.world.water");
    water.revision = 1U;
    water.roughness = 0.14F;
    water.opacity = 1.0F;
    water.alpha_mode = render::MaterialAlphaMode::Opaque;
    water.double_sided = true;
    water.vertex_color = true;
    mesh->materials = {opaque, water};
    if (opaque_index_count > 0U) {
        mesh->material_groups.push_back(
            {0U, static_cast<std::uint32_t>(opaque_index_count), 0U});
    }
    const std::size_t water_index_count = mesh->indices.size() - opaque_index_count;
    if (water_index_count > 0U) {
        if (opaque_index_count > std::numeric_limits<std::uint32_t>::max() ||
            water_index_count > std::numeric_limits<std::uint32_t>::max()) {
            return foundation::Result<WorldMeshArtifact, foundation::Error>::failure(
                {foundation::ErrorCode::OutOfRange, "world material draw range exceeds limit"});
        }
        mesh->material_groups.push_back(
            {static_cast<std::uint32_t>(opaque_index_count),
             static_cast<std::uint32_t>(water_index_count), 1U});
    }
    mesh->revision = source_revision;
    auto resolved_water_mesh = std::make_shared<const render::RenderMesh>(
        std::move(water_mesh));
    return foundation::Result<WorldMeshArtifact, foundation::Error>::success(
        {source_revision, std::move(mesh), std::move(resolved_water_mesh),
         std::move(part_draw_ranges)});
}

} // namespace genomes::world_render
