#include <genomes/jobs/JobSystem.hpp>

#include <algorithm>
#include <exception>
#include <utility>

namespace genomes::jobs {

thread_local JobSystem* JobSystem::current_worker_system_ = nullptr;
thread_local std::uint32_t JobSystem::current_worker_index_ = 0;

bool JobFence::signal(std::uint32_t count) noexcept {
    bool completed = false;
    {
        std::lock_guard lock(mutex_);
        if (count > pending_) {
            return false;
        }
        pending_ -= count;
        completed = pending_ == 0;
    }
    if (completed) {
        condition_.notify_all();
    }
    return true;
}

std::uint32_t JobFence::pending() const noexcept {
    std::lock_guard lock(mutex_);
    return pending_;
}

void JobFence::wait() const noexcept {
    std::unique_lock lock(mutex_);
    condition_.wait(lock, [this] { return pending_ == 0; });
}

JobSystem::JobSystem(std::uint32_t worker_count,
                     std::uint32_t reserved_main_threads,
                     WorkerLauncher worker_launcher)
    : owner_thread_(std::this_thread::get_id()) {
    if (worker_count == 0) {
        const std::uint32_t hardware = std::thread::hardware_concurrency();
        const std::uint32_t reserved = std::min(reserved_main_threads, hardware);
        worker_count = std::max<std::uint32_t>(1, hardware > reserved ? hardware - reserved : 1);
    }

    if (!worker_launcher) {
        worker_launcher = [](std::function<void()> entry) {
            return std::thread(std::move(entry));
        };
    }

    workers_.reserve(worker_count);
    try {
        for (std::uint32_t index = 0; index < worker_count; ++index) {
            workers_.push_back(worker_launcher([this, index] { workerLoop(index); }));
        }
    } catch (...) {
        {
            std::lock_guard lock(queue_mutex_);
            state_ = JobSystemState::ClosingCancel;
        }
        queue_condition_.notify_all();
        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
        {
            std::lock_guard lock(queue_mutex_);
            state_ = JobSystemState::Stopped;
        }
        throw;
    }
}

JobSystem::~JobSystem() {
    if (isCurrentWorker()) {
        std::terminate();
    }
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
        if (state_ != JobSystemState::Running) {
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
    if (isCurrentWorker() || std::this_thread::get_id() != owner_thread_) {
        std::terminate();
    }

    std::deque<JobNode> canceled;
    {
        std::lock_guard lock(queue_mutex_);
        if (state_ != JobSystemState::Running) {
            return;
        }
        state_ = mode == ShutdownMode::Drain ? JobSystemState::ClosingDrain
                                             : JobSystemState::ClosingCancel;
        if (state_ == JobSystemState::ClosingCancel) {
            canceled.swap(queue_);
        }
    }
    for (JobNode& node : canceled) {
        node.state->finish(true, {});
    }

    queue_condition_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
    {
        std::lock_guard lock(queue_mutex_);
        state_ = JobSystemState::Stopped;
    }
}

bool JobSystem::executeOne(std::uint32_t worker_index) noexcept {
    JobNode node;
    {
        std::lock_guard lock(queue_mutex_);
        if (state_ == JobSystemState::ClosingCancel || queue_.empty()) {
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
                return state_ != JobSystemState::Running || !queue_.empty();
            });
            if (queue_.empty() && state_ != JobSystemState::Running) {
                break;
            }
            if (state_ == JobSystemState::ClosingCancel) {
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

JobSystemState JobSystem::state() const noexcept {
    std::lock_guard lock(queue_mutex_);
    return state_;
}

bool JobSystem::isCancellationRequested() const noexcept {
    return state() == JobSystemState::ClosingCancel;
}

bool JobContext::isCancellationRequested() const noexcept {
    return system_.isCancellationRequested();
}

} // namespace genomes::jobs
