#include <genomes/jobs/JobSystem.hpp>
#include <genomes/simulation/SystemGraph.hpp>
#include <genomes/simulation/Cadence.hpp>

#include <atomic>
#include <cassert>
#include <latch>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>

namespace {

using genomes::simulation::SystemDescriptor;
using genomes::simulation::SystemGraph;
using genomes::simulation::SystemPhase;

SystemDescriptor descriptor(genomes::simulation::SystemId id,
                            bool main_thread_only,
                            genomes::simulation::SystemCallback callback) {
    SystemDescriptor value{};
    value.id = id;
    value.phase = SystemPhase::Sense;
    value.main_thread_only = main_thread_only;
    value.callback = std::move(callback);
    return value;
}

void mainThreadFailureDrainsAcceptedWorkerAndClearsCommands() {
    genomes::jobs::JobSystem jobs(1);
    const auto owner_thread = std::this_thread::get_id();
    SystemGraph graph;
    std::latch worker_started{1};
    std::latch allow_worker_finish{1};
    std::atomic<unsigned int> worker_finished{0};
    std::atomic<unsigned int> successor_runs{0};

    assert(graph.add(descriptor(1, false, [&](genomes::simulation::SystemContext&) {
        worker_started.count_down();
        allow_worker_finish.wait();
        worker_finished.fetch_add(1, std::memory_order_relaxed);
    })));
    assert(graph.add(descriptor(2, true, [&](genomes::simulation::SystemContext&) {
        assert(std::this_thread::get_id() == owner_thread);
        worker_started.wait();
        allow_worker_finish.count_down();
        throw std::runtime_error("expected callback failure");
    })));
    SystemDescriptor successor = descriptor(3, true, [&](genomes::simulation::SystemContext&) {
        assert(std::this_thread::get_id() == owner_thread);
        successor_runs.fetch_add(1, std::memory_order_relaxed);
    });
    successor.after = {1, 2};
    assert(graph.add(std::move(successor)));
    assert(graph.compile());

    genomes::simulation::CommandBufferSet commands;
    const auto result = graph.run({}, 1.0 / 60.0, &jobs, &commands);
    assert(!result);
    assert(worker_finished.load(std::memory_order_relaxed) == 1);
    assert(successor_runs.load(std::memory_order_relaxed) == 0);
    assert(commands.size() == 0);
}

void reverseCompletionFailureDrainsEveryAcceptedHandle() {
    genomes::jobs::JobSystem jobs(3);
    SystemGraph graph;
    std::latch both_started{2};
    std::latch failure_observed{1};
    std::atomic<unsigned int> completed{0};
    std::atomic<unsigned int> successor_runs{0};

    assert(graph.add(descriptor(10, false, [&](genomes::simulation::SystemContext&) {
        both_started.count_down();
        both_started.wait();
        failure_observed.wait();
        completed.fetch_add(1, std::memory_order_relaxed);
    })));
    assert(graph.add(descriptor(20, false, [&](genomes::simulation::SystemContext&) {
        both_started.count_down();
        both_started.wait();
        failure_observed.count_down();
        completed.fetch_add(1, std::memory_order_relaxed);
        throw std::runtime_error("expected worker failure");
    })));
    SystemDescriptor successor = descriptor(30, true, [&](genomes::simulation::SystemContext&) {
        successor_runs.fetch_add(1, std::memory_order_relaxed);
    });
    successor.after = {10, 20};
    assert(graph.add(std::move(successor)));
    assert(graph.compile());

    auto plan = graph.executionPlan();
    const auto result = plan.run({}, 1.0 / 60.0, &jobs);
    assert(!result);
    assert(completed.load(std::memory_order_relaxed) == 2);
    assert(successor_runs.load(std::memory_order_relaxed) == 0);
}

void directDependencyReleasesWithoutFrontierBarrier() {
    genomes::jobs::JobSystem jobs(3);
    SystemGraph graph;
    std::latch slow_started{1};
    std::latch release_slow{1};
    std::atomic_bool direct_successor_ran{false};

    auto slow = descriptor(100, false, [&](genomes::simulation::SystemContext&) {
        slow_started.count_down();
        release_slow.wait();
    });
    slow.phase = SystemPhase::Sense;
    assert(graph.add(std::move(slow)));
    auto prerequisite = descriptor(200, false, [&](genomes::simulation::SystemContext&) {
        slow_started.wait();
    });
    prerequisite.phase = SystemPhase::Decide;
    assert(graph.add(std::move(prerequisite)));
    SystemDescriptor direct = descriptor(300, false, [&](genomes::simulation::SystemContext& context) {
        assert(context.jobs == &jobs);
        direct_successor_ran.store(true, std::memory_order_release);
        release_slow.count_down();
    });
    direct.phase = SystemPhase::Navigate;
    direct.after = {200};
    assert(graph.add(std::move(direct)));
    assert(graph.compile());

    auto plan = graph.executionPlan();
    const auto result = plan.run({}, 1.0 / 60.0, &jobs);
    assert(result);
    assert(direct_successor_ran.load(std::memory_order_acquire));
}

void legacyRunUsesDirectDependencyScheduling() {
    genomes::jobs::JobSystem jobs(3);
    SystemGraph graph;
    std::latch slow_started{1};
    std::latch release_slow{1};
    std::atomic_bool direct_successor_ran{false};

    auto slow = descriptor(600, false, [&](genomes::simulation::SystemContext&) {
        slow_started.count_down();
        release_slow.wait();
    });
    assert(graph.add(std::move(slow)));

    auto prerequisite = descriptor(601, false, [&](genomes::simulation::SystemContext&) {
        slow_started.wait();
    });
    assert(graph.add(std::move(prerequisite)));

    auto direct = descriptor(602, false, [&](genomes::simulation::SystemContext&) {
        direct_successor_ran.store(true, std::memory_order_release);
        release_slow.count_down();
    });
    direct.after = {601};
    assert(graph.add(std::move(direct)));
    assert(graph.compile());

    const auto result = graph.run({}, 1.0 / 60.0, &jobs);
    assert(result);
    assert(direct_successor_ran.load(std::memory_order_acquire));
}

void executionPlanStartsWithoutWorkerBlocking() {
    genomes::jobs::JobSystem jobs(2);
    SystemGraph graph;
    std::atomic_uint32_t runs{0};
    assert(graph.add(descriptor(350, false, [&](genomes::simulation::SystemContext&) {
        runs.fetch_add(1, std::memory_order_relaxed);
    })));
    assert(graph.compile());
    auto plan = graph.executionPlan();
    genomes::jobs::JobCompletion completion;
    const auto owner = jobs.submit([&](genomes::jobs::JobContext&) {
        completion = plan.start({1}, 1.0 / 60.0, &jobs);
    });
    owner.wait();
    jobs.wait(completion);
    assert(completion.isComplete());
    assert(!completion.failed());
    assert(runs.load(std::memory_order_relaxed) == 1U);
}

void asynchronousExecutionPlanCommitsExactlyOnceAfterAllLeaves() {
    genomes::jobs::JobSystem jobs(2);
    const auto owner_thread = std::this_thread::get_id();
    SystemGraph graph;
    std::atomic_uint32_t leaves{0U};
    std::atomic_uint32_t commits{0U};
    assert(graph.add(descriptor(360, false, [&](genomes::simulation::SystemContext&) {
        leaves.fetch_add(1U, std::memory_order_release);
    })));
    assert(graph.add(descriptor(361, false, [&](genomes::simulation::SystemContext&) {
        leaves.fetch_add(1U, std::memory_order_release);
    })));
    assert(graph.compile());

    auto plan = graph.executionPlan();
    const auto completion = plan.start(
        {1U}, 1.0 / 60.0, &jobs, nullptr,
        [&] {
            assert(std::this_thread::get_id() == owner_thread);
            assert(leaves.load(std::memory_order_acquire) == 2U);
            commits.fetch_add(1U, std::memory_order_relaxed);
        });
    jobs.wait(completion);
    assert(completion.isComplete());
    assert(!completion.failed());
    assert(commits.load(std::memory_order_relaxed) == 1U);
}

void cadenceNoOpReportsOnlyDueSystems() {
    genomes::jobs::JobSystem jobs(genomes::jobs::SchedulerConfig{
        .mode = genomes::jobs::SchedulerMode::Serial});
    SystemGraph graph;
    std::atomic_uint32_t runs{0};
    auto periodic = descriptor(400, false, [&](genomes::simulation::SystemContext&) {
        runs.fetch_add(1, std::memory_order_relaxed);
    });
    periodic.cadence.kind = genomes::simulation::CadenceKind::EveryNTicks;
    periodic.cadence.period_ticks = 3U;
    assert(graph.add(std::move(periodic)));
    assert(graph.compile());

    auto plan = graph.executionPlan();
    std::size_t reported = 0U;
    for (std::uint64_t tick = 1U; tick <= 12U; ++tick) {
        const auto result = plan.run({tick}, 1.0 / 60.0, &jobs);
        assert(result);
        reported += result.value().systems_run;
    }
    assert(reported == runs.load(std::memory_order_relaxed));
    assert(reported == 4U);
    assert(reported < 12U);
}

void legacyGraphCadenceReportsOnlyDueSystems() {
    genomes::jobs::JobSystem jobs(genomes::jobs::SchedulerConfig{
        .mode = genomes::jobs::SchedulerMode::Serial});
    SystemGraph graph;
    std::atomic_uint32_t runs{0};
    auto periodic = descriptor(500, false, [&](genomes::simulation::SystemContext&) {
        runs.fetch_add(1, std::memory_order_relaxed);
    });
    periodic.cadence.kind = genomes::simulation::CadenceKind::EveryNTicks;
    periodic.cadence.period_ticks = 2U;
    assert(graph.add(std::move(periodic)));
    assert(graph.compile());

    std::size_t reported = 0U;
    for (std::uint64_t tick = 1U; tick <= 6U; ++tick) {
        const auto result = graph.run({tick}, 1.0 / 60.0, &jobs);
        assert(result);
        reported += result.value().systems_run;
    }
    assert(runs.load(std::memory_order_relaxed) == 3U);
    assert(reported == 3U);
}

void cadenceDoesNotWrapAtMaximumTick() {
    genomes::simulation::CadencePolicy policy{};
    policy.kind = genomes::simulation::CadenceKind::EveryNTicks;
    policy.period_ticks = 1U;
    genomes::simulation::CadenceState state{};
    const auto maximum = std::numeric_limits<std::uint64_t>::max();

    const auto first = genomes::simulation::evaluateCadence(
        policy, state, 7U, genomes::foundation::SimulationTick{maximum - 1U});
    assert(first.due);
    assert(state.next_due_tick.value == maximum);

    const auto second = genomes::simulation::evaluateCadence(
        policy, state, 7U, genomes::foundation::SimulationTick{maximum});
    assert(second.due);
    assert(state.next_due_tick.value == maximum);
}

} // namespace

int main() {
    mainThreadFailureDrainsAcceptedWorkerAndClearsCommands();
    reverseCompletionFailureDrainsEveryAcceptedHandle();
    directDependencyReleasesWithoutFrontierBarrier();
    legacyRunUsesDirectDependencyScheduling();
    executionPlanStartsWithoutWorkerBlocking();
    asynchronousExecutionPlanCommitsExactlyOnceAfterAllLeaves();
    cadenceNoOpReportsOnlyDueSystems();
    legacyGraphCadenceReportsOnlyDueSystems();
    cadenceDoesNotWrapAtMaximumTick();
    return 0;
}
