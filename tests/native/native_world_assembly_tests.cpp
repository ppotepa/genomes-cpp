#include <genomes/gameplay/WorldScenario.hpp>
#include <genomes/buildings/BuildingProfile.hpp>
#include <genomes/world/GridLayout.hpp>
#include <genomes/world/WorldGenerationProfile.hpp>

#include <cassert>
#include <filesystem>
#include <memory>

int main() {
    using namespace genomes;
    const auto loaded_building_profile = buildings::loadBuildingProfile(
        std::filesystem::path{GENOMES_SOURCE_DIR} / "mods/core/profiles/building.json");
    assert(loaded_building_profile);
    const auto building_profile = std::make_shared<const buildings::FrozenBuildingProfile>(
        std::move(loaded_building_profile.value()));
    jobs::JobSystem jobs(2U);
    gameplay::WorldScenario scenario(jobs, building_profile);
    const auto profile = world::loadWorldGenerationProfile(
        std::filesystem::path{GENOMES_SOURCE_DIR} /
        "mods/core/profiles/world-generation.json");
    assert(profile);
    const world::WorldGenerationRequest request =
        profile.value().makeRequest(0x12345678ULL);
    assert(scenario.startNew(request));
    assert(scenario.activePlan() != nullptr);
    const auto first_hash = scenario.status().active_content_hash;
    assert(first_hash != 0U && scenario.activeRequest() != nullptr);
    assert(scenario.activeRequest()->seed == request.seed);
    const auto* artifact = scenario.activeArtifact();
    const auto artifact_handle = scenario.activeArtifactHandle();
    assert(artifact != nullptr && artifact->valid());
    const auto layout = world::GridLayout::forMap(request.map_size_m);
    assert(layout.valid());
    assert(artifact->terrain->width() == layout.sample_count);
    assert(artifact->terrain->height() == layout.sample_count);
    assert(artifact_handle.get() == artifact);
    assert(artifact->revision == world::artifactRevision(artifact->plan));
    assert(artifact->destruction_invalidations != nullptr);
    assert(artifact->destruction_invalidations->worldRevision() == artifact->revision);
    const auto stale_revision = artifact->revision == 1U ? 2U : artifact->revision - 1U;
    assert(!artifact->destruction_invalidations->emit(
        world::WorldId{1U}, foundation::stable_id("stale"), stale_revision,
        {{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}},
        static_cast<world::DirtyReasonMask>(world::DirtyReason::Cover)));
    assert(scenario.activePlan() == &artifact->plan);
    assert(artifact->resolved_buildings->size() == artifact->plan.building_sites.size());

    const auto deterministic_artifact = gameplay::WorldScenario::compileArtifact(
        artifact->plan, request, *building_profile);
    assert(deterministic_artifact && deterministic_artifact.value().valid());
    assert(deterministic_artifact.value().revision == artifact->revision);
    assert(deterministic_artifact.value().terrain->width() == artifact->terrain->width());
    assert(deterministic_artifact.value().terrain_mesh->indices.size() ==
           artifact->terrain_mesh->indices.size());

    // Battlefield navigation must use the same cell dimensions and origin as
    // the terrain artifact, rather than a second hardcoded /8 calculation.
    assert(layout.cell_count == request.map_size_m / 8U);
    assert(layout.origin.x == -static_cast<float>(request.map_size_m) * 0.5F);
    assert(layout.origin.z == -static_cast<float>(request.map_size_m) * 0.5F);

    world::WorldGenerationRequest invalid = request;
    invalid.map_size_m = 64U;
    assert(!scenario.startNew(invalid));
    assert(scenario.status().active_content_hash == first_hash);
    assert(scenario.activePlan() != nullptr);

    assert(scenario.requestNew(request));
    assert(scenario.status().generation_pending);
    scenario.cancelPending();
    assert(!scenario.status().generation_pending);
    assert(scenario.status().active_content_hash == first_hash);
    return 0;
}
