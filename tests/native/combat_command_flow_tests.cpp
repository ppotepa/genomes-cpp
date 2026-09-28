#include <genomes/combat/CombatSystem.hpp>
#include <genomes/weapons/WeaponCatalog.hpp>

#include <cassert>

int main() {
    using namespace genomes;
    const auto* rifle = weapons::WeaponCatalog::find("rifle");
    assert(rifle != nullptr);
    const foundation::StableId source{2U};
    const simulation::EntityId target{4U, 1U};
    combat::CombatCommandFlow flow;
    const weapons::FireIntent late{source, rifle->id, rifle->ammunition_id, 2U,
                                   {1.0F, 1.5F, 2.0F}, {0.0F, 0.0F, 1.0F}, {4U}};
    const weapons::FireIntent early{source, rifle->id, rifle->ammunition_id, 1U,
                                    {0.0F, 1.5F, 2.0F}, {0.0F, 0.0F, 1.0F}, {3U}};
    assert(flow.submitFire(late));
    assert(flow.submitFire(early));
    assert(!flow.submitFire(early));
    const auto requests = flow.commitFire(0xCAFEU);
    assert(requests && requests.value().size() == 2U);
    assert(requests.value()[0].shot_sequence == 1U);
    assert(requests.value()[0].seed != requests.value()[1].seed);
    assert(flow.presentation().size() == 2U);

    combat::ImpactEvent impact{{2U, 1U}, target, combat::ImpactTargetKind::Infantry,
                               foundation::stable_id("torso"), {0.0F, 1.0F, 3.0F},
                               {0.0F, 1.0F, 0.0F}, 12.0F, {4U}, 7U};
    assert(flow.submitImpact(impact));
    assert(!flow.submitImpact(impact));
    const auto damage = flow.commitDamage();
    assert(damage && damage.value().size() == 1U);
    assert(damage.value().front().target == target);
    flow.clear();
    assert(flow.presentation().empty());
    return 0;
}
