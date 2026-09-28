#include <genomes/simulation/FixedStepClock.hpp>

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>

int main() {
    using namespace std::chrono_literals;
    using genomes::foundation::Nanoseconds;
    using genomes::foundation::SimulationTick;

    genomes::simulation::FixedStepClock clock;
    std::uint32_t callback_count = 0;
    SimulationTick last_tick{};
    const auto callback = [&](double dt, SimulationTick tick) noexcept {
        assert(std::abs(dt - (1.0 / 60.0)) < 1e-12);
        last_tick = tick;
        ++callback_count;
    };

    const auto first = clock.advanceBy(8'333'333ns, callback);
    assert(first.steps_executed == 0);
    assert(std::abs(first.interpolation_alpha - 0.49999998) < 1e-7);
    assert(first.first_tick.value == 0);
    assert(first.next_tick.value == 0);

    const auto second = clock.advanceBy(8'333'334ns, callback);
    assert(second.steps_executed == 1);
    assert(second.first_tick.value == 0);
    assert(second.next_tick.value == 1);
    assert(last_tick.value == 0);
    assert(callback_count == 1);

    clock.resetAll();
    callback_count = 0;
    const auto large = clock.advanceBy(1s, callback);
    assert(large.steps_executed == 8);
    assert(large.next_tick.value == 8);
    assert(callback_count == 8);
    assert(large.dropped_time == Nanoseconds{866'666'666});
    assert(clock.droppedTimeTotal() == large.dropped_time);
    assert(large.interpolation_alpha == 0.0);

    const auto wall_first = clock.advance(genomes::simulation::FixedStepClock::TimePoint{}, callback);
    assert(wall_first.steps_executed == 0);
    assert(wall_first.interpolation_alpha == 1.0);
    clock.resetTiming();
    assert(clock.currentTick().value == 8);
    return 0;
}
