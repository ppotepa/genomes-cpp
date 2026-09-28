#include <genomes/ballistics/Detonation.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>

int main() {
    using namespace genomes::ballistics;

    const StrategyId strategy_value = strategy_id("benchmark.he.strategy");
    const CaliberId caliber_value = caliber_id("benchmark.he.caliber");
    const VariantId variant_value = variant_id("benchmark.he.variant");
    AmmunitionStrategy strategy{};
    strategy.id = strategy_value;
    strategy.caliber_id = caliber_value;
    strategy.variant_id = variant_value;
    strategy.construction = ConstructionKind::HighExplosive;
    strategy.explosive = true;
    strategy.fuze = FuzeMode::ArmedContact;
    strategy.fragmentation = {40U, 0.75F, 0.65F, 0.25F, 1.0F, 0.3F};

    AmmunitionDefinition ammunition{};
    ammunition.id = ammunition_id("benchmark.he");
    ammunition.strategy_id_value = strategy_value;
    ammunition.caliber_id_value = caliber_value;
    ammunition.variant_id_value = variant_value;
    ammunition.mass_kg = 0.02F;
    ammunition.diameter_m = 0.02F;
    ammunition.drag_diameter_m = 0.02F;
    ammunition.muzzle_velocity_mps = 250.0F;
    ammunition.explosive_energy_j = 250.0F;

    AmmunitionCatalog catalog;
    if (!catalog.add(ammunition, strategy) || !catalog.freeze()) {
        return 1;
    }
    FireRequest request{};
    request.projectile_id = ProjectileId{1U};
    request.shot_id = ShotId{2U};
    request.trace_id = TraceId{3U};
    request.ammunition_id = ammunition.id;
    request.direction = {1.0F, 0.0F, 0.0F};
    request.seed = 7U;
    const auto projectile = ProjectileState::create(request, catalog);
    if (!projectile) {
        return 1;
    }

    constexpr std::uint32_t count = 10'000U;
    std::uint64_t fragments = 0;
    double represented_energy = 0.0;
    const auto begin = std::chrono::steady_clock::now();
    for (std::uint32_t index = 0; index < count; ++index) {
        const DetonationInput input{static_cast<std::uint64_t>(index + 100U),
                                    {static_cast<float>(index), 0.0F, 0.0F},
                                    {0.0F, 1.0F, 0.0F},
                                    true,
                                    2048U};
        const auto event = Detonation::detonate(projectile.value(), ammunition, strategy, input);
        if (!event) {
            return 1;
        }
        fragments += event.value().fragments.size();
        represented_energy += event.value().ledger.represented_fragment_energy;
    }
    const auto end = std::chrono::steady_clock::now();
    const auto elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
    std::cout << "detonation count=" << count << " fragments=" << fragments
              << " elapsed_us=" << elapsed_us
              << " represented_energy=" << represented_energy << '\n';
    return 0;
}
