#include <genomes/combat/PerceptionBroadphase.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

void run(std::size_t count) {
    using namespace genomes;
    std::vector<combat::PerceptionAgent> agents;
    agents.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        const float x = static_cast<float>(static_cast<int>(index % 500U) - 250) * 2.0F;
        const float z = static_cast<float>(static_cast<int>(index / 500U) -
                                          static_cast<int>(count / 1000U)) * 2.0F;
        agents.push_back({{static_cast<std::uint32_t>(index + 1U), 1U},
                          {x, 0.0F, z}, 0.0F, static_cast<std::uint32_t>(index % 2U),
                          0xFFFF'FFFFu, true});
    }
    spatial::UniformGrid grid(16.0F);
    combat::PerceptionBroadphase broadphase;
    (void)broadphase.rebuild(agents, grid);
    combat::PerceptionProfile profile{};
    profile.max_candidates = 32U;
    const auto begin = std::chrono::steady_clock::now();
    std::size_t accepted = 0U;
    const std::size_t stride = count > 10000U ? 32U : 1U;
    std::size_t query_count = 0U;
    for (std::size_t index = 0U; index < agents.size(); index += stride) {
        const auto& agent = agents[index];
        const auto result = broadphase.query(agent, profile, grid);
        if (result) {
            accepted += result.value().size();
        }
        ++query_count;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - begin);
    std::cout << "perception_broadphase count=" << count << " ms=" << elapsed.count()
              << " accepted=" << accepted << " queries=" << query_count << std::endl;
}

} // namespace

int main() {
    run(1000U);
    run(10000U);
    run(50000U);
    return 0;
}
