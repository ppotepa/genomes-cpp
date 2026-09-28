#include <genomes/combat/LineOfSight.hpp>

#include <cassert>
#include <utility>
#include <vector>

namespace {

genomes::world::QueryRegion makeRegion(genomes::world::WorldId world_id, bool resident,
                                       std::vector<genomes::world::QueryCandidate> candidates = {}) {
    const genomes::world::RegionCoord coordinate{0, 0, 0};
    const genomes::world::RegionId id = genomes::world::regionId(world_id, coordinate);
    return {coordinate, id, 3U, resident, std::move(candidates), nullptr, nullptr};
}

} // namespace

int main() {
    using namespace genomes;
    const world::WorldId world_id{3U};
    const world::RegionId region_id = world::regionId(world_id, {0, 0, 0});
    const world::QueryCandidate target{99U, {{-0.5F, 0.5F, 9.0F}, {0.5F, 1.5F, 9.8F}},
                                       world::QuerySourceKind::Dynamic, region_id, 3U};
    const world::QueryCandidate blocker{42U, {{-0.5F, 0.5F, 4.0F}, {0.5F, 1.5F, 5.0F}},
                                         world::QuerySourceKind::Static, region_id, 3U};
    auto created = world::WorldQuerySnapshot::create(
        world_id, {100.0}, {makeRegion(world_id, true, {target, blocker})});
    assert(created);
    const auto& snapshot = created.value();

    const combat::LOSRequest blocked{1U, 99U, {0.0F, 1.0F, 0.0F}, {0.0F, 1.0F, 10.0F}};
    const auto blocked_result = combat::LineOfSight::queryOne(snapshot, blocked);
    assert(blocked_result && !blocked_result.value().visible);
    assert(blocked_result.value().blocker.has_value() && blocked_result.value().blocker.value() == 42U);

    auto target_only = world::WorldQuerySnapshot::create(
        world_id, {100.0}, {makeRegion(world_id, true, {target})});
    assert(target_only);
    const auto visible = combat::LineOfSight::queryOne(target_only.value(), blocked);
    assert(visible && visible.value().visible);

    auto stale = world::WorldQuerySnapshot::create(world_id, {100.0}, {makeRegion(world_id, false)});
    assert(stale);
    const auto stale_result = combat::LineOfSight::queryOne(stale.value(), blocked);
    assert(stale_result && !stale_result.value().visible &&
           stale_result.value().completeness == world::QueryCompleteness::PartialUnloaded);

    const combat::LOSRequest other{2U, 99U, {0.0F, 1.0F, 0.0F}, {0.0F, 1.0F, 10.0F}};
    const std::vector<combat::LOSRequest> batch{blocked, other};
    const auto batch_result = combat::LineOfSight::query(target_only.value(), batch);
    assert(batch_result && batch_result.value().size() == 2U);
    assert(combat::LineOfSight::lastStats().visible == 2U);
    return 0;
}
