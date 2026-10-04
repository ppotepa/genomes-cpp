#include <genomes/jobs/JobGraph.hpp>
#include <genomes/jobs/ParallelFor.hpp>
#include <genomes/jobs/ScratchContext.hpp>
#include <genomes/jobs/JobSystem.hpp>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <latch>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

void serialExecutorAndScratchAreStable() {
    genomes::jobs::SchedulerConfig config;
    config.mode = genomes::jobs::SchedulerMode::Serial;
    genomes::jobs::JobSystem jobs(config);
    assert(jobs.workerCount() == 0);
    assert(jobs.mode() == genomes::jobs::SchedulerMode::Serial);

    void* first_address = nullptr;
    const auto first = jobs.submit([&](genomes::jobs::JobContext& context) {
        first_address = context.scratch().allocate(128, 64);
        assert(context.scratch().bytesUsed() >= 128);
    });
    const auto second = jobs.submit([&](genomes::jobs::JobContext& context) {
        assert(context.scratch().bytesUsed() == 0);
        assert(context.scratch().allocate(128, 64) == first_address);
    });
    assert(first.isComplete());
    assert(second.isComplete());
    bool serial_io_ran = false;
    const auto io = jobs.submit(
        [&](genomes::jobs::JobContext& context) {
            assert(context.lane() == genomes::jobs::ExecutionLane::IO);
            serial_io_ran = true;
        },
        {.lane = genomes::jobs::ExecutionLane::IO,
         .work_class = genomes::jobs::WorkClass::BlockingIO});
    assert(io.isComplete());
    assert(serial_io_ran);
    const auto telemetry = jobs.telemetry();
    assert(telemetry.submitted == 3);
    assert(telemetry.completed == 3);
}

void topologyPolicyReservesApplicationSlots() {
    assert(genomes::jobs::topologyWorkerCount(0) == 0U);
    assert(genomes::jobs::topologyWorkerCount(1) == 0U);
    assert(genomes::jobs::topologyWorkerCount(2) == 1U);
    assert(genomes::jobs::topologyWorkerCount(8) == 6U);
    assert(genomes::jobs::topologyWorkerCount(8, 8) == 0U);
}

void explicitParallelSchedulerExecutesWork() {
    genomes::jobs::JobSystem scheduler(genomes::jobs::SchedulerConfig{});
    assert(scheduler.mode() == genomes::jobs::SchedulerMode::Parallel);
    std::atomic_bool ran{false};
    const auto handle = scheduler.submit([&](genomes::jobs::JobContext&) {
        ran.store(true, std::memory_order_release);
    });
    handle.wait();
    assert(ran.load(std::memory_order_acquire));
}

void explicitSerialExecutorHasNoWorkers() {
    genomes::jobs::SchedulerConfig config;
    config.mode = genomes::jobs::SchedulerMode::Serial;
    config.worker_count = 0U;
    config.enable_io_worker = false;
    genomes::jobs::JobSystem scheduler(config);
    assert(scheduler.mode() == genomes::jobs::SchedulerMode::Serial);
    assert(scheduler.workerCount() == 0U);
    bool ran = false;
    const auto handle = scheduler.submit([&](genomes::jobs::JobContext&) { ran = true; });
    assert(handle.isComplete());
    assert(ran);
}

void ioLaneUsesCentralScheduler() {
    genomes::jobs::SchedulerConfig config;
    config.worker_count = 1U;
    config.enable_io_worker = true;
    genomes::jobs::JobSystem jobs(config);
    genomes::jobs::JobGroup group(jobs);
    std::atomic_bool ran{false};
    (void)group.submit(
        [&](genomes::jobs::JobContext& context) {
            assert(context.lane() == genomes::jobs::ExecutionLane::IO);
            ran.store(true, std::memory_order_release);
        },
        {.lane = genomes::jobs::ExecutionLane::IO,
         .work_class = genomes::jobs::WorkClass::BlockingIO});
    group.wait();
    assert(ran.load(std::memory_order_acquire));
}

