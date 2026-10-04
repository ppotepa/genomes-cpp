#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/buildings/BuildingModel.hpp>
#include <genomes/buildings/BuildingProfile.hpp>
#include <genomes/destruction/DestructionInvalidation.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/proc/ArtifactCache.hpp>
#include <genomes/proc/ProceduralRuntime.hpp>
#include <genomes/terrain/HeightField.hpp>
#include <genomes/terrain/TerrainMesh.hpp>
#include <genomes/world/WorldGenerationTask.hpp>
#include <genomes/world/WorldArtifactRevision.hpp>
#include <genomes/world/WorldSave.hpp>
#include <genomes/world/WorldStreamer.hpp>

#include <memory>
#include <optional>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace genomes::gameplay {

struct WorldSemanticSnapshot final {
    std::uint64_t content_hash{0U};
    std::uint32_t generator_version{0U};
    std::uint32_t map_size_m{0U};
    std::size_t feature_count{0U};
    std::size_t road_count{0U};
    std::size_t building_count{0U};
    std::size_t vegetation_count{0U};
    std::size_t building_site_count{0U};
    std::size_t parcel_count{0U};
    std::size_t river_count{0U};
    std::size_t terrain_sample_count{0U};

    friend bool operator==(const WorldSemanticSnapshot&, const WorldSemanticSnapshot&) = default;
    [[nodiscard]] bool valid() const noexcept { return content_hash != 0U && feature_count > 0U; }
};

struct LandscapeSample final {
    float ground_y{0.0F};
    hydrology::WaterSample water{};

    [[nodiscard]] bool traversable(float maximum_water_depth_m = 0.35F) const noexcept {
        return !water.has_water || water.depth_m <= maximum_water_depth_m;
    }
};

// The committed world artifact is the smallest backend-neutral handoff from
// procedural generation to a battlefield or a diagnostic viewer.  Semantic
// data remains authoritative in WorldPlan; terrain is generated from the same
// request/seed and is kept here so consumers cannot accidentally render one
// world while querying another.
struct ResolvedWorldArtifacts final {
    world::WorldArtifactRevision revision{0U};
    world::WorldPlan plan{};
    std::shared_ptr<const terrain::HeightField> terrain;
    std::shared_ptr<const terrain::TerrainMesh> terrain_mesh;
    // Renderer-independent water surface generated from the same final
    // heightfield and hydrology revision as terrain_mesh.
    std::shared_ptr<const terrain::TerrainMesh> water_mesh;
    std::shared_ptr<const std::vector<buildings::BuildingGenerationResult>> resolved_buildings;
    // Destruction invalidations are bound to this exact revision before any
    // consumer can publish derived collision/navigation/render state.
    std::shared_ptr<destruction::DestructionInvalidationQueue> destruction_invalidations;
    // Canonical empty-entity save package. Keeping this beside the generated
    // plan makes persistence and streaming consume the same content hash.
    std::shared_ptr<const std::vector<std::byte>> save_package;

    [[nodiscard]] bool valid() const noexcept {
        return revision == world::artifactRevision(plan) && plan.content_hash != 0U &&
               !plan.features.empty() && terrain != nullptr &&
               terrain_mesh != nullptr && resolved_buildings != nullptr &&
               water_mesh != nullptr &&
               destruction_invalidations != nullptr &&
               destruction_invalidations->worldRevision() == revision &&
               save_package != nullptr && terrain->width() >= 2U &&
               terrain->height() >= 2U && !terrain_mesh->vertices.empty() &&
               !terrain_mesh->indices.empty() &&
               resolved_buildings->size() == plan.building_sites.size() && !save_package->empty();
    }

    [[nodiscard]] LandscapeSample sampleLandscape(float x, float z) const noexcept {
        LandscapeSample result{};
        if (terrain != nullptr) result.ground_y = terrain->sampleBilinear(x, z);
        result.water = plan.hydrology.sampleWater(x, z);
        return result;
    }
};

// Compatibility name for existing scene/application callers. New code should
// use the neutral resolved-artifact name.
using WorldScenarioArtifact = ResolvedWorldArtifacts;

