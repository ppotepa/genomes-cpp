#include <genomes/jobs/JobSystem.hpp>

#include <atomic>
#include <cassert>
#include <functional>
#include <latch>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {

void drainRunsEveryAcceptedJobAndStopsSubmission() {
    genomes::jobs::JobSystem jobs(1);
    std::atomic<unsigned int> completed{0};
    std::vector<genomes::jobs::JobHandle> handles;
    for (unsigned int index = 0; index < 8; ++index) {
        handles.push_back(jobs.submit([&](genomes::jobs::JobContext&) {
            completed.fetch_add(1, std::memory_order_relaxed);
        }));
    }

    jobs.shutdown(genomes::jobs::ShutdownMode::Drain);
    for (const auto& handle : handles) {
        assert(handle.isComplete());
        assert(!handle.wasCanceled());
    }
    assert(completed.load(std::memory_order_relaxed) == handles.size());
    assert(jobs.state() == genomes::jobs::JobSystemState::Stopped);
    jobs.shutdown(genomes::jobs::ShutdownMode::CancelPending);
    assert(jobs.state() == genomes::jobs::JobSystemState::Stopped);
    const auto rejected = jobs.submit([](genomes::jobs::JobContext&) {});
    assert(rejected.isComplete());
    assert(rejected.wasCanceled());
}

void cancelPendingLeavesActiveWorkToCloseItsOwnScope() {
    genomes::jobs::JobSystem jobs(1);
    std::latch active_started{1};
    std::atomic<unsigned int> active_completed{0};
    const auto active = jobs.submit([&](genomes::jobs::JobContext& context) {
        active_started.count_down();
        while (!context.isCancellationRequested()) {
            std::this_thread::yield();
        }
        active_completed.fetch_add(1, std::memory_order_relaxed);
    });
    active_started.wait();
    const auto pending = jobs.submit([](genomes::jobs::JobContext&) {
        assert(false && "CancelPending must not execute queued work");
    });

    jobs.shutdown(genomes::jobs::ShutdownMode::CancelPending);
    assert(active.isComplete());
    assert(!active.wasCanceled());
    assert(pending.isComplete());
    assert(pending.wasCanceled());
    assert(active_completed.load(std::memory_order_relaxed) == 1);
    assert(jobs.state() == genomes::jobs::JobSystemState::Stopped);
}

void autoWorkersAreNeverInlineAndLauncherFailureRollsBack() {
    genomes::jobs::JobSystem automatic(0);
    assert(automatic.workerCount() >= 1);
    automatic.shutdown();

    std::atomic<unsigned int> exited{0};
    unsigned int launches = 0;
    bool threw = false;
    try {
        genomes::jobs::JobSystem failing(
            4,
            1,
            [&](std::function<void()> entry) -> std::thread {
                if (launches == 2) {
                    throw std::runtime_error("expected launcher failure");
                }
                ++launches;
                return std::thread([entry = std::move(entry), &exited] {
                    entry();
                    exited.fetch_add(1, std::memory_order_relaxed);
                });
            });
        (void)failing;
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
    assert(launches == 2);
    assert(exited.load(std::memory_order_relaxed) == 2);
}

} // namespace

int main() {
    drainRunsEveryAcceptedJobAndStopsSubmission();
    cancelPendingLeavesActiveWorkToCloseItsOwnScope();
    autoWorkersAreNeverInlineAndLauncherFailureRollsBack();
    return 0;
}
