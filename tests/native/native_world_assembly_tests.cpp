#include <genomes/gameplay/WorldScenario.hpp>

#include <cassert>

int main() {
    using namespace genomes;
    jobs::JobSystem jobs(2U);
    gameplay::WorldScenario scenario(jobs);
    world::WorldGenerationRequest request{};
    request.seed = 0x12345678ULL;
    request.map_size_m = 600U;
    assert(scenario.startNew(request));
    assert(scenario.activePlan() != nullptr);
    const auto first_hash = scenario.status().active_content_hash;
    assert(first_hash != 0U && scenario.activeRequest() != nullptr);
    assert(scenario.activeRequest()->seed == request.seed);
    const auto* artifact = scenario.activeArtifact();
    assert(artifact != nullptr && artifact->valid());
    assert(artifact->revision == world::artifactRevision(artifact->plan));
    assert(scenario.activePlan() == &artifact->plan);
    assert(artifact->resolved_buildings.size() == artifact->plan.building_sites.size());

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
