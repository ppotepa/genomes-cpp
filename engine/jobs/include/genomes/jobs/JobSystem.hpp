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

class JobFence final {
public:
    void add(std::uint32_t count = 1) noexcept;
    void signal(std::uint32_t count = 1) noexcept;
    void wait() const noexcept;

    [[nodiscard]] std::uint32_t pending() const noexcept {
        return pending_.load(std::memory_order_acquire);
    }

private:
    std::atomic<std::uint32_t> pending_{0};
    mutable std::mutex mutex_;
    mutable std::condition_variable condition_;
};

class JobSystem final {
public:
    using JobFunction = std::function<void(JobContext&)>;

    explicit JobSystem(std::uint32_t worker_count = 0,
                       std::uint32_t reserved_main_threads = 1);
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    [[nodiscard]] JobHandle submit(JobFunction function);
    void wait(const JobHandle& handle) const noexcept;
    void shutdown(ShutdownMode mode = ShutdownMode::Drain) noexcept;

    [[nodiscard]] std::uint32_t workerCount() const noexcept {
        return static_cast<std::uint32_t>(workers_.size());
    }

    [[nodiscard]] bool isCancellationRequested() const noexcept {
        return cancellation_requested_.load(std::memory_order_acquire);
    }

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
    std::atomic<bool> stopping_{false};
    std::atomic<bool> accepting_{true};
    std::atomic<bool> cancellation_requested_{false};
};

} // namespace genomes::jobs
