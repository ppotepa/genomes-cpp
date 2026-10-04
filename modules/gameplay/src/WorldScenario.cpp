#include <genomes/gameplay/WorldScenario.hpp>

#include <genomes/buildings/BuildingProcedural.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/jobs/JobGraph.hpp>
#include <genomes/proc/SeedPath.hpp>
#include <genomes/terrain/TerrainGenerator.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <span>
#include <utility>

namespace genomes::gameplay {
namespace {

[[nodiscard]] hydrology::TributaryDensity hydrologyTributaryDensity(
    world::TributaryDensity density) noexcept {
    switch (density) {
    case world::TributaryDensity::None: return hydrology::TributaryDensity::None;
    case world::TributaryDensity::Low: return hydrology::TributaryDensity::Low;
    case world::TributaryDensity::Medium: return hydrology::TributaryDensity::Medium;
    }
    return hydrology::TributaryDensity::None;
}

[[nodiscard]] hydrology::HydrologySpec makeHydrologySpec(
    const world::WorldGenerationRequest& request, const terrain::HeightField& terrain) noexcept {
    hydrology::HydrologySpec spec{};
    spec.seed = request.seed;
    spec.map_size_m = request.map_size_m;
    spec.cells_x = terrain.width() - 1U;
    spec.cells_z = terrain.height() - 1U;
    spec.cell_size_m = terrain.cellSize();
    spec.mode = request.hydrology_mode;
    spec.river_probability = request.river_probability;
    spec.main_river_min = request.hydrology.main_river_min;
    spec.main_river_max = request.hydrology.main_river_max;
    spec.tributary_density = hydrologyTributaryDensity(
        request.hydrology.tributary_density);
    spec.stream_width_min_m = request.hydrology.stream_width_min_m;
    spec.stream_width_max_m = request.hydrology.stream_width_max_m;
    spec.river_width_min_m = request.hydrology.river_width_min_m;
    spec.river_width_max_m = request.hydrology.river_width_max_m;
    spec.depth_min_m = request.hydrology.depth_min_m;
    spec.depth_max_m = request.hydrology.depth_max_m;
    spec.meander_strength = request.hydrology.meander_strength;
    spec.valley_width_min_m = request.hydrology.valley_width_min_m;
    spec.valley_width_max_m = request.hydrology.valley_width_max_m;
    return spec;
}

[[nodiscard]] bool segmentIntersection(foundation::Vec3 a, foundation::Vec3 b,
                                       foundation::Vec3 c, foundation::Vec3 d,
                                       foundation::Vec3& intersection) noexcept {
    const float ab_x = b.x - a.x;
    const float ab_z = b.z - a.z;
    const float cd_x = d.x - c.x;
    const float cd_z = d.z - c.z;
    const float denominator = ab_x * cd_z - ab_z * cd_x;
    if (std::abs(denominator) < 1.0e-5F) return false;
    const float ac_x = c.x - a.x;
    const float ac_z = c.z - a.z;
    const float along_ab = (ac_x * cd_z - ac_z * cd_x) / denominator;
    const float along_cd = (ac_x * ab_z - ac_z * ab_x) / denominator;
    if (along_ab < 0.0F || along_ab > 1.0F || along_cd < 0.0F || along_cd > 1.0F)
        return false;
    intersection = {a.x + ab_x * along_ab, std::lerp(c.y, d.y, along_cd),
                    a.z + ab_z * along_ab};
    return true;
}

void resolveCrossings(hydrology::HydrologyArtifact& hydrology_artifact,
                      const roads::RoadGraph& roads) {
    for (const roads::RoadEdge& road : roads.edges()) {
        const auto road_points = road.centerline.samplePoints();
        for (const hydrology::RiverPath& river : hydrology_artifact.rivers) {
            const std::size_t begin = river.point_offset;
            const std::size_t end = begin + river.point_count;
            if (end > hydrology_artifact.river_points.size()) continue;
            bool found = false;
            for (std::size_t road_point = 1U; road_point < road_points.size() && !found;
                 ++road_point) {
                for (std::size_t river_point = begin + 1U; river_point < end; ++river_point) {
                    foundation::Vec3 position{};
                    if (!segmentIntersection(road_points[road_point - 1U], road_points[road_point],
                                             hydrology_artifact.river_points[river_point - 1U],
                                             hydrology_artifact.river_points[river_point],
                                             position)) continue;
                    const auto type = river.width_m <= 3.0F
                                          ? hydrology::CrossingType::Culvert
                                          : (river.depth_m <= 0.5F
                                                 ? hydrology::CrossingType::Ford
                                                 : hydrology::CrossingType::Bridge);
                    const auto id = foundation::stableHashCombine(road.id, river.id);
                    hydrology_artifact.crossings.push_back(
                        {id == 0U ? 1U : id, river.id, road.id, position,
                         std::max(road.width, river.width_m + 2.0F), type});
                    hydrology_artifact.content_hash = foundation::stableHashCombine(
                        hydrology_artifact.content_hash, id == 0U ? 1U : id);
                    hydrology_artifact.content_hash = foundation::stableHashCombine(
                        hydrology_artifact.content_hash, static_cast<std::uint64_t>(type));
                    found = true;
                    break;
                }
            }
        }
    }
}

[[nodiscard]] foundation::Result<terrain::TerrainMesh, foundation::Error> buildWaterMesh(
    const hydrology::HydrologyArtifact& hydrology, const terrain::HeightField& terrain_field) {
    terrain::TerrainMesh mesh{};
    mesh.source_width = terrain_field.width();
    mesh.source_height = terrain_field.height();
    mesh.sample_step = 1U;
    for (const hydrology::RiverPath& river : hydrology.rivers) {
        if (river.point_count < 2U ||
            static_cast<std::size_t>(river.point_offset) + river.point_count >
                hydrology.river_points.size()) {
            continue;
        }
        const std::size_t begin = river.point_offset;
        const std::size_t end = begin + river.point_count;
        const float half_width = std::max(0.5F, river.width_m * 0.5F);
        const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
        for (std::size_t index = begin; index < end; ++index) {
            const foundation::Vec3 point = hydrology.river_points[index];
            const foundation::Vec3 before =
                hydrology.river_points[index == begin ? index : index - 1U];
            const foundation::Vec3 after =
                hydrology.river_points[index + 1U == end ? index : index + 1U];
            const float dx = after.x - before.x;
            const float dz = after.z - before.z;
            const float length = std::hypot(dx, dz);
            if (!std::isfinite(length) || length <= 0.001F) continue;
            const foundation::Vec3 perpendicular{-dz / length * half_width, 0.0F,
                                                  dx / length * half_width};
            const float water_y = std::max(point.y + 0.12F,
                                           terrain_field.sampleBilinear(point.x, point.z) + 0.10F);
            const foundation::Vec3 center{point.x, water_y, point.z};
            mesh.vertices.push_back({{center.x + perpendicular.x, center.y,
                                      center.z + perpendicular.z},
                                     {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F}});
            mesh.vertices.push_back({{center.x - perpendicular.x, center.y,
                                      center.z - perpendicular.z},
                                     {0.0F, 1.0F, 0.0F}, {1.0F, 0.0F}});
        }
        const std::size_t vertex_count = mesh.vertices.size() - base;
        if (vertex_count < 4U) continue;
        for (std::size_t pair = 1U; pair < vertex_count / 2U; ++pair) {
            const std::uint32_t previous = base + static_cast<std::uint32_t>((pair - 1U) * 2U);
            const std::uint32_t current = base + static_cast<std::uint32_t>(pair * 2U);
            mesh.indices.insert(mesh.indices.end(),
                                {previous, previous + 1U, current + 1U,
                                 previous, current + 1U, current});
        }
    }
    return foundation::Result<terrain::TerrainMesh, foundation::Error>::success(std::move(mesh));
}

} // namespace

foundation::Result<void, foundation::Error> WorldScenario::requestNew(
    const world::WorldGenerationRequest& request) {
    if (!request.valid() || building_profile_ == nullptr || !building_profile_->frozen()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument,
             "invalid world scenario request or building profile"});
    }
    if (pending_artifact_ticket_.valid() && !pending_artifact_ticket_.complete()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState,
             "world artifact compilation is already in progress"});
    }
    pending_ = generation_service_.submit(request, &generation_channel_);
    pending_request_ = request;
    status_.generation_pending = true;
    status_.last_error = {};
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<bool, foundation::Error> WorldScenario::poll() {
    if (streamer_) {
        streaming_tick_.increment();
        const auto streamed = streamer_->poll(streaming_tick_);
        if (!streamed) {
            status_.last_error = streamed.error();
            return foundation::Result<bool, foundation::Error>::failure(status_.last_error);
        }
        auto ready_regions = streamer_->takeReady();
        for (auto& region : ready_regions) {
            streamed_regions_.push_back(std::move(region));
        }
        status_.streaming_pending = streamer_->pendingCount();
        status_.streaming_resident = streamer_->residentCount();
    }
    if (pending_artifact_ticket_.valid()) {
        if (!pending_artifact_ticket_.complete()) {
            return foundation::Result<bool, foundation::Error>::success(false);
        }
        const auto completed_request = pending_artifact_request_;
        const auto artifact = pending_artifact_ticket_.artifact();
        const auto artifact_error = pending_artifact_ticket_.error();
        const auto artifact_status = pending_artifact_ticket_.status();
        pending_artifact_ticket_ = {};
        pending_artifact_request_.reset();
        if (artifact_status != proc::GenerationStatus::Completed || artifact == nullptr) {
            status_.generation_pending = false;
            status_.last_error = artifact_error.code == foundation::ErrorCode::None
                                     ? foundation::Error{foundation::ErrorCode::Internal,
                                                         "world artifact ticket produced no result"}
                                     : artifact_error;
            return foundation::Result<bool, foundation::Error>::failure(status_.last_error);
        }
        if (!completed_request.has_value()) {
            status_.generation_pending = false;
            status_.last_error = {foundation::ErrorCode::Internal,
                                  "world artifact completed without a request"};
            return foundation::Result<bool, foundation::Error>::failure(status_.last_error);
        }
        active_request_ = completed_request;
        active_artifact_ = std::move(artifact);
        status_.has_active_world = true;
        status_.generation_pending = false;
        status_.active_content_hash = active_artifact_->plan.content_hash;
        status_.last_error = {};
        streamer_.reset();
        streamed_regions_.clear();
        streaming_tick_ = {};
        world::WorldCoordinateConfig coordinates{};
        coordinates.region_size_m = static_cast<double>(completed_request->map_size_m);
        streamer_ = std::make_unique<world::WorldStreamer>(
            world::WorldId(foundation::stableHashU64(completed_request->seed)),
            *completed_request, coordinates, jobs_, world::WorldStreamerConfig{},
            generation_service_.registry());
        (void)requestRegion({1, 0, 0});
        return foundation::Result<bool, foundation::Error>::success(true);
    }
    if (!pending_.has_value()) {
        return foundation::Result<bool, foundation::Error>::success(false);
    }
    if (!pending_->ready()) {
        return foundation::Result<bool, foundation::Error>::success(false);
    }
    if (pending_->failed()) {
        status_.last_error = pending_->error();
        status_.generation_pending = false;
        pending_request_.reset();
        pending_.reset();
        return foundation::Result<bool, foundation::Error>::failure(status_.last_error);
    }
    std::optional<world::WorldPlan> candidate = pending_->take_result();
    const std::optional<world::WorldGenerationRequest> completed_request = pending_request_;
    pending_request_.reset();
    pending_.reset();
    if (!candidate.has_value() || !validCandidate(*candidate)) {
        status_.generation_pending = false;
        status_.last_error = {foundation::ErrorCode::Internal, "invalid generated world candidate"};
        return foundation::Result<bool, foundation::Error>::failure(status_.last_error);
    }
    if (!completed_request.has_value()) {
        status_.generation_pending = false;
        status_.last_error = {foundation::ErrorCode::Internal,
                               "world generation completed without a request"};
        return foundation::Result<bool, foundation::Error>::failure(status_.last_error);
    }
    pending_artifact_request_ = *completed_request;
    const auto profile = building_profile_;
    proc::ProceduralRuntime* runtime = generation_service_.ownsRuntime()
                                           ? nullptr
                                           : generation_service_.runtime();
    proc::GenerationOptions artifact_options{};
    artifact_options.input_hash = foundation::stableHashU64(completed_request->seed);
    artifact_options.retained_bytes = sizeof(WorldScenarioArtifact);
    artifact_options.use_cache = false;
    pending_artifact_ticket_ = generation_service_.runtime()->requestStage<WorldScenarioArtifact>(
        proc::generatorId("world.resolved"),
        proc::SeedPath(completed_request->seed).child("resolved-world", 0),
        artifact_options,
        [plan = std::move(*candidate), request = *completed_request, profile,
         runtime](proc::GenerationContext& context) mutable
            -> foundation::Result<std::shared_ptr<const WorldScenarioArtifact>, foundation::Error> {
            if (context.cancellationRequested()) {
                return foundation::Result<std::shared_ptr<const WorldScenarioArtifact>,
                                          foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState, "world artifact generation canceled"});
            }
            auto result = compileArtifactImpl(
                std::move(plan), request, *profile, runtime, &context);
            if (!result) {
                return foundation::Result<std::shared_ptr<const WorldScenarioArtifact>,
                                          foundation::Error>::failure(result.error());
            }
            return foundation::Result<std::shared_ptr<const WorldScenarioArtifact>,
                                      foundation::Error>::success(
                std::make_shared<const WorldScenarioArtifact>(std::move(result.value())));
        },
        &generation_channel_);
    return foundation::Result<bool, foundation::Error>::success(false);
}

