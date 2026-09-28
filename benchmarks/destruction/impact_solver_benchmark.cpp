#include <genomes/destruction/ImpactSolver.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

int main() {
    using namespace genomes::destruction;
    const MaterialCatalog catalog = MaterialCatalog::makeDefault();
    const auto assembly_result = MaterialAssembly::create(
        91U, MaterialFrame::identity(),
        std::vector<Layer>{{MaterialId::fromName("brick"),
                            PhysicalSolidId::fromName("benchmark-wall"), 0.0F, 0.20F,
                            LayerKind::Solid},
                           {{}, {}, 0.20F, 0.25F, LayerKind::Void},
                           {MaterialId::fromName("concrete"),
                            PhysicalSolidId::fromName("benchmark-wall"), 0.25F, 1.0F,
                            LayerKind::Solid}},
        catalog);
    if (!assembly_result) {
        return 1;
    }
    const MaterialAssembly assembly = std::move(assembly_result.value());
    ImpactInput input{};
    input.mass_kg = 0.01F;
    input.diameter_m = 0.00556F;
    input.projectile_velocity = {0.0F, 450.0F, 0.0F};
    input.assembly = &assembly;
    input.entry_point = {0.0F, 0.0F, 0.0F};
    input.exit_point = {0.0F, 1.0F, 0.0F};

    constexpr std::uint32_t count = 100'000;
    std::uint32_t solved = 0;
    const auto begin = std::chrono::steady_clock::now();
    for (std::uint32_t index = 0; index < count; ++index) {
        input.contact_point.x = static_cast<float>(index & 31U) * 0.01F;
        const auto result = ImpactSolver::solve(input, catalog);
        solved += result ? 1U : 0U;
    }
    const auto end = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - begin);
    std::cout << "impact_solver count=" << solved << " elapsed_us=" << elapsed.count()
              << "\n";
    return solved == count ? 0 : 1;
}
