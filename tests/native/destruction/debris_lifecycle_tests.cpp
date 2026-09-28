#include <genomes/destruction/DebrisLifecycle.hpp>

#include <cassert>
#include <cmath>
#include <utility>
#include <vector>

namespace {

genomes::destruction::DebrisRecord record(std::uint64_t id,
                                          float importance,
                                          bool awake = true,
                                          bool representable = true) {
    return {id,
            id + 1000U,
            {{genomes::destruction::MaterialId::fromName("brick"), 1.0}},
            {static_cast<float>(id), 0.0F, 0.0F},
            {1.0F, 2.0F, 3.0F},
            {0.1F, 0.2F, 0.3F},
            genomes::destruction::DebrisRepresentation::Cheap,
            0,
            importance,
            awake,
            representable};
}

} // namespace

int main() {
    using namespace genomes::destruction;
    const DebrisPolicy policy{1, 1, 1, 2};
    const auto created = DebrisLifecycle::create(policy);
    assert(created);
    DebrisLifecycle lifecycle = std::move(created.value());

    assert(lifecycle.spawn(record(1U, 1.0F)));
    assert(lifecycle.count(DebrisRepresentation::Hero) == 1U);
    assert(lifecycle.spawn(record(2U, 0.1F)));
    assert(lifecycle.count(DebrisRepresentation::Cheap) == 1U);
    assert(lifecycle.spawn(record(3U, 0.2F)));
    assert(lifecycle.records().size() == 3U);
    assert(lifecycle.count(DebrisRepresentation::Baked) == 1U);

    const RepresentationAccounting accounting = lifecycle.accounting();
    assert(std::abs(accounting.original_source_volume - 3.0) < 1.0e-12);
    assert(std::abs(accounting.representedVolume() - accounting.original_source_volume) < 1.0e-12);
    assert(accounting.material_totals.size() == 1U);

    assert(lifecycle.sleep(1U));
    lifecycle.advance(2U);
    const auto candidates = lifecycle.prepareSettlement();
    assert(candidates.size() == 1U);
    assert(candidates.front().id == 1U);
    const std::vector<genomes::foundation::StableId> ids{1U};
    assert(lifecycle.commitSettlement(ids));
    assert(lifecycle.count(DebrisRepresentation::Baked) == 2U);
    assert(lifecycle.prepareSettlement().empty());

    const auto direct = DebrisLifecycle::create(DebrisPolicy{0, 0, 0, 1});
    assert(direct);
    DebrisLifecycle direct_lifecycle = std::move(direct.value());
    auto unrepresentable = record(9U, 1.0F, true, false);
    assert(direct_lifecycle.spawn(std::move(unrepresentable)));
    assert(direct_lifecycle.count(DebrisRepresentation::Baked) == 1U);
    assert(direct_lifecycle.records().size() == 1U);
    return 0;
}