foundation::Result<void, foundation::Error> WorldScenario::startNew(
    const world::WorldGenerationRequest& request) {
    const auto requested = requestNew(request);
    if (!requested) {
        return requested;
    }
    for (;;) {
        const auto result = poll();
        if (!result) {
            return foundation::Result<void, foundation::Error>::failure(result.error());
        }
        if (result.value()) break;
        if (pending_artifact_ticket_.valid()) {
            pending_artifact_ticket_.wait();
        } else if (pending_.has_value()) {
            pending_->wait();
        } else {
            return foundation::Result<void, foundation::Error>::failure(
                {foundation::ErrorCode::Internal, "world generation did not complete"});
        }
    }
    active_request_ = request;
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> WorldScenario::requestRegion(
    world::RegionCoord coordinate, world::ResidencyMask desired) {
    if (!active_request_ || !streamer_) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "no active world streamer"});
    }
    const world::WorldId world_id =
        world::WorldId(foundation::stableHashU64(active_request_->seed));
    const world::RegionId id = world::regionId(world_id, coordinate);
    const world::ResidencyRequest request{
        id, coordinate, desired, world::ResidencyReason::Camera, 1U, 0U, streaming_tick_};
    const auto result = streamer_->setDesired(request);
    if (!result) {
        status_.last_error = result.error();
        return result;
    }
    status_.streaming_pending = streamer_->pendingCount();
    status_.streaming_resident = streamer_->residentCount();
    return foundation::Result<void, foundation::Error>::success();
}