struct WorldScenarioStatus final {
    bool has_active_world{false};
    bool generation_pending{false};
    foundation::StableId active_content_hash{0};
    std::size_t streaming_pending{0U};
    std::size_t streaming_resident{0U};
    foundation::Error last_error{};
};

class WorldScenario final {
public:
    explicit WorldScenario(jobs::JobSystem& jobs,
                           std::shared_ptr<const buildings::FrozenBuildingProfile> building_profile,
                           std::shared_ptr<proc::ArtifactCache> cache = {},
                           proc::GeneratorRegistry registry = {},
                           proc::ProceduralRuntime* shared_runtime = nullptr) noexcept
        : jobs_(jobs), generation_service_(jobs, std::move(cache), std::move(registry),
                                            shared_runtime),
          building_profile_(std::move(building_profile)) {}

    [[nodiscard]] foundation::Result<void, foundation::Error> requestNew(
        const world::WorldGenerationRequest& request);
    [[nodiscard]] foundation::Result<bool, foundation::Error> poll();
    [[nodiscard]] foundation::Result<void, foundation::Error> startNew(
        const world::WorldGenerationRequest& request);
    [[nodiscard]] foundation::Result<void, foundation::Error> requestRegion(
        world::RegionCoord coordinate,
        world::ResidencyMask desired =
            residency(world::ResidencyAxis::Semantic) |
            residency(world::ResidencyAxis::Render));
    [[nodiscard]] std::vector<world::StreamedRegion> takeStreamedRegions();
    void cancelPending() noexcept;

    [[nodiscard]] const world::WorldPlan* activePlan() const noexcept;
    [[nodiscard]] const WorldScenarioArtifact* activeArtifact() const noexcept;
    [[nodiscard]] std::shared_ptr<const WorldScenarioArtifact> activeArtifactHandle() const noexcept {
        return active_artifact_;
    }
    [[nodiscard]] const world::WorldGenerationRequest* activeRequest() const noexcept;
    [[nodiscard]] WorldSemanticSnapshot semanticSnapshot() const noexcept;
    [[nodiscard]] const WorldScenarioStatus& status() const noexcept { return status_; }
    [[nodiscard]] proc::ProceduralRuntimeTelemetry proceduralTelemetry() const noexcept {
        return generation_service_.runtime()->telemetry();
    }

    // Compile a generated plan into the same immutable, revision-bound handoff
    // used by the asynchronous scenario.  Deterministic/headless callers use
    // this entry point instead of maintaining a second terrain/building path.
    [[nodiscard]] static foundation::Result<ResolvedWorldArtifacts, foundation::Error>
    compileArtifact(world::WorldPlan plan,
                    const world::WorldGenerationRequest& request,
                    const buildings::FrozenBuildingProfile& building_profile);

private:
    [[nodiscard]] static bool validCandidate(const world::WorldPlan& plan) noexcept;
    [[nodiscard]] static foundation::Result<ResolvedWorldArtifacts, foundation::Error>
    compileArtifactImpl(world::WorldPlan plan,
                        const world::WorldGenerationRequest& request,
                        const buildings::FrozenBuildingProfile& building_profile,
                        proc::ProceduralRuntime* procedural_runtime);

    jobs::JobSystem& jobs_;
    world::WorldGenerationService generation_service_;
    proc::GenerationChannel generation_channel_;
    std::shared_ptr<const buildings::FrozenBuildingProfile> building_profile_;
    std::unique_ptr<world::WorldStreamer> streamer_;
    std::vector<world::StreamedRegion> streamed_regions_;
    foundation::SimulationTick streaming_tick_{};
    std::optional<world::WorldGenerationTask> pending_;
    std::optional<world::WorldGenerationRequest> pending_request_;
    proc::GenerationTicket<WorldScenarioArtifact> pending_artifact_ticket_;
    std::optional<world::WorldGenerationRequest> pending_artifact_request_;
    std::optional<world::WorldGenerationRequest> active_request_;
    std::shared_ptr<const ResolvedWorldArtifacts> active_artifact_;
    WorldScenarioStatus status_{};
};

} // namespace genomes::gameplay
