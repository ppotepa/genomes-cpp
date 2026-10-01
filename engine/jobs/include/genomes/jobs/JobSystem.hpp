#pragma once

#include <genomes/jobs/JobContext.hpp>
#include <genomes/jobs/JobHandle.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

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
    // A fence is armed once. Construct a new instance for each batch rather
    // than reusing a completed fence for unrelated work.
    explicit JobFence(std::uint32_t expected_count = 0) noexcept : pending_(expected_count) {}

    // Returns false without changing pending work when the request would
    // over-signal the fence.
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

    explicit JobSystem(std::uint32_t worker_count = 0,
                       std::uint32_t reserved_main_threads = 1,
                       WorkerLauncher worker_launcher = {});
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    [[nodiscard]] JobHandle submit(JobFunction function);
    void wait(const JobHandle& handle) const noexcept;
    void shutdown(ShutdownMode mode = ShutdownMode::Drain) noexcept;

    [[nodiscard]] std::uint32_t workerCount() const noexcept {
        return static_cast<std::uint32_t>(workers_.size());
    }

    [[nodiscard]] JobSystemState state() const noexcept;
    [[nodiscard]] bool isCancellationRequested() const noexcept;

private:
    struct JobNode final {
        JobFunction function;
        std::shared_ptr<detail::JobState> state;
    };

    static thread_local JobSystem* current_worker_system_;
    static thread_local std::uint32_t current_worker_index_;

    [[nodiscard]] bool executeOne(std::uint32_t worker_index) noexcept;
    void workerLoop(std::uint32_t worker_index) noexcept;
    void execute(JobNode node, std::uint32_t worker_index) noexcept;
    [[nodiscard]] bool isCurrentWorker() const noexcept;

    mutable std::mutex queue_mutex_;
    std::condition_variable queue_condition_;
    std::deque<JobNode> queue_;
    std::vector<std::thread> workers_;
    std::thread::id owner_thread_;
    JobSystemState state_{JobSystemState::Running};
};

} // namespace genomes::jobs