void groupSelectsFailureByJobId() {
    genomes::jobs::JobSystem jobs(2);
    genomes::jobs::JobGroup group(jobs);
    std::latch later_failed{1};
    const auto lower_id = group.submit([&](genomes::jobs::JobContext&) {
        later_failed.wait();
        throw std::runtime_error("lower id");
    });
    const auto higher_id = group.submit([&](genomes::jobs::JobContext&) {
        later_failed.count_down();
        throw std::runtime_error("higher id");
    });
    group.wait();
    assert(group.failed());
    assert(group.firstFailureId() == lower_id.id());
    assert(lower_id.id() < higher_id.id());
}

void cancellationReachesActiveAndPendingJobs() {
    genomes::jobs::JobSystem jobs(1);
    genomes::jobs::JobGroup group(jobs);
    std::latch active{1};
    std::atomic_bool observed{false};
    const auto running = group.submit([&](genomes::jobs::JobContext& context) {
        active.count_down();
        while (!context.isCancellationRequested()) {
            std::this_thread::yield();
        }
        observed.store(true, std::memory_order_release);
    });
    active.wait();
    const auto pending = group.submit([](genomes::jobs::JobContext&) {
        assert(false && "canceled group work must not start");
    });
    group.cancel();
    group.wait();
    assert(observed.load(std::memory_order_acquire));
    assert(running.isComplete());
    assert(!running.wasCanceled());
    assert(pending.wasCanceled());
}

void graphRunsFanInAndDeepContinuationsExactlyOnce() {
    genomes::jobs::JobSystem jobs(4);
    std::atomic_uint32_t sources{0};
    std::atomic_uint32_t fan_in{0};
    std::atomic_uint32_t tail{0};
    genomes::jobs::JobGraphBuilder builder;
    const auto first = builder.add([&](genomes::jobs::JobContext&) {
        sources.fetch_add(1, std::memory_order_relaxed);
    });
    const auto second = builder.add([&](genomes::jobs::JobContext&) {
        sources.fetch_add(1, std::memory_order_relaxed);
    });
    const auto join = builder.add([&](genomes::jobs::JobContext&) {
        assert(sources.load(std::memory_order_acquire) == 2);
        fan_in.fetch_add(1, std::memory_order_relaxed);
    });
    builder.precedes(first, join);
    builder.precedes(second, join);
    auto previous = join;
    for (std::size_t index = 0; index < 1024; ++index) {
        const auto next = builder.add([&](genomes::jobs::JobContext&) {
            tail.fetch_add(1, std::memory_order_relaxed);
        });
        builder.precedes(previous, next);
        previous = next;
    }
    auto graph = std::move(builder).build();
    auto group = graph.run(jobs);
    group.wait();
    assert(!group.failed());
    assert(fan_in.load(std::memory_order_relaxed) == 1);
    assert(tail.load(std::memory_order_relaxed) == 1024);
}

void graphCanStartFromWorkerAndScheduleContinuation() {
    genomes::jobs::JobSystem jobs(2);
    std::atomic_uint32_t leaves{0};
    std::atomic_uint32_t continuations{0};
    genomes::jobs::JobCompletion completion;

    genomes::jobs::JobGraphBuilder builder;
    const auto first = builder.add([&](genomes::jobs::JobContext&) {
        leaves.fetch_add(1, std::memory_order_relaxed);
    });
    const auto second = builder.add([&](genomes::jobs::JobContext&) {
        leaves.fetch_add(1, std::memory_order_relaxed);
    });
    const auto join = builder.add([&](genomes::jobs::JobContext&) {
        assert(leaves.load(std::memory_order_acquire) == 2U);
    });
    builder.precedes(first, join);
    builder.precedes(second, join);
    const auto graph = std::move(builder).build();

    const auto owner = jobs.submit([&](genomes::jobs::JobContext&) {
        completion = graph.start(jobs);
        completion.then([&](genomes::jobs::JobContext&) {
            continuations.fetch_add(1, std::memory_order_relaxed);
        });
    });
    owner.wait();
    completion.wait();

    assert(completion.isComplete());
    assert(!completion.failed());
    assert(leaves.load(std::memory_order_relaxed) == 2U);
    // The continuation is scheduled by the scheduler, not run inline by the
    // worker that observes the final graph node.
    while (continuations.load(std::memory_order_relaxed) == 0U) {
        std::this_thread::yield();
    }
    assert(continuations.load(std::memory_order_relaxed) == 1U);
}

