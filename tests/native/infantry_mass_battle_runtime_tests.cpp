#include <genomes/gameplay/InfantryMassBattleRuntime.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <unordered_set>

namespace {

using Runtime = genomes::gameplay::InfantryMassBattleRuntime;
using Config = genomes::gameplay::InfantryMassBattleConfig;
using State = genomes::gameplay::InfantryMassBattleRenderState;

void assertSameStates(const std::vector<State>& expected,
                      const std::vector<State>& actual) {
    assert(expected.size() == actual.size());
    for (std::size_t index = 0U; index < expected.size(); ++index) {
        const State& lhs = expected[index];
        const State& rhs = actual[index];
        assert(lhs.entity == rhs.entity);
        assert(lhs.team == rhs.team);
        assert(lhs.position.x == rhs.position.x);
        assert(lhs.position.y == rhs.position.y);
        assert(lhs.position.z == rhs.position.z);
        assert(lhs.heading == rhs.heading);
        assert(lhs.animation_phase == rhs.animation_phase);
        assert(lhs.animation_speed == rhs.animation_speed);
        assert(lhs.animation_variant == rhs.animation_variant);
    }
}

} // namespace

int main() {
    using namespace genomes;

    Config config{};
    config.seed = 0xA11CE55U;
    auto first_result = Runtime::start(config);
    auto second_result = Runtime::start(config);
    assert(first_result);
    assert(second_result);

    Runtime& first = *first_result.value();
    Runtime& second = *second_result.value();
    assert(first.entities().size() == 2000U);
    assert(first.snapshot().total_units == 2000U);
    assert(first.snapshot().blue_units == 1000U);
    assert(first.snapshot().red_units == 1000U);
    assert(first.renderStates().size() == 2000U);
    assertSameStates(first.renderStates(), second.renderStates());

    std::unordered_set<std::uint8_t> variants;
    float maximum_abs_x = 0.0F;
    float maximum_abs_z = 0.0F;
    for (const State& state : first.renderStates()) {
        variants.insert(state.animation_variant);
        const float half_map = static_cast<float>(config.map_size_m) * 0.5F;
        assert(state.position.x >= -half_map && state.position.x <= half_map);
        assert(state.position.z >= -half_map && state.position.z <= half_map);
        maximum_abs_x = std::max(maximum_abs_x, std::abs(state.position.x));
        maximum_abs_z = std::max(maximum_abs_z, std::abs(state.position.z));
    }
    assert(variants.size() >= 4U);
    assert(maximum_abs_x < 200.0F);
    assert(maximum_abs_z < 60.0F);

    const auto initial_states = first.renderStates();
    for (std::uint64_t tick = 1U; tick <= 120U; ++tick) {
        const simulation::TickContext context{
            foundation::SimulationTick{tick},
            1.0 / 60.0,
            simulation::SessionSimulationTickRateHz};
        first.fixedUpdate(context);
        second.fixedUpdate(context);
    }

    assert(first.snapshot().tick == 120U);
    assert(first.snapshot().direction_changes > 0U);
    assert(first.snapshot().average_speed_mps > 0.0F);
    assertSameStates(first.renderStates(), second.renderStates());

    bool moved = false;
    bool phase_advanced = false;
    for (std::size_t index = 0U; index < initial_states.size(); ++index) {
        const State& before = initial_states[index];
        const State& after = first.renderStates()[index];
        if (before.position.x != after.position.x || before.position.z != after.position.z) {
            moved = true;
        }
        if (before.animation_phase != after.animation_phase) phase_advanced = true;
    }
    assert(moved);
    assert(phase_advanced);
    return 0;
}
