#include <genomes/ballistics/BallisticsBatch.hpp>

#include <cassert>
#include <cmath>
#include <utility>

namespace {

genomes::ballistics::AmmunitionCatalog makeCatalog() {
    using namespace genomes::ballistics;
    const StrategyId strategy_value = strategy_id("batch.strategy");
    const CaliberId caliber_value = caliber_id("batch.caliber");
    const VariantId variant_value = variant_id("batch.variant");
    AmmunitionStrategy strategy{};
    strategy.id = strategy_value;
    strategy.caliber_id = caliber_value;
    strategy.variant_id = variant_value;
    strategy.drag_coefficient = 0.1F;
    AmmunitionCatalog catalog;
    assert(catalog.add({ammunition_id("batch.ammo"),
                        strategy_value,
                        caliber_value,
                        variant_value,
                        0.01F,
                        0.01F,
                        0.01F,
                        100.0F,
                        0.25F,
                        1U,
                        "batch"},
                       strategy));
    assert(catalog.freeze());
    return catalog;
}

genomes::ballistics::ProjectileState makeProjectile(
    const genomes::ballistics::AmmunitionCatalog& catalog,
    std::uint32_t id) {
    using namespace genomes::ballistics;
    FireRequest request{};
    request.projectile_id = ProjectileId{id};
    request.shot_id = ShotId{id + 1000U};
    request.trace_id = TraceId{id + 2000U};
    request.ammunition_id = ammunition_id("batch.ammo");
    request.position = {static_cast<float>(id), 0.0F, 0.0F};
    request.direction = {1.0F, 0.01F * static_cast<float>(id % 3U), 0.0F};
    request.seed = id * 17U;
    const auto result = ProjectileState::create(request, catalog);
    assert(result);
    return result.value();
}

} // namespace

int main() {
    using namespace genomes::ballistics;
    using namespace genomes::world;

    const AmmunitionCatalog catalog = makeCatalog();
    std::vector<ProjectileState> projectiles;
    for (std::uint32_t id = 1; id <= 17U; ++id) {
        projectiles.push_back(makeProjectile(catalog, id));
    }

    ProjectileStorage storage{{32U, 4U}};
    std::vector<ProjectileHandle> handles;
    for (const ProjectileState& state : projectiles) {
        const auto inserted = storage.insert(state);
        assert(inserted);
        handles.push_back(inserted.value());
    }
    assert(storage.activeCount() == projectiles.size());
    const auto stable_handles = storage.stableHandles();
    for (std::size_t index = 1; index < stable_handles.size(); ++index) {
        assert(storage.get(stable_handles[index - 1U])->projectile_id <
               storage.get(stable_handles[index])->projectile_id);
    }
    const ProjectileHandle removed = handles[4];
    assert(storage.erase(removed));
    assert(storage.get(removed) == nullptr);
    const auto replacement = storage.insert(makeProjectile(catalog, 900U));
    assert(replacement);
    assert(replacement.value().generation != removed.generation ||
           replacement.value().slot != removed.slot);
    storage.stableCompact();
    std::size_t chunked = 0;
    for (const ProjectileChunk& chunk : storage.activeChunks()) {
        assert(chunk.batch_index < 8U);
        chunked += chunk.handles.size();
    }
    assert(chunked == storage.activeCount());

    const FlightEnvironment environment{{1.0F, 0.0F, 0.0F}, {}, 0.0F, 343.0F, 1.0F};
    const auto scalar = BallisticsBatch::integrate(projectiles, catalog, environment, 0.02F);
    assert(scalar);
    genomes::jobs::JobSystem worker_jobs{2U};
    const auto parallel =
        BallisticsBatch::integrate(projectiles, catalog, environment, 0.02F, &worker_jobs, 4U);
    assert(parallel);
    const auto storage_batch =
        BallisticsBatch::integrate(storage, catalog, environment, 0.02F, nullptr, 4U);
    assert(storage_batch);
    assert(scalar.value().valid() && parallel.value().valid());
    for (std::size_t index = 0; index < projectiles.size(); ++index) {
        assert(scalar.value().status[index] == BatchFlightStatus::Integrated);
        assert(parallel.value().status[index] == BatchFlightStatus::Integrated);
        assert(std::abs(scalar.value().steps[index].position.x -
                        parallel.value().steps[index].position.x) < 1.0e-6F);
        assert(std::abs(scalar.value().steps[index].position.y -
                        parallel.value().steps[index].position.y) < 1.0e-6F);
        assert(std::abs(scalar.value().steps[index].travel_distance_m -
                        parallel.value().steps[index].travel_distance_m) < 1.0e-6F);
    }

    const WorldId world_id{123U};
    QueryRegion region{};
    region.coordinate = {0, 0, 0};
    region.id = regionId(world_id, region.coordinate);
    region.revision = 1U;
    region.resident = true;
    region.candidates.push_back({77U,
                                 {{0.5F, -1.0F, -1.0F}, {1.5F, 1.0F, 1.0F}},
                                 QuerySourceKind::Static,
                                 region.id,
                                 1U});
    const auto snapshot_result =
        WorldQuerySnapshot::create(world_id, {16.0}, {std::move(region)});
    assert(snapshot_result);
    const WorldQuerySnapshot snapshot = std::move(snapshot_result.value());
    const std::vector<QuerySegmentRequest> requests{{{0.0F, 0.0F, 0.0F},
                                                     {2.0F, 0.0F, 0.0F}},
                                                    {{3.0F, 0.0F, 0.0F},
                                                     {4.0F, 0.0F, 0.0F}}};
    const auto query_results = BallisticsBatch::querySegments(snapshot, requests);
    assert(query_results.size() == requests.size());
    assert(query_results[0].hits.size() == 1U);
    assert(query_results[0].hits.front().candidate.id == 77U);
    assert(query_results[1].hits.empty());

    std::vector<BallisticsBatchEvent> events{{{2U, 10U, 1U}, 0U, 2U},
                                             {{1U, 30U, 0U}, 1U, 3U},
                                             {{1U, 20U, 0U}, 0U, 1U}};
    BallisticsBatch::stableSortEvents(events);
    assert(events[0].key.projectile_id == 20U);
    assert(events[1].key.projectile_id == 30U);
    assert(events[2].key.projectile_id == 10U);
    return 0;
}
