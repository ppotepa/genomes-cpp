#include <genomes/jobs/JobSystem.hpp>

#include <algorithm>
#include <chrono>
#include <utility>

namespace genomes::jobs {

thread_local JobSystem* JobSystem::current_worker_system_ = nullptr;
thread_local std::uint32_t JobSystem::current_worker_index_ = 0;

void JobFence::add(std::uint32_t count) noexcept {
    pending_.fetch_add(count, std::memory_order_release);
}

void JobFence::signal(std::uint32_t count) noexcept {
    const std::uint32_t previous = pending_.fetch_sub(count, std::memory_order_acq_rel);
    if (previous <= count) {
        pending_.store(0, std::memory_order_release);
        condition_.notify_all();
    }
}

void JobFence::wait() const noexcept {
    if (pending() == 0) {
        return;
    }
    std::unique_lock lock(mutex_);
    condition_.wait(lock, [this] { return pending() == 0; });
}

JobSystem::JobSystem(std::uint32_t worker_count, std::uint32_t reserved_main_threads) {
    if (worker_count == 0) {
        const std::uint32_t hardware = std::thread::hardware_concurrency();
        const std::uint32_t reserved = std::min(reserved_main_threads, hardware);
        worker_count = std::max<std::uint32_t>(1, hardware > reserved ? hardware - reserved : 1);
    }

    workers_.reserve(worker_count);
    for (std::uint32_t index = 0; index < worker_count; ++index) {
        workers_.emplace_back([this, index] { workerLoop(index); });
    }
}

JobSystem::~JobSystem() {
    shutdown(ShutdownMode::Drain);
}

JobHandle JobSystem::submit(JobFunction function) {
    auto state = std::make_shared<detail::JobState>();
    JobHandle handle(state);
    if (!function) {
        state->finish(false, {});
        return handle;
    }

    {
        std::lock_guard lock(queue_mutex_);
        if (!accepting_.load(std::memory_order_acquire)) {
            state->finish(true, {});
            return handle;
        }
        queue_.push_back({std::move(function), std::move(state)});
    }
    queue_condition_.notify_one();
    return handle;
}

void JobSystem::wait(const JobHandle& handle) const noexcept {
    if (!handle.valid() || handle.isComplete()) {
        return;
    }

    if (!isCurrentWorker()) {
        handle.wait();
        return;
    }

    while (!handle.isComplete()) {
        auto* self = const_cast<JobSystem*>(this);
        if (!self->executeOne(current_worker_index_)) {
            std::this_thread::yield();
        }
    }
}

void JobSystem::shutdown(ShutdownMode mode) noexcept {
    bool expected = false;
    if (!stopping_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    accepting_.store(false, std::memory_order_release);
    if (mode == ShutdownMode::CancelPending) {
        cancellation_requested_.store(true, std::memory_order_release);
        std::deque<JobNode> canceled;
        {
            std::lock_guard lock(queue_mutex_);
            canceled.swap(queue_);
        }
        for (JobNode& node : canceled) {
            node.state->finish(true, {});
        }
    }

    queue_condition_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

bool JobSystem::executeOne(std::uint32_t worker_index) noexcept {
    JobNode node;
    {
        std::lock_guard lock(queue_mutex_);
        if (queue_.empty()) {
            return false;
        }
        node = std::move(queue_.front());
        queue_.pop_front();
    }
    execute(std::move(node), worker_index);
    return true;
}

void JobSystem::workerLoop(std::uint32_t worker_index) noexcept {
    current_worker_system_ = this;
    current_worker_index_ = worker_index;
    for (;;) {
        {
            std::unique_lock lock(queue_mutex_);
            queue_condition_.wait(lock, [this] {
                return stopping_.load(std::memory_order_acquire) || !queue_.empty();
            });
            if (queue_.empty() && stopping_.load(std::memory_order_acquire)) {
                break;
            }
        }
        (void)executeOne(worker_index);
    }
    current_worker_system_ = nullptr;
}

void JobSystem::execute(JobNode node, std::uint32_t worker_index) noexcept {
    try {
        JobContext context(*this, worker_index);
        node.function(context);
        node.state->finish(false, {});
    } catch (...) {
        node.state->finish(false, std::current_exception());
    }
}

bool JobSystem::isCurrentWorker() const noexcept {
    return current_worker_system_ == this;
}

bool JobContext::isCancellationRequested() const noexcept {
    return system_.isCancellationRequested();
}

} // namespace genomes::jobs
