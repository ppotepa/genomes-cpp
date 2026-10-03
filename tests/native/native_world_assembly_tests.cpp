#include <genomes/gameplay/WorldScenario.hpp>
#include <genomes/gameplay/ProductionGenerators.hpp>
#include <genomes/buildings/BuildingProfile.hpp>
#include <genomes/world/GridLayout.hpp>
#include <genomes/world/WorldGenerationProfile.hpp>

#include <cassert>
#include <filesystem>
#include <memory>
#include <thread>
#include <utility>

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

    // Production scenes and region streaming share the same frozen generator
    // registry; the terrain stage must remain parity-equivalent to the legacy
    // synchronous facade.
    jobs::JobSystem production_jobs(2U);
    auto production_registry_result = gameplay::makeProductionGeneratorRegistry();
    assert(production_registry_result);
    gameplay::WorldScenario production_scenario(
        production_jobs, building_profile, {}, std::move(production_registry_result.value()));
    assert(production_scenario.startNew(request));
    const auto* production_artifact = production_scenario.activeArtifact();
    assert(production_artifact != nullptr && production_artifact->valid());
    assert(production_artifact->plan.content_hash == artifact->plan.content_hash);
    assert(production_artifact->terrain->width() == artifact->terrain->width());
    assert(production_artifact->terrain->height() == artifact->terrain->height());
    assert(production_artifact->terrain->at(0U, 0U) == artifact->terrain->at(0U, 0U));
    assert(production_artifact->terrain->at(artifact->terrain->width() - 1U,
                                             artifact->terrain->height() - 1U) ==
           artifact->terrain->at(artifact->terrain->width() - 1U,
                                 artifact->terrain->height() - 1U));

    const auto deterministic_artifact = gameplay::WorldScenario::compileArtifact(
        artifact->plan, request, *building_profile);
    assert(deterministic_artifact && deterministic_artifact.value().valid());
    assert(deterministic_artifact.value().revision == artifact->revision);
    assert(deterministic_artifact.value().terrain->width() == artifact->terrain->width());
    assert(deterministic_artifact.value().terrain_mesh->indices.size() ==
           artifact->terrain_mesh->indices.size());

    // The resolved artifact owns one revision. The production battlefield
    // binds collision/navigation consumers to that same revision before
    // publishing render products; stale or mixed revisions are rejected.
    assert(artifact->revision != 0U);
    assert(artifact->destruction_invalidations->worldRevision() == artifact->revision);

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

    world::WorldGenerationRequest superseded_request = request;
    superseded_request.seed += 1U;
    world::WorldGenerationRequest latest_request = request;
    latest_request.seed += 2U;
    assert(scenario.requestNew(superseded_request));
    assert(scenario.requestNew(latest_request));
    for (std::size_t attempt = 0U; attempt < 20'000U &&
         scenario.status().generation_pending; ++attempt) {
        const auto polled = scenario.poll();
        assert(polled);
        std::this_thread::yield();
    }
    assert(!scenario.status().generation_pending);
    assert(scenario.activeRequest() != nullptr);
    assert(scenario.activeRequest()->seed == latest_request.seed);
    return 0;
}
