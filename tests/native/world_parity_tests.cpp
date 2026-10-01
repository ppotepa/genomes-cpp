#include <genomes/gameplay/WorldScenario.hpp>

#include <cassert>

int main() {
    using namespace genomes;
    jobs::JobSystem headless_jobs(2U);
    jobs::JobSystem graphical_jobs(2U);
    gameplay::WorldScenario headless(headless_jobs);
    gameplay::WorldScenario graphical(graphical_jobs);
    world::WorldGenerationRequest request{};
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