void graphCancellationSuppressesPendingContinuation() {
    genomes::jobs::JobSystem jobs(2);
    std::latch root_started{1};
    std::latch release_root{1};
    std::atomic_uint32_t continuation_runs{0};
    genomes::jobs::JobGraphBuilder builder;
    const auto root = builder.add([&](genomes::jobs::JobContext&) {
        root_started.count_down();
        release_root.wait();
    });
    const auto continuation = builder.add([&](genomes::jobs::JobContext&) {
        continuation_runs.fetch_add(1, std::memory_order_relaxed);
    });
    builder.precedes(root, continuation);
    auto graph = std::move(builder).build();
    auto group = graph.run(jobs);
    root_started.wait();
    group.cancel();
    release_root.count_down();
    group.wait();
    assert(continuation_runs.load(std::memory_order_relaxed) == 0U);
}

void graphFailureSuppressesContinuation() {
    genomes::jobs::JobSystem jobs(2);
    std::atomic_uint32_t continuation_runs{0};
    genomes::jobs::JobGraphBuilder builder;
    const auto failing = builder.add([](genomes::jobs::JobContext&) {
        throw std::runtime_error("expected graph failure");
    });
    const auto continuation = builder.add([&](genomes::jobs::JobContext&) {
        continuation_runs.fetch_add(1, std::memory_order_relaxed);
    });
    builder.precedes(failing, continuation);
    auto group = std::move(builder).build().run(jobs);
    group.wait();
    assert(group.failed());
    assert(continuation_runs.load(std::memory_order_relaxed) == 0U);
}

void ownerPumpsAffinityLanesWithWeightedFairness() {
    genomes::jobs::JobSystem jobs(1);
    std::vector<genomes::jobs::JobPriority> order;
    genomes::jobs::JobGroup group(jobs);
    for (std::size_t index = 0; index < 20; ++index) {
        (void)group.submit([&](genomes::jobs::JobContext& context) {
            assert(context.lane() == genomes::jobs::ExecutionLane::Main);
            order.push_back(genomes::jobs::JobPriority::Critical);
        },
                           {.lane = genomes::jobs::ExecutionLane::Main,
                            .priority = genomes::jobs::JobPriority::Critical});
    }
    (void)group.submit([&](genomes::jobs::JobContext&) {
        order.push_back(genomes::jobs::JobPriority::Normal);
    },
                       {.lane = genomes::jobs::ExecutionLane::Main,
                        .priority = genomes::jobs::JobPriority::Normal});
    (void)group.submit([&](genomes::jobs::JobContext&) {
        order.push_back(genomes::jobs::JobPriority::Background);
    },
                       {.lane = genomes::jobs::ExecutionLane::Main,
                        .priority = genomes::jobs::JobPriority::Background});
    assert(order.empty());
    assert(jobs.pump(genomes::jobs::ExecutionLane::Main) == 22);
    group.wait();
    const auto background = std::find(order.begin(), order.end(),
                                      genomes::jobs::JobPriority::Background);
    assert(background != order.end());
    assert(static_cast<std::size_t>(background - order.begin()) < 20);
}

