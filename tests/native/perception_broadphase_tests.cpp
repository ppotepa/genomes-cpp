#include <genomes/combat/PerceptionBroadphase.hpp>

#include <cassert>

int main() {
    using namespace genomes;
    const simulation::EntityId observer_id{1U, 1U};
    std::vector<combat::PerceptionAgent> agents{
        {observer_id, {0.0F, 0.0F, 0.0F}, 0.0F, 1U, 0xFFFF'FFFFu, true},
        {{2U, 1U}, {0.0F, 0.0F, 10.0F}, 0.0F, 2U, 0xFFFF'FFFFu, true},
        {{3U, 1U}, {0.0F, 0.0F, 20.0F}, 0.0F, 2U, 0xFFFF'FFFFu, true},
        {{7U, 1U}, {0.0F, 0.0F, 8.0F}, 0.0F, 1U, 0xFFFF'FFFFu, true},
        {{4U, 1U}, {0.0F, 0.0F, -10.0F}, 0.0F, 2U, 0xFFFF'FFFFu, true},
        {{5U, 1U}, {80.0F, 0.0F, 0.0F}, 0.0F, 2U, 0xFFFF'FFFFu, true},
        {{6U, 1U}, {-4.0F, 0.0F, 6.0F}, 0.0F, 2U, 0xFFFF'FFFFu, false},
    };
    spatial::UniformGrid grid(8.0F);
    combat::PerceptionBroadphase broadphase;
    assert(broadphase.rebuild(agents, grid));
    combat::PerceptionProfile profile{};
    profile.max_candidates = 1U;
    const auto result = broadphase.query(agents.front(), profile, grid);
    assert(result && result.value().size() == 1U);
    const simulation::EntityId nearest{2U, 1U};
    assert(result.value().front().target == nearest);
    assert(broadphase.diagnostics().grid_candidates >= 4U);
    assert(broadphase.diagnostics().rejected_team >= 1U);
    assert(broadphase.diagnostics().rejected_fov >= 2U);
    assert(broadphase.diagnostics().truncated);

    std::vector<combat::PerceptionAgent> duplicate = agents;
    duplicate.push_back(agents[1]);
    assert(!broadphase.rebuild(duplicate, grid));
    return 0;
}
