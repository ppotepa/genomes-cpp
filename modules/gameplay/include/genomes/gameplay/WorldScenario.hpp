#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/buildings/BuildingModel.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/proc/ArtifactCache.hpp>
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

// The committed world artifact is the smallest backend-neutral handoff from
// procedural generation to a battlefield or a diagnostic viewer.  Semantic
// data remains authoritative in WorldPlan; terrain is generated from the same
// request/seed and is kept here so consumers cannot accidentally render one
// world while querying another.
struct WorldScenarioArtifact final {
    world::WorldArtifactRevision revision{0U};
    world::WorldPlan plan{};
    std::optional<terrain::HeightField> terrain;
    std::optional<terrain::TerrainMesh> terrain_mesh;
    std::vector<buildings::BuildingGenerationResult> resolved_buildings;
    // Canonical empty-entity save package. Keeping this beside the generated
    // plan makes persistence and streaming consume the same content hash.
    std::vector<std::byte> save_package;

    [[nodiscard]] bool valid() const noexcept {
        return revision == world::artifactRevision(plan) && plan.content_hash != 0U &&
               !plan.features.empty() && terrain.has_value() &&
               terrain_mesh.has_value() && terrain->width() >= 2U &&
               terrain->height() >= 2U && !terrain_mesh->vertices.empty() &&
               !terrain_mesh->indices.empty() &&
               resolved_buildings.size() == plan.building_sites.size() && !save_package.empty();
    }
};

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
                           std::shared_ptr<proc::ArtifactCache> cache = {}) noexcept
        : jobs_(jobs), generation_service_(jobs, std::move(cache)) {}

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
    [[nodiscard]] const world::WorldGenerationRequest* activeRequest() const noexcept;
    [[nodiscard]] WorldSemanticSnapshot semanticSnapshot() const noexcept;
    [[nodiscard]] const WorldScenarioStatus& status() const noexcept { return status_; }

private:
    [[nodiscard]] static bool validCandidate(const world::WorldPlan& plan) noexcept;
    [[nodiscard]] static foundation::Result<WorldScenarioArtifact, foundation::Error>
    compileArtifact(world::WorldPlan plan,
                    const world::WorldGenerationRequest& request);

    jobs::JobSystem& jobs_;
    world::WorldGenerationService generation_service_;
    std::unique_ptr<world::WorldStreamer> streamer_;
    std::vector<world::StreamedRegion> streamed_regions_;
    foundation::SimulationTick streaming_tick_{};
    std::optional<world::WorldGenerationTask> pending_;
    std::optional<world::WorldGenerationRequest> pending_request_;
    std::optional<world::WorldGenerationRequest> active_request_;
    std::shared_ptr<const WorldScenarioArtifact> active_artifact_;
    WorldScenarioStatus status_{};
};

} // namespace genomes::gameplay
