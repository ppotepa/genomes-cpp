#include <genomes/simulation/SessionSimulationClock.hpp>

#include <cassert>
#include <chrono>

int main() {
    using namespace genomes;
    assert(simulation::secondsToNextTick(1.0 / 60.0) == 1U);
    assert(simulation::secondsToNextTick(1.0 / 60.0 + 0.00001) == 2U);
    assert(simulation::secondsToNextTick(0.5, 30U) == 15U);
    simulation::SessionSimulationClock clock;
    std::uint32_t contexts = 0U;
    const auto result = clock.advanceBy(std::chrono::milliseconds(17),
        [&contexts](const simulation::TickContext& context) {
            assert(context.tick_rate_hz == 60U);
            assert(context.fixed_dt_seconds == 1.0 / 60.0);
            assert(context.tick.value == 1U);
            ++contexts;
        });
    assert(result.steps_executed == contexts);
    assert(clock.currentTick().value == 1U);

    clock.resetAll();
    const auto plan = clock.planBy(std::chrono::seconds(1),
        simulation::TickSchedulingMode::DeterministicCapture);
    assert(plan.tick_count == 8U);
    assert(plan.advance.steps_executed == 8U);
    assert(plan.advance.dropped_time > foundation::Nanoseconds::zero());
    assert(plan.requiresCompletionBeforePublish());
    for (std::uint32_t index = 0; index < plan.tick_count; ++index) {
        assert(plan.ticks[index].tick.value == index + 1U);
    }
    simulation::SessionSimulationClock bounded({64U, std::chrono::milliseconds(250)});
    const auto bounded_plan = bounded.planBy(std::chrono::seconds(1));
    assert(bounded_plan.tick_count == 8U);
    return 0;
}
