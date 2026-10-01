#include <genomes/gameplay/WorldScenario.hpp>

#include <genomes/foundation/StableHash.hpp>
#include <genomes/proc/SeedPath.hpp>
#include <genomes/terrain/TerrainGenerator.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace genomes::gameplay {

foundation::Result<void, foundation::Error> WorldScenario::requestNew(
    const world::WorldGenerationRequest& request) {
    if (!request.valid() || building_profile_ == nullptr || !building_profile_->frozen()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument,
             "invalid world scenario request or building profile"});
    }
    pending_ = generation_service_.submit(request);
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
    status_.generation_pending = false;
    if (!candidate.has_value() || !validCandidate(*candidate)) {
        status_.last_error = {foundation::ErrorCode::Internal, "invalid generated world candidate"};
        return foundation::Result<bool, foundation::Error>::failure(status_.last_error);
    }
    if (!completed_request.has_value()) {
        status_.last_error = {foundation::ErrorCode::Internal,
                               "world generation completed without a request"};
        return foundation::Result<bool, foundation::Error>::failure(status_.last_error);
    }
    const auto artifact = compileArtifact(std::move(*candidate), *completed_request,
                                          *building_profile_);
    if (!artifact) {
        status_.last_error = artifact.error();
        return foundation::Result<bool, foundation::Error>::failure(status_.last_error);
    }
    active_request_ = completed_request;
    active_artifact_ = std::make_shared<const WorldScenarioArtifact>(
        std::move(artifact.value()));
    status_.has_active_world = true;
    status_.active_content_hash = active_artifact_->plan.content_hash;
    status_.last_error = {};
    streamer_.reset();
    streamed_regions_.clear();
    streaming_tick_ = {};
    world::WorldCoordinateConfig coordinates{};
    coordinates.region_size_m = static_cast<double>(completed_request->map_size_m);
    streamer_ = std::make_unique<world::WorldStreamer>(
        world::WorldId(foundation::stableHashU64(completed_request->seed)),
        *completed_request, coordinates, jobs_);
    // Keep the first region immediately available while an adjacent region is
    // prepared asynchronously through the normal streaming service.
    (void)requestRegion({1, 0, 0});
    return foundation::Result<bool, foundation::Error>::success(true);
}

foundation::Result<void, foundation::Error> WorldScenario::startNew(
    const world::WorldGenerationRequest& request) {
    const auto requested = requestNew(request);
    if (!requested) {
        return requested;
    }
    if (pending_.has_value()) {
        pending_->wait();
    }
    const auto result = poll();
    if (!result) {
        return foundation::Result<void, foundation::Error>::failure(result.error());
    }
    if (!result.value()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::Internal, "world generation did not complete"});
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
    pending_.reset();
    pending_request_.reset();
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
    if (!request.valid() || !validCandidate(plan) || plan.seed != request.seed ||
        !building_profile.frozen()) {
        return foundation::Result<WorldScenarioArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument,
             "world artifact request, generated plan, and building profile do not match"});
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
    terrain_spec.samples_x = layout.sample_count;
    terrain_spec.samples_z = layout.sample_count;
    terrain_spec.cell_size_m = layout.spacing_m;
    terrain_spec.origin_offset_x = static_cast<double>(layout.origin.x);
    terrain_spec.origin_offset_z = static_cast<double>(layout.origin.z);
    const auto terrain_result = terrain::TerrainGenerator::generate(terrain_spec);
    if (!terrain_result) {
        return foundation::Result<WorldScenarioArtifact, foundation::Error>::failure(
            terrain_result.error());
    }
    terrain::HeightField terrain_field = terrain_result.value();
    const auto mesh_result = terrain::TerrainMeshBuilder::build(terrain_field);
    if (!mesh_result) {
        return foundation::Result<WorldScenarioArtifact, foundation::Error>::failure(
            mesh_result.error());
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
    for (const world::BuildingSiteRequest& site : artifact.plan.building_sites) {
        auto building = buildings::BuildingGenerator::generateSite(
            site, building_profile.siteGeneration());
        if (!building) {
            return foundation::Result<ResolvedWorldArtifacts, foundation::Error>::failure(
                building.error());
        }
        resolved_buildings->push_back(std::move(building.value()));
    }
    artifact.resolved_buildings = std::move(resolved_buildings);
    artifact.terrain = std::make_shared<const terrain::HeightField>(std::move(terrain_field));
    artifact.terrain_mesh = std::make_shared<const terrain::TerrainMesh>(
        std::move(mesh_result.value()));
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
