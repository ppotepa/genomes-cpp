#include <genomes/ballistics/Detonation.hpp>
#include <genomes/ballistics/BallisticsWorld.hpp>
#include <genomes/world/WorldQuery.hpp>
#include <genomes/destruction/MaterialAssembly.hpp>

#include <cassert>
#include <cmath>
#include <utility>

namespace {

struct ContactContext final {
    const genomes::destruction::MaterialAssembly* assembly{nullptr};
};

bool contactProvider(void* raw,
                    const genomes::world::QuerySegmentHit& hit,
                    genomes::ballistics::ContactCandidate& result) noexcept {
    const auto* context = static_cast<const ContactContext*>(raw);
    result.contact_id = hit.candidate.id;
    result.point = hit.point;
    result.normal = {0.0F, 1.0F, 0.0F};
    result.entry_point = hit.point;
    result.exit_point = {hit.point.x + 0.05F, hit.point.y, hit.point.z};
    result.target_center_of_mass = hit.point;
    result.assembly = context == nullptr ? nullptr : context->assembly;
    return result.assembly != nullptr;
}

} // namespace

int main() {
    using namespace genomes::ballistics;

    const StrategyId strategy_value = strategy_id("he.strategy");
    const CaliberId caliber_value = caliber_id("he.caliber");
    const VariantId variant_value = variant_id("he.variant");
    AmmunitionStrategy strategy{};
    strategy.id = strategy_value;
    strategy.caliber_id = caliber_value;
    strategy.variant_id = variant_value;
    strategy.construction = ConstructionKind::HighExplosive;
    strategy.explosive = true;
    strategy.fuze = FuzeMode::ArmedContact;
    strategy.nose_crush_work_j = 10.0F;
    strategy.fragmentation = {5U, 0.8F, 0.6F, 0.0F, 1.0F, 0.3F};

    AmmunitionDefinition ammunition{};
    ammunition.id = ammunition_id("he.ammo");
    ammunition.strategy_id_value = strategy_value;
    ammunition.caliber_id_value = caliber_value;
    ammunition.variant_id_value = variant_value;
    ammunition.mass_kg = 0.02F;
    ammunition.diameter_m = 0.02F;
    ammunition.drag_diameter_m = 0.02F;
    ammunition.muzzle_velocity_mps = 200.0F;
    ammunition.explosive_energy_j = 100.0F;

    AmmunitionCatalog catalog;
    assert(catalog.add(ammunition, strategy));
    assert(catalog.freeze());

    FireRequest request{};
    request.projectile_id = ProjectileId{1001U};
    request.shot_id = ShotId{1002U};
    request.trace_id = TraceId{1003U};
    request.ammunition_id = ammunition.id;
    request.position = {1.0F, 2.0F, 3.0F};
    request.direction = {1.0F, 0.0F, 0.0F};
    request.seed = 0x12345678U;
    const auto created = ProjectileState::create(request, catalog);
    assert(created);

    const auto invalid = Detonation::detonate(
        created.value(), ammunition, strategy, {77U, {}, {0.0F, 1.0F, 0.0F}, false, 8U});
    assert(!invalid);

    const auto event_result = Detonation::detonate(
        created.value(), ammunition, strategy, {77U, {2.0F, 2.0F, 3.0F}, {0.0F, 1.0F, 0.0F},
                                                true, 4U});
    assert(event_result);
    const DetonationEvent& event = event_result.value();
    assert(event.valid());
    assert(event.blast.valid());
    assert(event.blast.energy_j > 0.0F);
    assert(event.fragments.size() == 4U); // two complete opposite pairs
    assert(event.ledger.omitted_mass > 0.0F);
    assert(event.ledger.unrepresented_energy > 0.0F);
    assert(std::abs(event.ledger.represented_mass + event.ledger.omitted_mass -
                    event.ledger.requested_mass) < 1.0e-6F);

    for (std::size_t index = 0; index < event.fragments.size(); index += 2U) {
        const auto& left = event.fragments[index];
        const auto& right = event.fragments[index + 1U];
        const auto radial_x = (left.velocity.x + right.velocity.x) -
                              2.0F * created.value().velocity.x;
        const auto radial_y = (left.velocity.y + right.velocity.y) -
                              2.0F * created.value().velocity.y;
        const auto radial_z = (left.velocity.z + right.velocity.z) -
                              2.0F * created.value().velocity.z;
        assert(std::abs(radial_x) < 1.0e-4F);
        assert(std::abs(radial_y) < 1.0e-4F);
        assert(std::abs(radial_z) < 1.0e-4F);
    }

    const FireRequest fragment_request = Detonation::makeFireRequest(
        event, event.fragments.front(), 9U);
    assert(fragment_request.fragment);
    assert(fragment_request.parent_trace_id.has_value());
    assert(fragment_request.velocity_override.has_value());
    const auto fragment = ProjectileState::create(fragment_request, catalog);
    assert(fragment);
    assert(fragment.value().fragment);
    assert(std::abs(fragment.value().mass_kg - event.fragments.front().mass_kg) < 1.0e-6F);

    genomes::world::QueryRegion region{};
    const genomes::world::WorldId world_id{991U};
    region.coordinate = {0, 0, 0};
    region.id = genomes::world::regionId(world_id, region.coordinate);
    region.revision = 1U;
    region.resident = true;
    region.candidates.push_back({9001U,
                                 {{0.8F, -1.0F, -1.0F}, {0.9F, 1.0F, 1.0F}},
                                 genomes::world::QuerySourceKind::Static,
                                 region.id,
                                 1U});
    const auto snapshot_result =
        genomes::world::WorldQuerySnapshot::create(world_id, {16.0}, {std::move(region)});
    assert(snapshot_result);
    genomes::world::WorldQuerySnapshot snapshot = std::move(snapshot_result.value());

    const auto materials = genomes::destruction::MaterialCatalog::makeDefault();
    const auto assembly_result = genomes::destruction::MaterialAssembly::create(
        77U,
        genomes::destruction::MaterialFrame::identity(),
        {{genomes::destruction::MaterialId::fromName("concrete"),
          genomes::destruction::PhysicalSolidId::fromName("he-solid"),
          0.0F,
          1.0F,
          genomes::destruction::LayerKind::Solid}},
        materials);
    assert(assembly_result);
    const auto assembly = std::move(assembly_result.value());
    ContactContext context{&assembly};
    BallisticsProfile profile{};
    profile.fixed_step_seconds = 1.0F / 60.0F;
    BallisticsWorld world{std::move(catalog),
                          &snapshot,
                          profile,
                          {},
                          materials,
                          contactProvider,
                          &context};
    FireRequest world_request = request;
    world_request.position = {0.0F, 0.0F, 0.0F};
    assert(world.queueFire(world_request).accepted);
    const BallisticsTickResult world_tick = world.advanceFixed(false);
    assert(world_tick.detonations.size() == 1U);
    assert(world_tick.detonations.front().blast.valid());
    assert(world_tick.contacts.size() == 1U);
    assert(world.pendingCount() == 5U);
    assert(world.activeCount() == 0U);
    return 0;
}