std::vector<world::StreamedRegion> WorldScenario::takeStreamedRegions() {
    std::vector<world::StreamedRegion> result;
    result.swap(streamed_regions_);
    return result;
}

void WorldScenario::cancelPending() noexcept {
    if (pending_.has_value()) {
        pending_->cancel();
    }
    pending_.reset();
    pending_request_.reset();
    pending_artifact_ticket_.cancel();
    pending_artifact_ticket_ = {};
    pending_artifact_request_.reset();
    status_.generation_pending = false;
}

const world::WorldPlan* WorldScenario::activePlan() const noexcept {
    return active_artifact_ != nullptr ? &active_artifact_->plan : nullptr;
}

const WorldScenarioArtifact* WorldScenario::activeArtifact() const noexcept {
    return active_artifact_.get();
}

const world::WorldGenerationRequest* WorldScenario::activeRequest() const noexcept {
    return active_request_ ? &*active_request_ : nullptr;
}

WorldSemanticSnapshot WorldScenario::semanticSnapshot() const noexcept {
    if (active_artifact_ == nullptr) {
        return {};
    }
    const world::WorldPlan& plan = active_artifact_->plan;
    return {plan.content_hash,
            plan.generator_version,
            plan.map_size_m,
            plan.features.size(),
            plan.count(world::WorldFeatureKind::Road),
            plan.count(world::WorldFeatureKind::Building),
            plan.count(world::WorldFeatureKind::Vegetation),
            plan.building_sites.size(),
            plan.city.parcels.size(),
            plan.hydrology.rivers.size(),
            active_artifact_->terrain
                ? static_cast<std::size_t>(active_artifact_->terrain->width()) *
                      active_artifact_->terrain->height()
                : 0U};
}

