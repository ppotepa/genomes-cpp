#include <genomes/combat/TacticalAI.hpp>
#include <genomes/weapons/WeaponCatalog.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

void run(std::size_t count) {
    using namespace genomes;
    const auto* rifle = weapons::WeaponCatalog::find("rifle");
    if (rifle == nullptr) {
        return;
    }

    const simulation::EntityId target{0xFFFFU, 1U};
    std::vector<combat::VisibleTarget> visible{{target, {0.0F, 1.0F, 20.0F}, 400.0F, true}};
    std::vector<combat::AIState> states(count);
    std::vector<combat::TacticalAIEntity> entities;
    entities.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        entities.push_back({{static_cast<std::uint32_t>(index + 1U), 1U},
                            {static_cast<float>(index % 128U), 1.0F,
                             static_cast<float>(index / 128U)},
                            0.0F, rifle->id, visible, &states[index]});
    }

    combat::TacticalAISystem ai;
    (void)ai.registerDefaults();
    const auto begin = std::chrono::steady_clock::now();
    std::size_t intents = 0U;
    for (std::uint64_t tick = 0U; tick < 120U; ++tick) {
        const auto result = ai.evaluate(entities, {tick});
        if (result) {
            intents += result.value().size();
        }
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - begin);
    std::cout << "tactical_ai count=" << count << " ms=" << elapsed.count()
              << " intents=" << intents << " evaluations=" << ai.evaluatedCount() << std::endl;
}

} // namespace

int main() {
    run(1000U);
    run(10000U);
    return 0;
}
