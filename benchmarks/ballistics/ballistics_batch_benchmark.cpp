#include <genomes/ballistics/BallisticsBatch.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    using namespace genomes::ballistics;

    const StrategyId strategy_value = strategy_id("batch.benchmark.strategy");
    const CaliberId caliber_value = caliber_id("batch.benchmark.caliber");
    const VariantId variant_value = variant_id("batch.benchmark.variant");
    AmmunitionStrategy strategy{};
    strategy.id = strategy_value;
    strategy.caliber_id = caliber_value;
    strategy.variant_id = variant_value;
    strategy.drag_coefficient = 0.15F;
    AmmunitionCatalog catalog;
    if (!catalog.add({ammunition_id("batch.benchmark.ammo"),
                      strategy_value,
                      caliber_value,
                      variant_value,
                      0.01F,
                      0.01F,
                      0.01F,
                      120.0F,
                      0.25F,
                      1U,
                      "batch-benchmark"},
                     strategy) ||
        !catalog.freeze()) {
        return 1;
    }

    constexpr std::uint32_t count = 10'000U;
    std::vector<ProjectileState> projectiles;
    projectiles.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        FireRequest request{};
        request.projectile_id = ProjectileId{index + 1U};
        request.shot_id = ShotId{index + 10001U};
        request.trace_id = TraceId{index + 20001U};
        request.ammunition_id = ammunition_id("batch.benchmark.ammo");
        request.position = {static_cast<float>(index % 100U), 0.0F,
                            static_cast<float>(index / 100U)};
        request.direction = {1.0F, 0.0F, 0.01F};
        request.seed = index + 1U;
        const auto state = ProjectileState::create(request, catalog);
        if (!state) {
            return 1;
        }
        projectiles.push_back(state.value());
    }
    const FlightEnvironment environment{{1.0F, 0.0F, 0.0F}, {}, 0.0F, 343.0F, 1.0F};
    const auto scalar_begin = std::chrono::steady_clock::now();
    const auto scalar = BallisticsBatch::integrate(projectiles, catalog, environment, 1.0F / 60.0F);
    const auto scalar_end = std::chrono::steady_clock::now();
    genomes::jobs::JobSystem jobs{4U};
    const auto parallel_begin = std::chrono::steady_clock::now();
    const auto parallel =
        BallisticsBatch::integrate(projectiles, catalog, environment, 1.0F / 60.0F, &jobs, 128U);
    const auto parallel_end = std::chrono::steady_clock::now();
    if (!scalar || !parallel) {
        return 1;
    }
    const auto scalar_us = std::chrono::duration_cast<std::chrono::microseconds>(
        scalar_end - scalar_begin).count();
    const auto parallel_us = std::chrono::duration_cast<std::chrono::microseconds>(
        parallel_end - parallel_begin).count();
    std::cout << "ballistics_batch count=" << count << " scalar_us=" << scalar_us
              << " parallel_us=" << parallel_us << " workers=" << jobs.workerCount() << '\n';
    return 0;
}
