#include <genomes/gameplay/WorldScenario.hpp>
#include <genomes/buildings/BuildingProfile.hpp>
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
    jobs::JobSystem headless_jobs(2U);
    jobs::JobSystem graphical_jobs(2U);
    gameplay::WorldScenario headless(headless_jobs, building_profile);
    gameplay::WorldScenario graphical(graphical_jobs, building_profile);
    const auto profile = world::loadWorldGenerationProfile(
        std::filesystem::path{GENOMES_SOURCE_DIR} /
        "mods/core/profiles/world-generation.json");
    assert(profile);
    world::WorldGenerationRequest request = profile.value().makeDefaultRequest();
    auto zero_seed = request;
    zero_seed.seed = 0U;
    assert(!zero_seed.valid());
    auto non_grid_size = request;
    non_grid_size.map_size_m = 129U;
    assert(!non_grid_size.valid());
    request.seed = 0xCAFEBABEULL;
    assert(headless.startNew(request));
    assert(graphical.startNew(request));
    const auto headless_snapshot = headless.semanticSnapshot();
    const auto graphical_snapshot = graphical.semanticSnapshot();
    assert(headless_snapshot.valid() && graphical_snapshot.valid());
    assert(headless_snapshot == graphical_snapshot);
    const auto* headless_plan = headless.activePlan();
    const auto* graphical_plan = graphical.activePlan();
    assert(headless_plan != nullptr && graphical_plan != nullptr);
    assert(headless_plan->hasValidStageFingerprints());
    assert(graphical_plan->hasValidStageFingerprints());
    assert(headless_plan->stage_fingerprints == graphical_plan->stage_fingerprints);
    return 0;
}
