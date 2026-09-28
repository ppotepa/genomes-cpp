#include <genomes/gameplay/WorldScenario.hpp>

#include <cassert>

int main() {
    using namespace genomes;
    jobs::JobSystem headless_jobs(2U);
    jobs::JobSystem graphical_jobs(2U);
    gameplay::WorldScenario headless(headless_jobs);
    gameplay::WorldScenario graphical(graphical_jobs);
    world::WorldGenerationRequest request{};
    request.seed = 0xCAFEBABEULL;
    assert(headless.startNew(request));
    assert(graphical.startNew(request));
    const auto headless_snapshot = headless.semanticSnapshot();
    const auto graphical_snapshot = graphical.semanticSnapshot();
    assert(headless_snapshot.valid() && graphical_snapshot.valid());
    assert(headless_snapshot == graphical_snapshot);
    return 0;
}
