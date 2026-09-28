#include <genomes/ballistics/BallisticsBatch.hpp>
#include <genomes/ballistics/Detonation.hpp>

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Validation final {
    std::uint32_t total{0};
    std::uint32_t passed{0};

    void check(bool condition, std::string_view id) {
        ++total;
        if (condition) {
            ++passed;
            std::cout << "PASS " << id << '\n';
        } else {
            std::cout << "FAIL " << id << '\n';
        }
    }
};

[[nodiscard]] std::string readFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream contents;
    contents << stream.rdbuf();
    return contents.str();
}

genomes::ballistics::AmmunitionCatalog makeCatalog(
    genomes::ballistics::AmmunitionStrategy strategy,
    float explosive_energy = 0.0F) {
    using namespace genomes::ballistics;
    AmmunitionCatalog catalog;
    AmmunitionDefinition ammunition{};
    ammunition.id = ammunition_id("validation.ballistics.ammo");
    ammunition.strategy_id_value = strategy.id;
    ammunition.caliber_id_value = strategy.caliber_id;
    ammunition.variant_id_value = strategy.variant_id;
    ammunition.mass_kg = 0.02F;
    ammunition.diameter_m = 0.01F;
    ammunition.drag_diameter_m = 0.01F;
    ammunition.muzzle_velocity_mps = 200.0F;
    ammunition.explosive_energy_j = explosive_energy;
    assert(catalog.add(ammunition, strategy));
    assert(catalog.freeze());
    return catalog;
}

genomes::ballistics::ProjectileState makeProjectile(
    const genomes::ballistics::AmmunitionCatalog& catalog,
    std::uint32_t id,
    bool fragment = false) {
    using namespace genomes::ballistics;
    FireRequest request{};
    request.projectile_id = ProjectileId{id};
    request.shot_id = ShotId{id + 1000U};
    request.trace_id = TraceId{id + 2000U};
    request.ammunition_id = ammunition_id("validation.ballistics.ammo");
    request.position = {0.0F, 0.0F, 0.0F};
    request.direction = {1.0F, 0.0F, 0.0F};
    request.seed = id * 37U;
    request.fragment = fragment;
    const auto result = ProjectileState::create(request, catalog);
    assert(result);
    return result.value();
}

} // namespace

