#include <genomes/ballistics/BallisticsWorld.hpp>

#include <cassert>
#include <cmath>
#include <utility>
#include <vector>

namespace {

genomes::ballistics::AmmunitionCatalog catalog() {
    using namespace genomes::ballistics;
    const StrategyId strategy_value = strategy_id("flight.strategy");
    const CaliberId caliber_value = caliber_id("flight.caliber");
    const VariantId variant_value = variant_id("flight.variant");
    AmmunitionStrategy strategy{};
    strategy.id = strategy_value;
    strategy.caliber_id = caliber_value;
    strategy.variant_id = variant_value;
    strategy.drag_coefficient = 0.0F;
    assert(strategy.valid());
    AmmunitionCatalog result;
    assert(result.add({ammunition_id("flight.ammo"),
                       strategy_value,
                       caliber_value,
                       variant_value,
                       0.01F,
                       0.01F,
                       0.01F,
                       100.0F,
                       0.25F,
                       1U,
                       "flight-test"},
                      strategy));
    assert(result.freeze());
    return result;
}

genomes::ballistics::FireRequest fire() {
    using namespace genomes::ballistics;
    FireRequest request{};
    request.projectile_id = ProjectileId{1U};
    request.shot_id = ShotId{2U};
    request.trace_id = TraceId{3U};
    request.ammunition_id = ammunition_id("flight.ammo");
    request.position = {0.0F, 0.0F, 0.0F};
    request.direction = {1.0F, 0.0F, 0.0F};
    request.tick = 0U;
    return request;
}

} // namespace

int main() {
    using namespace genomes::ballistics;
    using namespace genomes::world;

    AmmunitionCatalog flight_catalog = catalog();
    const auto interval = FlightIntegrator::chooseInterval({nullptr, nullptr, nullptr, 1.0F,
                                                            0.01F, false});
    assert(!interval);

    ProjectileState state = ProjectileState::create(fire(), flight_catalog).value();
    FlightEnvironment vacuum{};
    vacuum.gravity = {};
    vacuum.density = 0.0F;
    const auto integrated = FlightIntegrator::integrate(
        {&state, flight_catalog.strategy(state.strategy_id), &vacuum, 0.1F, state.diameter_m, false},
        0.01F);
    assert(integrated);
    assert(std::abs(integrated.value().position.x - 1.0F) < 1.0e-4F);
    assert(std::abs(integrated.value().velocity.x - 100.0F) < 1.0e-4F);

    const WorldId world_id{41U};
    const RegionCoord coordinate{0, 0, 0};
    const RegionId region_id = regionId(world_id, coordinate);
    QueryRegion query_region{};
    query_region.coordinate = coordinate;
    query_region.id = region_id;
    query_region.revision = 1U;
    query_region.resident = true;
    query_region.candidates.push_back({900U,
                                       {{0.8F, -1.0F, -1.0F}, {0.9F, 1.0F, 1.0F}},
                                       QuerySourceKind::Static,
                                       region_id,
                                       1U});
    const auto snapshot_result =
        WorldQuerySnapshot::create(world_id, {16.0}, {std::move(query_region)});
    assert(snapshot_result);
    WorldQuerySnapshot snapshot = std::move(snapshot_result.value());

    BallisticsProfile profile{};
    profile.fixed_step_seconds = 1.0F / 60.0F;
    BallisticsWorld world{std::move(flight_catalog), &snapshot, profile, vacuum};
    ProjectileTrace trace;
    assert(world.queueFire(fire()).accepted);
    const BallisticsTickResult tick = world.advanceFixed(false, &trace);
    assert(tick.contacts.size() == 1U);
    assert(tick.terminals.size() == 1U);
    assert(tick.terminals.front().trace.reason == TerminalReason::Stopped);
    assert(world.activeCount() == 0U);
    assert(trace.segments().size() == 1U);
    assert(trace.contacts().size() == 1U);
    assert(trace.terminals().size() == 1U);

    BallisticsProfile tracking_profile{};
    tracking_profile.fixed_step_seconds = 0.02F;
    tracking_profile.tracking_seconds_without_ground = 0.01F;
    BallisticsWorld tracking_world{catalog(), nullptr, tracking_profile, vacuum};
    ProjectileTrace tracking_trace;
    assert(tracking_world.queueFire(fire()).accepted);
    const BallisticsTickResult tracking_tick = tracking_world.advanceFixed(false, &tracking_trace);
    assert(tracking_tick.terminals.size() == 1U);
    assert(tracking_tick.terminals.front().trace.reason == TerminalReason::TrackingLimit);
    return 0;
}
