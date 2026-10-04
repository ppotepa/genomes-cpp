#pragma once

#include <genomes/jobs/JobContext.hpp>
#include <genomes/jobs/JobGroup.hpp>
#include <genomes/jobs/JobHandle.hpp>
#include <genomes/jobs/SchedulerTypes.hpp>

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace genomes::jobs {

enum class ShutdownMode {
    Drain,
    CancelPending
};

enum class JobSystemState : std::uint8_t {
    Running,
    ClosingDrain,
    ClosingCancel,
    Stopped,
};

class JobFence final {
public:
    explicit JobFence(std::uint32_t expected_count = 0) noexcept : pending_(expected_count) {}

    [[nodiscard]] bool signal(std::uint32_t count = 1) noexcept;
    void wait() const noexcept;
    [[nodiscard]] std::uint32_t pending() const noexcept;

private:
    std::uint32_t pending_{0};
    mutable std::mutex mutex_;
    mutable std::condition_variable condition_;
};

class JobSystem final {
public:
    using JobFunction = std::function<void(JobContext&)>;
    using WorkerLauncher = std::function<std::thread(std::function<void()>)>;

    explicit JobSystem(SchedulerConfig config, WorkerLauncher worker_launcher = {});
    explicit JobSystem(std::uint32_t worker_count = 0,
                       std::uint32_t reserved_main_threads = 2,
                       WorkerLauncher worker_launcher = {});
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    [[nodiscard]] JobHandle submit(JobFunction function, JobOptions options = {});
    [[nodiscard]] JobHandle submit(JobGroup& group,
                                   JobFunction function,
                                   JobOptions options = {});
    void wait(const JobHandle& handle) const noexcept;
    void wait(const JobCompletion& completion) const noexcept;
    void shutdown(ShutdownMode mode = ShutdownMode::Drain) noexcept;

    [[nodiscard]] std::size_t pump(
        ExecutionLane lane,
        std::size_t maximum_jobs = static_cast<std::size_t>(-1));

    [[nodiscard]] std::uint32_t workerCount() const noexcept;
    // Worker waits are cooperative: while waiting for scheduler-owned
    // work they help execute runnable worker tasks. Prefer JobGraph
    // continuations for large pipelines, but bounded nested bulk operations
    // remain deadlock-safe even with a single worker.
    [[nodiscard]] bool isWorkerThread() const noexcept;
    [[nodiscard]] SchedulerMode mode() const noexcept;
    [[nodiscard]] JobSystemState state() const noexcept;
    [[nodiscard]] bool isCancellationRequested() const noexcept;
    [[nodiscard]] SchedulerTelemetry telemetry() const noexcept;

private:
    friend class JobGroup;
    friend class JobGraph;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace genomes::jobs