int main() {
    using namespace genomes::ballistics;

    Validation validation;
    const std::filesystem::path fixture_root =
        std::filesystem::path{GENOMES_SOURCE_DIR} / "reference" / "fixtures" / "ballistics";
    const std::string source_commit = "da885ca68b2ae63154a004574fed00eb9dfeb458";
    const std::vector<std::string> fixture_names = {
        "flight_profiles.json", "contact_profiles.json", "he_profiles.json",
        "batch_profiles.json", "trace_profiles.json"};
    bool fixtures_valid = true;
    for (const std::string& name : fixture_names) {
        const std::string contents = readFile(fixture_root / name);
        fixtures_valid = fixtures_valid && !contents.empty() &&
                         contents.find("\"schema\": \"ballistics-fixtures-1\"") !=
                             std::string::npos &&
                         contents.find(source_commit) != std::string::npos;
    }
    validation.check(fixtures_valid, "fixtures.schema_and_source_pin");
    const std::string manifest = readFile(fixture_root / "README.txt");
    validation.check(manifest.find("ballistics-fixtures-1") != std::string::npos &&
                         manifest.find("sourceCommit=") != std::string::npos,
                     "fixtures.manifest_contract");

    BallisticsProfile profile{};
    validation.check(profile.max_projectiles == 256U && profile.max_fragments == 2048U &&
                         std::abs(profile.tracking_seconds_without_ground - 15.0F) < 1.0e-6F &&
                         std::abs(profile.tracking_seconds_with_ground - 120.0F) < 1.0e-6F,
                     "policy.capacity_and_tracking");

    const StrategyId strategy_value = strategy_id("validation.ballistics.strategy");
    const CaliberId caliber_value = caliber_id("validation.ballistics.caliber");
    const VariantId variant_value = variant_id("validation.ballistics.variant");
    AmmunitionStrategy strategy{};
    strategy.id = strategy_value;
    strategy.caliber_id = caliber_value;
    strategy.variant_id = variant_value;
    strategy.drag_coefficient = 0.0F;
    const AmmunitionCatalog flight_catalog = makeCatalog(strategy);
    const ProjectileState flight_projectile = makeProjectile(flight_catalog, 11U);
    const FlightEnvironment vacuum{{}, {}, 0.0F, 343.0F, 1.0F};
    const auto projectile_interval = FlightIntegrator::chooseInterval(
        {&flight_projectile,
         flight_catalog.strategy(strategy_value),
         &vacuum,
         1.0F,
         flight_projectile.diameter_m,
         false});
    const auto fragment_interval = FlightIntegrator::chooseInterval(
        {&flight_projectile,
         flight_catalog.strategy(strategy_value),
         &vacuum,
         1.0F,
         flight_projectile.diameter_m,
         true});
    validation.check(projectile_interval && fragment_interval &&
                         std::abs(projectile_interval.value() - 0.06F) < 1.0e-6F &&
                         std::abs(fragment_interval.value() - 0.03F) < 1.0e-6F,
                     "flight.adaptive_distance_policy");

    std::vector<ProjectileState> states;
    for (std::uint32_t id = 1U; id <= 32U; ++id) {
        states.push_back(makeProjectile(flight_catalog, id));
    }
    const auto scalar = BallisticsBatch::integrate(states, flight_catalog, vacuum, 0.02F);
    genomes::jobs::JobSystem one_worker{1U};
    genomes::jobs::JobSystem four_workers{4U};
    const auto batch_one =
        BallisticsBatch::integrate(states, flight_catalog, vacuum, 0.02F, &one_worker, 8U);
    const auto batch_four =
        BallisticsBatch::integrate(states, flight_catalog, vacuum, 0.02F, &four_workers, 8U);
    bool worker_parity = scalar && batch_one && batch_four;
    if (worker_parity) {
        for (std::size_t index = 0; index < states.size(); ++index) {
            worker_parity = worker_parity &&
                            scalar.value().status[index] == BatchFlightStatus::Integrated &&
                            batch_one.value().status[index] == BatchFlightStatus::Integrated &&
                            batch_four.value().status[index] == BatchFlightStatus::Integrated &&
                            std::abs(scalar.value().steps[index].position.x -
                                     batch_four.value().steps[index].position.x) < 1.0e-6F &&
                            std::abs(batch_one.value().steps[index].position.y -
                                     batch_four.value().steps[index].position.y) < 1.0e-6F;
        }
    }
    validation.check(worker_parity, "batch.scalar_one_four_worker_parity");

    AmmunitionStrategy he_strategy = strategy;
    he_strategy.construction = ConstructionKind::HighExplosive;
    he_strategy.explosive = true;
    he_strategy.fuze = FuzeMode::ArmedContact;
    he_strategy.fragmentation = {40U, 0.8F, 0.6F, 0.15F, 1.0F, 0.3F};
    const AmmunitionCatalog he_catalog = makeCatalog(he_strategy, 100.0F);
    const ProjectileState he_projectile = makeProjectile(he_catalog, 500U);
    const DetonationInput full_capacity{701U, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}, true,
                                        2048U};
    const auto full_event = Detonation::detonate(
        he_projectile, *he_catalog.find(ammunition_id("validation.ballistics.ammo")), he_strategy,
        full_capacity);
    validation.check(full_event && full_event.value().fragments.size() == 40U &&
                         full_event.value().ledger.omitted_mass < 1.0e-6F &&
                         full_event.value().ledger.unrepresented_energy < 1.0e-5F,
                     "he.full_capacity_mass_energy");

    const auto bounded_event = Detonation::detonate(
        he_projectile, *he_catalog.find(ammunition_id("validation.ballistics.ammo")), he_strategy,
        {702U, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}, true, 19U});
    validation.check(bounded_event && bounded_event.value().fragments.size() == 18U &&
                         bounded_event.value().ledger.omitted_mass > 0.0F &&
                         bounded_event.value().ledger.unrepresented_energy > 0.0F,
                     "he.whole_pair_capacity_omission");

    bool pair_momentum = false;
    if (full_event && full_event.value().fragments.size() >= 2U) {
        const auto& first = full_event.value().fragments[0];
        const auto& second = full_event.value().fragments[1];
        pair_momentum = std::abs((first.velocity.x + second.velocity.x) -
                                 2.0F * he_projectile.velocity.x) < 1.0e-4F;
    }
    validation.check(pair_momentum, "he.opposite_pair_momentum");

    bool trace_tree = false;
    if (full_event && !full_event.value().fragments.empty()) {
        const FireRequest child = Detonation::makeFireRequest(
            full_event.value(), full_event.value().fragments.front(), 4U);
        trace_tree = child.parent_trace_id.has_value() &&
                     child.parent_trace_id.value() == he_projectile.trace_id;
    }
    validation.check(trace_tree, "trace.child_parent_identity");

    std::vector<BallisticsBatchEvent> events{{{4U, 20U, 1U}, 0U, 2U},
                                             {{3U, 20U, 0U}, 0U, 1U},
                                             {{3U, 10U, 0U}, 1U, 3U}};
    BallisticsBatch::stableSortEvents(events);
    validation.check(events.size() == 3U && events[0].key.tick == 3U &&
                         events[0].key.projectile_id == 10U &&
                         events[1].key.projectile_id == 20U && events[2].key.tick == 4U,
                     "batch.stable_event_commit_key");

    validation.check(std::filesystem::exists(fixture_root / "flight_profiles.json") &&
                         std::filesystem::exists(fixture_root / "he_profiles.json"),
                     "fixtures.required_categories");

    std::cout << "VALIDATION " << validation.passed << '/' << validation.total << '\n';
    return validation.passed == validation.total ? 0 : 1;
}
