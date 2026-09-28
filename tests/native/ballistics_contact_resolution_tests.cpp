#include <genomes/ballistics/ContactResolver.hpp>

#include <cassert>
#include <cmath>
#include <utility>
#include <vector>

namespace {

genomes::ballistics::AmmunitionCatalog makeCatalog() {
    using namespace genomes::ballistics;
    const StrategyId strategy_value = strategy_id("contact.strategy");
    const CaliberId caliber_value = caliber_id("contact.caliber");
    const VariantId variant_value = variant_id("contact.variant");
    AmmunitionStrategy strategy{};
    strategy.id = strategy_value;
    strategy.caliber_id = caliber_value;
    strategy.variant_id = variant_value;
    strategy.drag_coefficient = 0.0F;
    strategy.ricochet_threshold = 0.35F;
    AmmunitionCatalog catalog;
    assert(catalog.add({ammunition_id("contact.ammo"),
                        strategy_value,
                        caliber_value,
                        variant_value,
                        0.01F,
                        0.005F,
                        0.005F,
                        120.0F,
                        0.25F,
                        1U,
                        "contact"},
                       strategy));
    assert(catalog.freeze());
    return catalog;
}

genomes::destruction::MaterialAssembly makeAssembly(
    const genomes::destruction::MaterialCatalog& catalog,
    std::vector<genomes::destruction::Layer> layers) {
    const auto result = genomes::destruction::MaterialAssembly::create(
        501U, genomes::destruction::MaterialFrame::identity(), std::move(layers), catalog);
    assert(result);
    return std::move(result.value());
}

genomes::ballistics::ProjectileState makeProjectile(
    const genomes::ballistics::AmmunitionCatalog& catalog,
    genomes::foundation::Vec3 direction) {
    using namespace genomes::ballistics;
    FireRequest request{};
    request.projectile_id = ProjectileId{601U};
    request.shot_id = ShotId{602U};
    request.trace_id = TraceId{603U};
    request.ammunition_id = ammunition_id("contact.ammo");
    request.direction = direction;
    const auto state = ProjectileState::create(request, catalog);
    assert(state);
    return std::move(state.value());
}

} // namespace

int main() {
    using namespace genomes::ballistics;
    using namespace genomes::destruction;

    const MaterialCatalog materials = MaterialCatalog::makeDefault();
    const AmmunitionCatalog catalog = makeCatalog();
    const PhysicalSolidId solid = PhysicalSolidId::fromName("contact-solid");
    MaterialAssembly foliage = makeAssembly(
        materials,
        {{MaterialId::fromName("foliage"), solid, 0.0F, 0.20F, LayerKind::Solid},
         {{}, {}, 0.20F, 0.30F, LayerKind::Void},
         {MaterialId::fromName("foliage"), solid, 0.30F, 0.80F, LayerKind::Solid}});

    ProjectileState penetrating = makeProjectile(catalog, {0.0F, -1.0F, 0.0F});
    ContactCandidate candidate{};
    candidate.contact_id = 701U;
    candidate.point = {};
    candidate.normal = {0.0F, 1.0F, 0.0F};
    candidate.entry_point = {0.0F, 0.0F, 0.0F};
    candidate.exit_point = {0.0F, 1.0F, 0.0F};
    candidate.assembly = &foliage;
    const auto penetration = ContactResolver::resolve(penetrating, candidate, catalog, materials);
    assert(penetration);
    assert(penetration.value().outcome == ContactOutcome::Penetrated);
    assert(penetration.value().continue_flight);
    assert(penetration.value().impulse_token == 701U);
    assert(penetration.value().impact.energy.material_work > 0.0F);
    assert(penetrating.apply(penetration.value().transition));
    assert(penetrating.setVelocity(penetration.value().outgoing_velocity));
    assert(penetrating.projectile_id.value() == 601U);
    assert(penetrating.trace_id.value() == 603U);

    ProjectileState grazing = makeProjectile(catalog, {1.0F, -0.03F, 0.0F});
    const auto ricochet = ContactResolver::resolve(grazing, candidate, catalog, materials);
    assert(ricochet);
    assert(ricochet.value().outcome == ContactOutcome::Ricocheted ||
           ricochet.value().outcome == ContactOutcome::Glanced);
    assert(ricochet.value().continue_flight);
    assert(ricochet.value().outgoing_velocity.y > 0.0F);

    MaterialAssembly concrete = makeAssembly(
        materials,
        {{MaterialId::fromName("concrete"), solid, 0.0F, 2.0F, LayerKind::Solid}});
    ProjectileState stopped = makeProjectile(catalog, {0.0F, -1.0F, 0.0F});
    ContactCandidate thick = candidate;
    thick.contact_id = 702U;
    thick.assembly = &concrete;
    thick.exit_point = {0.0F, 2.0F, 0.0F};
    const auto stop = ContactResolver::resolve(stopped, thick, catalog, materials);
    assert(stop);
    assert(stop.value().outcome == ContactOutcome::Stopped);
    assert(!stop.value().continue_flight);
    return 0;
}
