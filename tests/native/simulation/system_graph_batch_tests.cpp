#include <genomes/jobs/JobSystem.hpp>
#include <genomes/simulation/SystemGraph.hpp>

#include <atomic>
#include <cassert>
#include <latch>
#include <stdexcept>
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
        worker_started.wait();
        allow_worker_finish.count_down();
        throw std::runtime_error("expected callback failure");
    })));
    SystemDescriptor successor = descriptor(3, true, [&](genomes::simulation::SystemContext&) {
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
    genomes::jobs::JobSystem jobs(2);
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

    const auto result = graph.run({}, 1.0 / 60.0, &jobs);
    assert(!result);
    assert(completed.load(std::memory_order_relaxed) == 2);
    assert(successor_runs.load(std::memory_order_relaxed) == 0);
}

} // namespace

int main() {
    mainThreadFailureDrainsAcceptedWorkerAndClearsCommands();
    reverseCompletionFailureDrainsEveryAcceptedHandle();
    return 0;
}