void schedulerCompletesOneHundredThousandJobs() {
    genomes::jobs::JobSystem jobs(4);
    genomes::jobs::JobGroup group(jobs);
    std::atomic_uint32_t completed{0};
    for (std::uint32_t index = 0; index < 100'000; ++index) {
        (void)group.submit([&](genomes::jobs::JobContext&) {
            completed.fetch_add(1, std::memory_order_relaxed);
        });
    }
    group.wait();
    assert(completed.load(std::memory_order_relaxed) == 100'000);
}

void parallelForReportsWorkerFailure() {
    genomes::jobs::JobSystem jobs(2);
    const bool completed = genomes::jobs::parallelForAndWait(
        jobs, 0U, 16U, 4U, [](const genomes::jobs::BatchRange&) {
            throw std::runtime_error("expected parallel-for failure");
        });
    assert(!completed);
}

void parallelForRejectsWorkerBarrier() {
    genomes::jobs::JobSystem jobs(2);
    std::atomic_bool rejected{false};
    const auto handle = jobs.submit([&](genomes::jobs::JobContext&) {
        assert(jobs.isWorkerThread());
        rejected.store(!genomes::jobs::parallelForAndWait(
                           jobs, 0U, 8U, 2U,
                           [](const genomes::jobs::BatchRange&) {}),
                       std::memory_order_release);
    });
    handle.wait();
    assert(rejected.load(std::memory_order_acquire));
}

void parallelForPreservesStableBatchRangesAcrossModes() {
    constexpr std::size_t begin = 3U;
    constexpr std::size_t end = 103U;
    constexpr std::size_t grain = 16U;
    constexpr std::size_t batch_count = (end - begin + grain - 1U) / grain;

    auto collect = [](genomes::jobs::JobSystem& jobs) {
        std::vector<genomes::jobs::BatchRange> observed(batch_count);
        std::atomic_uint32_t seen{0};
        const bool completed = genomes::jobs::parallelForAndWait(
            jobs, begin, end, grain, [&](const genomes::jobs::BatchRange& range) {
                assert(range.batch_index < batch_count);
                observed[range.batch_index] = range;
                seen.fetch_add(1U, std::memory_order_relaxed);
            });
        assert(completed);
        assert(seen.load(std::memory_order_relaxed) == batch_count);
        return observed;
    };

    genomes::jobs::SchedulerConfig serial_config;
    serial_config.mode = genomes::jobs::SchedulerMode::Serial;
    genomes::jobs::JobSystem serial(serial_config);
    genomes::jobs::JobSystem parallel(2U);
    const auto serial_ranges = collect(serial);
    const auto parallel_ranges = collect(parallel);
    assert(serial_ranges.size() == parallel_ranges.size());
    for (std::size_t batch = 0; batch < batch_count; ++batch) {
        const auto& range = serial_ranges[batch];
        const auto& parallel_range = parallel_ranges[batch];
        assert(range.begin == parallel_range.begin);
        assert(range.end == parallel_range.end);
        assert(range.batch_index == parallel_range.batch_index);
        const auto expected_begin = begin + batch * grain;
        const auto expected_end = std::min(end, expected_begin + grain);
        assert(range.begin == expected_begin);
        assert(range.end == expected_end);
        assert(range.batch_index == batch);
    }

    // The partition arithmetic must remain valid when the half-open range
    // ends at the largest representable index.
    const std::size_t extreme_begin = std::numeric_limits<std::size_t>::max() - 3U;
    std::vector<genomes::jobs::BatchRange> extreme_ranges(2U);
    assert(genomes::jobs::parallelForAndWait(
        serial, extreme_begin, std::numeric_limits<std::size_t>::max(), 2U,
        [&extreme_ranges](const genomes::jobs::BatchRange& range) {
            extreme_ranges[range.batch_index] = range;
        }));
    assert(extreme_ranges[0].begin == extreme_begin);
    assert(extreme_ranges[0].end == extreme_begin + 2U);
    assert(extreme_ranges[1].begin == extreme_begin + 2U);
    assert(extreme_ranges[1].end == std::numeric_limits<std::size_t>::max());
}

} // namespace

int main() {
    serialExecutorAndScratchAreStable();
    topologyPolicyReservesApplicationSlots();
    explicitParallelSchedulerExecutesWork();
    explicitSerialExecutorHasNoWorkers();
    ioLaneUsesCentralScheduler();
    groupSelectsFailureByJobId();
    cancellationReachesActiveAndPendingJobs();
    graphRunsFanInAndDeepContinuationsExactlyOnce();
    graphCanStartFromWorkerAndScheduleContinuation();
    graphCancellationSuppressesPendingContinuation();
    graphFailureSuppressesContinuation();
    ownerPumpsAffinityLanesWithWeightedFairness();
    schedulerCompletesOneHundredThousandJobs();
    parallelForReportsWorkerFailure();
    parallelForRejectsWorkerBarrier();
    parallelForPreservesStableBatchRangesAcrossModes();
    return 0;
}
