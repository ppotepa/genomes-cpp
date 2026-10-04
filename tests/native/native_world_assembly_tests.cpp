#include <genomes/gameplay/WorldScenario.hpp>
#include <genomes/gameplay/BattlefieldRuntime.hpp>
#include <genomes/gameplay/InfantryMassBattleRuntime.hpp>
#include <genomes/gameplay/ProductionGenerators.hpp>
#include <genomes/buildings/BuildingProfile.hpp>
#include <genomes/world/GridLayout.hpp>
#include <genomes/world/WorldGenerationProfile.hpp>

#include <cassert>
#include <cmath>
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

    // Terrain resolution is a first-class world setting. Hydrology must use
    // the same cells as the height field rather than silently falling back to
    // the legacy 8 m navigation grid.
    world::WorldGenerationRequest high_resolution_request = request;
    high_resolution_request.terrain.sample_spacing_m = 4U;
    assert(high_resolution_request.valid());
    const auto high_resolution_plan = world::WorldGenerator::generate(high_resolution_request);
    assert(high_resolution_plan);
    const auto high_resolution_artifact = gameplay::WorldScenario::compileArtifact(
        high_resolution_plan.value(), high_resolution_request, *building_profile);
    assert(high_resolution_artifact && high_resolution_artifact.value().valid());
    const std::uint32_t expected_terrain_cells =
        high_resolution_request.map_size_m / high_resolution_request.terrain.sample_spacing_m;
    assert(high_resolution_artifact.value().terrain->cellSize() == 4.0F);
    assert(high_resolution_artifact.value().terrain->width() == expected_terrain_cells + 1U);
    assert(high_resolution_artifact.value().terrain->height() == expected_terrain_cells + 1U);
    assert(high_resolution_artifact.value().plan.hydrology.cells_x == expected_terrain_cells);
    assert(high_resolution_artifact.value().plan.hydrology.cells_z == expected_terrain_cells);
    assert(high_resolution_artifact.value().plan.hydrology.cell_size_m == 4.0F);

    world::WorldGenerationRequest river_request = request;
    river_request.hydrology_mode = hydrology::HydrologyMode::Forced;
    river_request.hydrology.main_river_min = 1U;
    river_request.hydrology.main_river_max = 1U;
    river_request.hydrology.depth_min_m = 1.0F;
    river_request.hydrology.depth_max_m = 1.0F;
    const auto river_plan = world::WorldGenerator::generate(river_request);
    assert(river_plan);
    const auto river_artifact = gameplay::WorldScenario::compileArtifact(
        river_plan.value(), river_request, *building_profile);
    assert(river_artifact && river_artifact.value().valid());
    assert(!river_artifact.value().plan.hydrology.rivers.empty());
    const auto& river = river_artifact.value().plan.hydrology.rivers.front();
    const auto& river_points = river_artifact.value().plan.hydrology.river_points;
    for (std::size_t index = river.point_offset + 1U;
         index < river.point_offset + river.point_count; ++index)
        assert(river_points[index].y < river_points[index - 1U].y);
    const auto channel_center = river_points[river.point_offset + river.point_count / 2U];
    const auto landscape = river_artifact.value().sampleLandscape(channel_center.x,
                                                                   channel_center.z);
    assert(landscape.water.has_water);
    assert(!landscape.traversable());
    assert(landscape.ground_y < landscape.water.surface_y);
    for (const auto& site : river_artifact.value().plan.building_sites) {
        assert(!river_artifact.value().sampleLandscape(site.preferred_position.x,
                                                        site.preferred_position.z)
                    .water.has_water);
        assert(!river_artifact.value().plan.hydrology.isFloodplain(
            site.preferred_position.x, site.preferred_position.z));
    }

#if GENOMES_HAS_INFANTRY
    auto battlefield_runtime = gameplay::BattlefieldRuntime::start(
        {.seed = river_request.seed, .map_size_m = 25U}, &jobs);
    assert(battlefield_runtime);
    const auto shared_river_artifact =
        std::make_shared<const gameplay::ResolvedWorldArtifacts>(river_artifact.value());
    assert(battlefield_runtime.value()->bindWorldArtifact(shared_river_artifact));
    assert(battlefield_runtime.value()->worldArtifactRevision() ==
           shared_river_artifact->revision);
    for (const auto& state : battlefield_runtime.value()->presentationSnapshot().states) {
        const float expected_height = shared_river_artifact->terrain->sampleBilinear(
            state.position.x, state.position.z);
        assert(std::abs(state.position.y - expected_height) < 0.001F);
    }
    const auto mass_runtime = gameplay::InfantryMassBattleRuntime::start(
        {.seed = river_request.seed, .map_size_m = river_request.map_size_m,
         .units_per_team = 8U}, &jobs);
    assert(mass_runtime);
    assert(mass_runtime.value()->bindWorldArtifact(shared_river_artifact));
    assert(mass_runtime.value()->worldArtifactRevision() == shared_river_artifact->revision);
    for (const auto& state : mass_runtime.value()->presentationSnapshot().states) {
        const float expected_height = shared_river_artifact->terrain->sampleBilinear(
            state.position.x, state.position.z);
        assert(std::abs(state.position.y - expected_height) < 0.001F);
    }
#endif

    jobs::JobSystem one_worker_jobs(1U);
    jobs::JobSystem four_worker_jobs(4U);
    gameplay::WorldScenario one_worker_scenario(one_worker_jobs, building_profile);
    gameplay::WorldScenario four_worker_scenario(four_worker_jobs, building_profile);
    assert(one_worker_scenario.startNew(river_request));
    assert(four_worker_scenario.startNew(river_request));
    const auto* one_worker_artifact = one_worker_scenario.activeArtifact();
    const auto* four_worker_artifact = four_worker_scenario.activeArtifact();
    assert(one_worker_artifact != nullptr && four_worker_artifact != nullptr);
    assert(one_worker_artifact->revision == four_worker_artifact->revision);
    assert(one_worker_artifact->plan.hydrology.content_hash ==
           four_worker_artifact->plan.hydrology.content_hash);
    assert(one_worker_artifact->terrain->samples().size() ==
           four_worker_artifact->terrain->samples().size());
    for (std::size_t index = 0U; index < one_worker_artifact->terrain->samples().size(); ++index)
        assert(one_worker_artifact->terrain->samples()[index] ==
               four_worker_artifact->terrain->samples()[index]);

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
