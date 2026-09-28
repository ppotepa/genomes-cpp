#include <genomes/weapons/WeaponCatalog.hpp>
#include <genomes/weapons/WeaponHandlingSystem.hpp>

#include <cassert>

int main() {
    using namespace genomes::weapons;
    const auto* rifle = WeaponCatalog::find("rifle");
    const auto* sidearm = WeaponCatalog::find("sidearm");
    assert(rifle != nullptr && sidearm != nullptr);
    WeaponVariant variant{};
    variant.seed = 9U;
    const auto rifle_artifact = WeaponGeometryGenerator::build(*rifle, variant);
    const auto sidearm_artifact = WeaponGeometryGenerator::build(*sidearm, variant);
    assert(rifle_artifact && sidearm_artifact);

    WeaponHandlingSystem system;
    WeaponRuntimeState state{};
    assert(system.select(state, rifle->id));
    assert(system.requestReadiness(state, 1.0F));
    WeaponHandlingInput input{42U, rifle, &rifle_artifact.value(), {0.0F, 1.0F, 0.0F},
                              genomes::foundation::Vec3{10.0F, 1.2F, 20.0F}, {}};
    WeaponStepOutput output{};
    bool saw_fire = false;
    for (std::uint64_t tick = 0U; tick < 40U; ++tick) {
        input.request_fire = tick == 35U;
        assert(system.step(state, input, {tick}, 1.0F / 60.0F, output));
        saw_fire = saw_fire || output.fire.has_value();
    }
    assert(state.handling == WeaponHandlingState::Held);
    assert(output.pose.valid());
    assert(output.pose.support.owner == HandOwnership::Support);
    assert(saw_fire);
    assert(state.shot_sequence == 1U);

    assert(system.select(state, sidearm->id));
    input = {42U, sidearm, &sidearm_artifact.value(), {0.0F, 1.0F, 0.0F},
             std::nullopt, {}};
    bool sidearm_held = false;
    for (std::uint64_t tick = 40U; tick < 80U; ++tick) {
        assert(system.step(state, input, {tick}, 1.0F / 60.0F, output));
        sidearm_held = sidearm_held || state.handling == WeaponHandlingState::Held;
    }
    assert(sidearm_held && output.pose.support.owner == HandOwnership::Free);

    WeaponRuntimeState sprint_state{};
    assert(system.select(sprint_state, rifle->id));
    assert(system.requestReadiness(sprint_state, 1.0F));
    input = {42U, rifle, &rifle_artifact.value(), {0.0F, 1.0F, 0.0F},
             std::nullopt, {true, false, false, 5.0F}, true};
    for (std::uint64_t tick = 0U; tick < 40U; ++tick) {
        assert(system.step(sprint_state, input, {tick}, 1.0F / 60.0F, output));
        assert(!output.fire.has_value());
    }
    assert(sprint_state.readiness < 0.5F);
    return 0;
}