bool WorldScenario::validCandidate(const world::WorldPlan& plan) noexcept {
    return plan.generator_version == world::WorldGeneratorVersion && plan.seed != 0U &&
           plan.map_size_m >= 128U && !plan.features.empty() && plan.content_hash != 0U &&
           plan.hasValidStageFingerprints();
}

foundation::Result<WorldScenarioArtifact, foundation::Error> WorldScenario::compileArtifact(
    world::WorldPlan plan, const world::WorldGenerationRequest& request,
    const buildings::FrozenBuildingProfile& building_profile) {
    return compileArtifactImpl(
        std::move(plan), request, building_profile, nullptr, nullptr);
}

foundation::Result<WorldScenarioArtifact, foundation::Error> WorldScenario::compileArtifactImpl(
    world::WorldPlan plan, const world::WorldGenerationRequest& request,
    const buildings::FrozenBuildingProfile& building_profile,
    proc::ProceduralRuntime* procedural_runtime,
    proc::GenerationContext* generation_context) {
    if (!request.valid() || !validCandidate(plan) || plan.seed != request.seed ||
        !building_profile.frozen()) {
        return foundation::Result<WorldScenarioArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument,
             "world artifact request, generated plan, and building profile do not match"});
    }

    if (procedural_runtime != nullptr) {
        proc::GenerationPipeline pipeline;
        pipeline.addStage(proc::generatorId("terrain.height-field"));
        pipeline.addStage(proc::generatorId("hydrology.artifact"));
        pipeline.addStage(proc::generatorId("roads.graph"));
        pipeline.addStage(proc::generatorId("buildings.site"));
        pipeline.addStage(proc::generatorId("world.resolved"));
        const auto pipeline_valid = pipeline.validate(procedural_runtime->registry());
        if (!pipeline_valid) {
            return foundation::Result<WorldScenarioArtifact, foundation::Error>::failure(
                pipeline_valid.error());
        }
    }

    const world::GridLayout layout = world::GridLayout::forMap(request.map_size_m);
    if (!layout.valid()) {
        return foundation::Result<WorldScenarioArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "world request has no valid grid layout"});
    }

    terrain::TerrainSpec terrain_spec{};
    terrain_spec.world_id = world::WorldId(foundation::stableHashU64(request.seed));
    terrain_spec.region = {0, 0, 0};
    terrain_spec.coordinates.region_size_m = static_cast<double>(request.map_size_m);
    terrain_spec.seed_path = proc::SeedPath(request.seed).child("terrain", 0);
    const std::uint32_t terrain_cell_count =
        request.map_size_m / request.terrain.sample_spacing_m;
    terrain_spec.samples_x = terrain_cell_count + 1U;
    terrain_spec.samples_z = terrain_cell_count + 1U;
    terrain_spec.cell_size_m = static_cast<float>(request.terrain.sample_spacing_m);
    terrain_spec.origin_offset_x = static_cast<double>(layout.origin.x);
    terrain_spec.origin_offset_z = static_cast<double>(layout.origin.z);
    terrain_spec.generation = request.terrain;
    foundation::Result<terrain::HeightField, foundation::Error> terrain_result =
        foundation::Result<terrain::HeightField, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "terrain generator unavailable"});
    if (procedural_runtime != nullptr &&
        procedural_runtime->registry().find(proc::generatorId("terrain.height-field")) !=
            nullptr) {
        proc::GenerationRequest<terrain::TerrainSpec, terrain::HeightField> generation;
        generation.generator = proc::generatorId("terrain.height-field");
        generation.input = std::make_shared<const terrain::TerrainSpec>(terrain_spec);
        generation.seed_path = terrain_spec.seed_path;
        std::uint64_t terrain_input_hash = foundation::stableHashCombine(
            foundation::stableHashU64(request.seed), request.map_size_m);
        terrain_input_hash = foundation::stableHashCombine(
            terrain_input_hash, static_cast<std::uint64_t>(request.terrain.preset));
        terrain_input_hash = foundation::stableHashCombine(
            terrain_input_hash, request.terrain.sample_spacing_m);
        terrain_input_hash = foundation::stableHashCombine(
            terrain_input_hash, foundation::stableHashFloat(request.terrain.elevation_range_m));
        terrain_input_hash = foundation::stableHashCombine(
            terrain_input_hash, foundation::stableHashFloat(request.terrain.landform_scale_m));
        terrain_input_hash = foundation::stableHashCombine(
            terrain_input_hash, foundation::stableHashFloat(request.terrain.roughness));
        generation.options.input_hash = terrain_input_hash;
        generation.options.retained_bytes = sizeof(terrain::HeightField);
        if (generation_context == nullptr) {
            return foundation::Result<WorldScenarioArtifact, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState,
                 "live world composition is missing its procedural context"});
        }
        const auto generated = procedural_runtime->generateInline(
            std::move(generation), *generation_context);
        if (generated) {
            terrain_result = foundation::Result<terrain::HeightField, foundation::Error>::success(
                *generated.value());
        } else {
            terrain_result = foundation::Result<terrain::HeightField, foundation::Error>::failure(
                generated.error());
        }
    } else if (procedural_runtime == nullptr) {
        // Direct generation is reserved for the explicit deterministic
        // compileArtifact tool/test entry point. Live scenes must use the
        // composition-root registry.
        terrain_result = terrain::TerrainGenerator::generate(terrain_spec);
    }
    if (!terrain_result) {
        return foundation::Result<WorldScenarioArtifact, foundation::Error>::failure(
            terrain_result.error());
    }
    terrain::HeightField terrain_field = terrain_result.value();

    // Roads are a first-class procedural stage.  The world plan still carries
    // the deterministic city seed and parcel semantics, but the production
    // composition root owns the canonical road artifact consumed by crossings
    // and presentation.  Deterministic compileArtifact keeps the direct city
    // result as its explicit tool/test path.
    if (procedural_runtime != nullptr &&
        procedural_runtime->registry().find(proc::generatorId("roads.graph")) != nullptr) {
        const world::CityGenerationRequest city_request{
            request.seed, request.map_size_m, request.buildings, request.fenced_parcels};
        proc::GenerationRequest<world::CityGenerationRequest, roads::RoadGraph> generation;
        generation.generator = proc::generatorId("roads.graph");
        generation.input = std::make_shared<const world::CityGenerationRequest>(city_request);
        generation.seed_path = proc::SeedPath(request.seed).child("roads", 0);
        generation.options.input_hash = foundation::stableHashCombine(
            foundation::stableHashU64(request.seed), request.map_size_m);
        generation.options.input_hash = foundation::stableHashCombine(
            generation.options.input_hash, foundation::stableHashFloat(request.buildings));
        generation.options.input_hash = foundation::stableHashCombine(
            generation.options.input_hash, foundation::stableHashFloat(request.fenced_parcels));
        generation.options.dependency_hash = plan.stage_fingerprints[
            static_cast<std::size_t>(world::WorldGenerationStage::Roads)]
                                                   .dependency_fingerprint;
        generation.options.retained_bytes = sizeof(roads::RoadGraph);
        if (generation_context == nullptr) {
            return foundation::Result<WorldScenarioArtifact, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState,
                 "live road composition is missing its procedural context"});
        }
        const auto generated = procedural_runtime->generateInline(
            std::move(generation), *generation_context);
        if (!generated || !generated.value()) {
            return foundation::Result<WorldScenarioArtifact, foundation::Error>::failure(
                generated ? foundation::Error{foundation::ErrorCode::Internal,
                                              "road generator returned null"}
                          : generated.error());
        }
        plan.city.road_graph = *generated.value();
    } else if (procedural_runtime != nullptr) {
        return foundation::Result<WorldScenarioArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "roads generator unavailable"});
    }

    const hydrology::HydrologySpec hydrology_spec = makeHydrologySpec(request, terrain_field);
    const hydrology::HydrologyTerrainView terrain_view{
        terrain_field.width(), terrain_field.height(), terrain_field.cellSize(),
        static_cast<float>(terrain_field.originX()), static_cast<float>(terrain_field.originZ()),
        terrain_field.samples()};
    foundation::Result<hydrology::HydrologyArtifact, foundation::Error> hydrology_result =
        foundation::Result<hydrology::HydrologyArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "hydrology generator unavailable"});
    if (procedural_runtime != nullptr &&
        procedural_runtime->registry().find(proc::generatorId("hydrology.artifact")) != nullptr) {
        auto heights = std::make_shared<const std::vector<float>>(
            terrain_field.samples().begin(), terrain_field.samples().end());
        hydrology::HydrologyGenerationInput input{};
        input.spec = hydrology_spec;
        input.samples_x = terrain_field.width();
        input.samples_z = terrain_field.height();
        input.cell_size_m = terrain_field.cellSize();
        input.origin_x = static_cast<float>(terrain_field.originX());
        input.origin_z = static_cast<float>(terrain_field.originZ());
        input.heights = std::move(heights);
        proc::GenerationRequest<hydrology::HydrologyGenerationInput,
                                hydrology::HydrologyArtifact> generation;
        generation.generator = proc::generatorId("hydrology.artifact");
        generation.input = std::make_shared<const hydrology::HydrologyGenerationInput>(input);
        generation.seed_path = proc::SeedPath(request.seed).child("hydrology", 0);
        std::uint64_t terrain_hash = foundation::stableHashU64(terrain_field.width());
        terrain_hash = foundation::stableHashCombine(terrain_hash, terrain_field.height());
        for (const float height : terrain_field.samples()) {
            terrain_hash = foundation::stableHashCombine(
                terrain_hash, foundation::stableHashFloat(height));
        }
        generation.options.input_hash = foundation::stableHashCombine(
            foundation::stableHashU64(request.seed), terrain_hash);
        generation.options.dependency_hash = terrain_hash;
        generation.options.retained_bytes = sizeof(hydrology::HydrologyArtifact);
        if (generation_context == nullptr) {
            return foundation::Result<WorldScenarioArtifact, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState,
                 "live hydrology composition is missing its procedural context"});
        }
        const auto generated = procedural_runtime->generateInline(
            std::move(generation), *generation_context);
        if (generated) {
            hydrology_result = foundation::Result<hydrology::HydrologyArtifact,
                                                   foundation::Error>::success(
                *generated.value());
        } else {
            hydrology_result = foundation::Result<hydrology::HydrologyArtifact,
                                                   foundation::Error>::failure(
                generated.error());
        }
    } else if (procedural_runtime == nullptr) {
        // Explicit deterministic compileArtifact remains the only direct
        // generator entry point; live scenarios always use ProceduralRuntime.
        hydrology_result = hydrology::HydrologyGenerator::generate(hydrology_spec, terrain_view);
    }
    if (!hydrology_result) {
        return foundation::Result<WorldScenarioArtifact, foundation::Error>::failure(
            hydrology_result.error());
    }
    plan.hydrology = std::move(hydrology_result.value());
    resolveCrossings(plan.hydrology, plan.city.road_graph);
    // The resolved hydrology stage is part of the immutable world identity.
    // This keeps revision/content hashes tied to the exact heightfield-aware
    // artifact rather than the preliminary plan produced by WorldGenerator.
    auto& hydrology_stage = plan.stage_fingerprints[
        static_cast<std::size_t>(world::WorldGenerationStage::Hydrology)];
    if (hydrology_stage.dependency_fingerprint != plan.hydrology.content_hash) {
        plan.content_hash = foundation::stableHashCombine(
            plan.content_hash, plan.hydrology.content_hash == 0U
                                   ? foundation::stableHashU64(1U)
                                   : plan.hydrology.content_hash);
    }

    const auto conflicts_with_water = [&plan](foundation::Vec3 position) {
        const auto water = plan.hydrology.sampleWater(position.x, position.z);
        return water.has_water || water.distance_m < 2.0F;
    };
    const auto conflicts_with_building = [&plan, &conflicts_with_water](
                                             foundation::Vec3 position) {
        return conflicts_with_water(position) ||
               plan.hydrology.isFloodplain(position.x, position.z);
    };
    std::erase_if(plan.building_sites, [&conflicts_with_building](const auto& site) {
        return conflicts_with_building(site.preferred_position);
    });
    std::erase_if(plan.features, [&conflicts_with_building, &conflicts_with_water](
                                     const world::WorldFeature& feature) {
        switch (feature.kind) {
        case world::WorldFeatureKind::Building:
        case world::WorldFeatureKind::Fence:
        case world::WorldFeatureKind::Parcel:
            return conflicts_with_building(feature.position);
        case world::WorldFeatureKind::Vegetation:
            return conflicts_with_water(feature.position);
        case world::WorldFeatureKind::TerrainPatch:
        case world::WorldFeatureKind::Road:
            return false;
        }
        return false;
    });

    std::vector<terrain::TerrainChannel> channels;
    channels.reserve(plan.hydrology.rivers.size());
    for (const hydrology::RiverPath& river : plan.hydrology.rivers) {
        const std::size_t begin = river.point_offset;
        const std::size_t end = begin + river.point_count;
        if (river.point_count < 2U || end > plan.hydrology.river_points.size()) continue;
        channels.push_back({river.id,
                            std::span<const foundation::Vec3>(plan.hydrology.river_points)
                                .subspan(begin, river.point_count),
                            river.width_m, river.depth_m, river.valley_width_m});
    }
    const auto carved = terrain::TerrainGenerator::carveChannels(terrain_field, channels);
    if (!carved) {
        return foundation::Result<WorldScenarioArtifact, foundation::Error>::failure(
            carved.error());
    }

    auto& hydrology_fingerprint = plan.stage_fingerprints[
        static_cast<std::size_t>(world::WorldGenerationStage::Hydrology)];
    hydrology_fingerprint.version = hydrology::HydrologyGeneratorVersion;
    hydrology_fingerprint.dependency_fingerprint = plan.hydrology.content_hash == 0U
                                                       ? 1U
                                                       : plan.hydrology.content_hash;
    const auto mesh_result = terrain::TerrainMeshBuilder::build(terrain_field);
    if (!mesh_result) {
        return foundation::Result<WorldScenarioArtifact, foundation::Error>::failure(
            mesh_result.error());
    }
    const auto water_mesh_result = buildWaterMesh(plan.hydrology, terrain_field);
    if (!water_mesh_result) {
        return foundation::Result<WorldScenarioArtifact, foundation::Error>::failure(
            water_mesh_result.error());
    }

    ResolvedWorldArtifacts artifact{};
    artifact.plan = std::move(plan);
    artifact.revision = world::artifactRevision(artifact.plan);
    destruction::DestructionInvalidationConfig invalidation_config{};
    invalidation_config.coordinates.region_size_m =
        static_cast<double>(artifact.plan.map_size_m);
    artifact.destruction_invalidations =
        std::make_shared<destruction::DestructionInvalidationQueue>(invalidation_config);
    artifact.destruction_invalidations->bindWorldRevision(artifact.revision);
    auto resolved_buildings = std::make_shared<std::vector<buildings::BuildingGenerationResult>>();
    resolved_buildings->reserve(artifact.plan.building_sites.size());
    if (procedural_runtime != nullptr) {
        if (generation_context == nullptr || generation_context->job() == nullptr) {
            return foundation::Result<ResolvedWorldArtifacts, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState,
                 "live building composition is missing its worker context"});
        }
        using BuildingResult = foundation::Result<
            std::shared_ptr<const buildings::BuildingGenerationResult>, foundation::Error>;
        std::vector<std::optional<BuildingResult>> generated_buildings(
            artifact.plan.building_sites.size());
        jobs::JobGraphBuilder builder;
        jobs::JobOptions options;
        options.lane = jobs::ExecutionLane::Worker;
        options.work_class = jobs::WorkClass::Procedural;
        options.priority = jobs::JobPriority::Normal;
        const jobs::CancelToken parent_cancellation =
            generation_context->cancellationToken();
        const jobs::CancelToken superseded =
            generation_context->supersededToken();
        proc::ArtifactReader* artifact_reader = generation_context->artifacts();
        proc::GenerationDiagnostics* diagnostics = generation_context->diagnostics();
        const foundation::StableId building_profile_hash =
            building_profile.fingerprint().value;
        for (std::size_t index = 0U; index < artifact.plan.building_sites.size(); ++index) {
            const world::BuildingSiteRequest site = artifact.plan.building_sites[index];
            const buildings::BuildingSiteGenerationProfile profile =
                building_profile.siteGeneration();
            (void)builder.add(
                [procedural_runtime, site, profile, index, &generated_buildings,
                 parent_cancellation, superseded, artifact_reader, diagnostics,
                 building_profile_hash](
                    jobs::JobContext& job) {
                    proc::GenerationContext child_context(
                        proc::SeedPath(site.seed), &job, parent_cancellation,
                        superseded, artifact_reader, diagnostics);
                    proc::GenerationRequest<buildings::BuildingSiteGenerationRequest,
                                            buildings::BuildingGenerationResult> generation;
                    generation.generator = proc::generatorId("buildings.site");
                    generation.input =
                        std::make_shared<const buildings::BuildingSiteGenerationRequest>(
                            buildings::BuildingSiteGenerationRequest{site, profile});
                    generation.seed_path = proc::SeedPath(site.seed);
                    generation.options.dependency_hash = building_profile_hash;
                    generation.options.retained_bytes =
                        sizeof(buildings::BuildingGenerationResult);
                    generated_buildings[index].emplace(
                        procedural_runtime->generateInline(
                            std::move(generation), child_context));
                },
                options);
        }
        auto building_group = std::move(builder).build().run(
            generation_context->job()->system());
        building_group.wait();
        if (building_group.failed()) {
            return foundation::Result<ResolvedWorldArtifacts, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "parallel building generation job failed"});
        }
        for (std::size_t index = 0U; index < generated_buildings.size(); ++index) {
            if (!generated_buildings[index].has_value()) {
                return foundation::Result<ResolvedWorldArtifacts, foundation::Error>::failure(
                    {foundation::ErrorCode::Internal,
                     "parallel building generation produced no result"});
            }
            auto& generated = *generated_buildings[index];
            if (!generated || !generated.value()) {
                return foundation::Result<ResolvedWorldArtifacts, foundation::Error>::failure(
                    generated ? foundation::Error{foundation::ErrorCode::Internal,
                                                  "building generator returned null"}
                              : generated.error());
            }
            resolved_buildings->push_back(*generated.value());
        }
    } else {
        for (const world::BuildingSiteRequest& site : artifact.plan.building_sites) {
            auto building = buildings::BuildingGenerator::generateSite(
                site, building_profile.siteGeneration());
            if (!building) {
                return foundation::Result<ResolvedWorldArtifacts, foundation::Error>::failure(
                    building.error());
            }
            resolved_buildings->push_back(std::move(building.value()));
        }
    }
    artifact.resolved_buildings = std::move(resolved_buildings);
    artifact.terrain = std::make_shared<const terrain::HeightField>(std::move(terrain_field));
    artifact.terrain_mesh = std::make_shared<const terrain::TerrainMesh>(
        std::move(mesh_result.value()));
    artifact.water_mesh = std::make_shared<const terrain::TerrainMesh>(
        std::move(water_mesh_result.value()));
    world::WorldSaveModel save{};
    save.metadata.generator_version = artifact.plan.generator_version;
    save.metadata.seed = artifact.plan.seed;
    save.metadata.content_hash = artifact.plan.content_hash;
    save.metadata.catalog_hash = foundation::stable_id("catalog.world");
    save.regions.push_back({world::regionId(terrain_spec.world_id, {0, 0, 0}),
                            artifact.plan.content_hash, {}});
    const auto serialized_save = world::WorldSaveCodec::serialize(save);
    if (!serialized_save) {
        return foundation::Result<WorldScenarioArtifact, foundation::Error>::failure(
            serialized_save.error());
    }
    artifact.save_package = std::make_shared<const std::vector<std::byte>>(
        std::move(serialized_save.value()));
    if (!artifact.valid()) {
        return foundation::Result<WorldScenarioArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::Internal, "world artifact compilation produced no data"});
    }
    return foundation::Result<WorldScenarioArtifact, foundation::Error>::success(
        std::move(artifact));
}

} // namespace genomes::gameplay
