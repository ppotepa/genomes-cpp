#include <genomes/destruction/ImpactSolver.hpp>

#include <cassert>
#include <cmath>
#include <vector>

namespace {

using namespace genomes;
using destruction::Layer;
using destruction::LayerKind;
using destruction::MaterialAssembly;
using destruction::MaterialCatalog;
using destruction::MaterialFrame;
using destruction::MaterialId;
using destruction::PhysicalSolidId;

MaterialAssembly makeAssembly(const MaterialCatalog& catalog,
                              std::vector<Layer> layers) {
    const auto result = MaterialAssembly::create(17U, MaterialFrame::identity(),
                                                  std::move(layers), catalog);
    assert(result);
    return std::move(result.value());
}

destruction::ImpactInput inputFor(const MaterialAssembly& assembly) {
    destruction::ImpactInput input{};
    input.mass_kg = 0.01F;
    input.diameter_m = 0.00556F;
    input.projectile_velocity = {0.0F, 120.0F, 0.0F};
    input.projectile_center = {0.0F, -0.1F, 0.0F};
    input.contact_point = {0.2F, 0.0F, 0.0F};
    input.contact_normal = {0.0F, 1.0F, 0.0F};
    input.assembly = &assembly;
    input.entry_point = {0.0F, 0.0F, 0.0F};
    input.exit_point = {0.0F, 1.0F, 0.0F};
    return input;
}

} // namespace

int main() {
    const MaterialCatalog catalog = MaterialCatalog::makeDefault();
    const PhysicalSolidId solid = PhysicalSolidId::fromName("test-solid");

    const MaterialAssembly void_assembly = makeAssembly(
        catalog, {{{}, {}, 0.0F, 1.0F, LayerKind::Void}});
    const auto void_result = destruction::ImpactSolver::solve(
        inputFor(void_assembly), catalog);
    assert(void_result);
    assert(void_result.value().outcome == destruction::ImpactOutcome::Contact);
    assert(std::abs(void_result.value().energy.balance_error) < 1.0e-3F);

    const MaterialAssembly foliage_assembly = makeAssembly(
        catalog, {{MaterialId::fromName("foliage"), solid, 0.0F, 0.20F,
                   LayerKind::Solid},
                  {{}, {}, 0.20F, 0.30F, LayerKind::Void},
                  {MaterialId::fromName("foliage"), solid, 0.30F, 0.80F,
                   LayerKind::Solid}});
    const auto foliage_result = destruction::ImpactSolver::solve(
        inputFor(foliage_assembly), catalog);
    assert(foliage_result);
    assert(foliage_result.value().outcome == destruction::ImpactOutcome::Penetrated);
    assert(foliage_result.value().energy.material_work > 0.0F);
    assert(foliage_result.value().traversed_distance > 0.79F);

    auto moving_input = inputFor(foliage_assembly);
    moving_input.target_linear_velocity = {0.0F, 20.0F, 0.0F};
    const auto moving_result = destruction::ImpactSolver::solve(moving_input, catalog);
    assert(moving_result);
    assert(std::abs(moving_result.value().relative_incoming_velocity.y - 100.0F) < 1.0e-4F);

    auto reverse_input = inputFor(foliage_assembly);
    reverse_input.projectile_velocity = {0.0F, -120.0F, 0.0F};
    reverse_input.entry_point = {0.0F, 1.0F, 0.0F};
    reverse_input.exit_point = {0.0F, 0.0F, 0.0F};
    const auto reverse_result = destruction::ImpactSolver::solve(reverse_input, catalog);
    assert(reverse_result);
    assert(std::abs(reverse_result.value().energy.material_work -
                    foliage_result.value().energy.material_work) < 1.0e-3F);

    auto budget_input = inputFor(foliage_assembly);
    budget_input.limits.max_intervals = 1;
    const auto budget_result = destruction::ImpactSolver::solve(budget_input, catalog);
    assert(budget_result);
    assert(budget_result.value().outcome == destruction::ImpactOutcome::BudgetExceeded);
    assert(budget_result.value().budget_exhausted);
    return 0;
}
