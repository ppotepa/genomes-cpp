#include <genomes/combat/SquadState.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

void run(std::size_t count) {
    using namespace genomes;
    combat::SignalPropagationPolicy policy{};
    policy.max_pending_signals = static_cast<std::uint32_t>(count + 1U);
    combat::SquadSystem system(policy);
    std::vector<simulation::EntityId> members;
    members.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        members.push_back({static_cast<std::uint32_t>(index + 1U), 1U});
    }
    (void)system.create(1U, members);
    const simulation::EntityId enemy{0xFFFF'FFFEU, 1U};
    const auto begin = std::chrono::steady_clock::now();
    for (std::size_t index = 0U; index < count; ++index) {
        (void)system.schedule({members[index], 1U, enemy, {10.0F, 1.0F, 10.0F}, {0U}, {3U},
                               60U, 0U, 0.75F, 20.0F, combat::TacticalSignalType::Contact,
                               combat::TacticalSignalChannel::Radio});
    }
    const auto delivered = system.deliver({3U});
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - begin);
    std::cout << "squad_signal count=" << count << " ms=" << elapsed.count()
              << " delivered=" << (delivered ? delivered.value() : 0U) << std::endl;
}

} // namespace

int main() {
    run(1000U);
    run(10000U);
    return 0;
}
