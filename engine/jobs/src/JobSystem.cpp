#include <genomes/jobs/JobSystem.hpp>

#include <genomes/jobs/JobGraph.hpp>
#include <genomes/jobs/ScratchContext.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <exception>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace genomes::jobs {

namespace detail {

struct JobGroupState final {
    explicit JobGroupState(JobSystem& owner) noexcept : system(&owner) {}

    void add(std::size_t count = 1) noexcept {
        std::lock_guard lock(mutex);
        pending += count;
    }

    void finish(JobId id, std::exception_ptr failure) noexcept {
        bool notify = false;
        std::vector<std::function<void()>> continuations;
        {
            std::lock_guard lock(mutex);
            if (failure && id < first_failure_id) {
                first_failure_id = id;
                first_failure = failure;
            }
            if (pending > 0) {
                --pending;
            }
            notify = pending == 0;
            if (notify) {
                continuations.swap(on_complete);
            }
        }
        if (notify) {
            condition.notify_all();
            for (auto& continuation : continuations) {
                continuation();
            }
        }
    }

    void addContinuation(std::function<void()> continuation) {
        bool complete = false;
        {
            std::lock_guard lock(mutex);
            if (pending == 0) {
                complete = true;
            } else {
                on_complete.push_back(std::move(continuation));
            }
        }
        if (complete) {
            continuation();
        }
    }

    JobSystem* system{nullptr};
    CancelSource cancellation;
    mutable std::mutex mutex;
    mutable std::condition_variable condition;
    std::size_t pending{0};
    std::vector<std::function<void()>> on_complete;
    JobId first_failure_id{std::numeric_limits<JobId>::max()};
    std::exception_ptr first_failure;
};

} // namespace detail

namespace {

constexpr std::uint32_t no_worker = std::numeric_limits<std::uint32_t>::max();
constexpr std::array<JobPriority, 13> priority_schedule{
    JobPriority::Critical, JobPriority::Critical, JobPriority::Critical,
    JobPriority::Critical, JobPriority::Critical, JobPriority::Critical,
    JobPriority::Critical, JobPriority::Critical, JobPriority::Normal,
    JobPriority::Normal, JobPriority::Normal, JobPriority::Normal,
    JobPriority::Background};

[[nodiscard]] constexpr std::size_t priorityIndex(JobPriority priority) noexcept {
    return static_cast<std::size_t>(priority);
}

[[nodiscard]] constexpr std::size_t laneIndex(ExecutionLane lane) noexcept {
    return static_cast<std::size_t>(lane);
}

[[nodiscard]] SchedulerConfig compatibilityConfig(std::uint32_t worker_count,
                                                  std::uint32_t reserved_threads) noexcept {
    SchedulerConfig config;
    config.worker_count = worker_count;
    config.reserved_threads = reserved_threads;
    return config;
}

[[nodiscard]] std::uint32_t automaticWorkerCount(const SchedulerConfig& config) noexcept {
    const std::uint32_t hardware = std::thread::hardware_concurrency();
    return topologyWorkerCount(hardware, std::min(config.reserved_threads, hardware));
}

[[noreturn]] void terminateContract(const char* diagnostic) noexcept {
    std::fputs("genomes.jobs.contract_violation: ", stderr);
    std::fputs(diagnostic, stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
    std::_Exit(EXIT_FAILURE);
}

} // namespace

struct JobSystem::Impl final {
    struct JobNode final {
        JobFunction function;
        JobOptions options;
        std::shared_ptr<detail::JobState> state;
        std::shared_ptr<detail::JobGroupState> group;
        std::function<void()> continuation;
        JobId id{0};
        bool tracked{false};
        bool started{false};
    };

    struct QueueSet final {
        std::mutex mutex;
        std::array<std::deque<JobNode>, 3> queues;
        std::size_t schedule_cursor{0};
    };

    struct WorkerData final {
        explicit WorkerData(std::size_t scratch_capacity) : scratch(scratch_capacity) {}
        QueueSet local;
        ScratchContext scratch;
        std::size_t local_since_external{0};
    };

    struct Counters final {
        std::atomic_uint64_t submitted{0};
        std::atomic_uint64_t started{0};
        std::atomic_uint64_t completed{0};
        std::atomic_uint64_t canceled{0};
        std::atomic_uint64_t failed{0};
        std::atomic_uint64_t stolen{0};
        std::atomic_uint64_t injection_pops{0};
        std::array<std::atomic_uint64_t, 6> submitted_by_class{};
        std::array<std::atomic_uint64_t, 6> running_by_class{};
        std::array<std::atomic_uint64_t, 6> completed_by_class{};
        std::array<std::atomic_uint64_t, 6> canceled_by_class{};
    };

    explicit Impl(JobSystem& owner, SchedulerConfig requested, WorkerLauncher launcher)
        : system(owner), config(requested), owner_thread(std::this_thread::get_id()),
          serial_scratch(config.scratch_capacity), io_scratch(config.scratch_capacity) {
        if (config.mode == SchedulerMode::Serial) {
            config.worker_count = 0;
            config.enable_io_worker = false;
            return;
        }
        if (config.worker_count == 0) {
            config.worker_count = automaticWorkerCount(config);
        }
        if (config.worker_count == 0) {
            config.mode = SchedulerMode::Serial;
            config.enable_io_worker = false;
            return;
        }
        if (!launcher) {
            launcher = [](std::function<void()> entry) { return std::thread(std::move(entry)); };
        }

        worker_data.reserve(config.worker_count);
        workers.reserve(config.worker_count);
        for (std::uint32_t index = 0; index < config.worker_count; ++index) {
            worker_data.push_back(std::make_unique<WorkerData>(config.scratch_capacity));
        }
        try {
            for (std::uint32_t index = 0; index < config.worker_count; ++index) {
                workers.push_back(launcher([this, index] { workerLoop(index); }));
            }
            if (config.enable_io_worker) {
                io_worker = launcher([this] { ioLoop(); });
            }
        } catch (...) {
            state.store(JobSystemState::ClosingCancel, std::memory_order_release);
            wake.notify_all();
            for (std::thread& worker : workers) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
            if (io_worker.joinable()) {
                io_worker.join();
            }
            state.store(JobSystemState::Stopped, std::memory_order_release);
            throw;
        }
    }

    [[nodiscard]] JobId reserveIds(std::size_t count) noexcept {
        return next_id.fetch_add(static_cast<JobId>(count), std::memory_order_relaxed);
    }

    [[nodiscard]] JobHandle submit(JobFunction function,
                                   JobOptions options,
                                   std::shared_ptr<detail::JobGroupState> group = {},
                                   bool register_group = false,
                                   std::function<void()> continuation = {},
                                   std::optional<JobId> forced_id = std::nullopt) {
        const JobId id = forced_id.value_or(reserveIds(1));
        auto job_state = std::make_shared<detail::JobState>();
        JobHandle handle(job_state, id, options.lane);
        if (register_group && group) {
            group->add();
        }

        JobNode node{std::move(function), options, std::move(job_state), std::move(group),
                     std::move(continuation), id, false};
        if (!node.function) {
            finish(std::move(node), false, {});
            return handle;
        }
        if (state.load(std::memory_order_acquire) != JobSystemState::Running ||
            node.options.cancellation.isCancellationRequested() ||
            (node.group && node.group->cancellation.isCancellationRequested())) {
            finish(std::move(node), true, {});
            return handle;
        }

        counters.submitted.fetch_add(1, std::memory_order_relaxed);
        counters.submitted_by_class[static_cast<std::size_t>(options.work_class)]
            .fetch_add(1, std::memory_order_relaxed);
        outstanding.fetch_add(1, std::memory_order_acq_rel);
        node.tracked = true;
        if (config.mode == SchedulerMode::Serial &&
            options.lane != ExecutionLane::Main && options.lane != ExecutionLane::Render) {
            execute(std::move(node), serial_scratch, no_worker, options.lane);
            return handle;
        }

        push(std::move(node));
        return handle;
    }

    void push(JobNode node) {
        const ExecutionLane lane = node.options.lane;
        QueueSet* destination = nullptr;
        if (node.options.lane == ExecutionLane::Worker && tls_system == this &&
            tls_lane == ExecutionLane::Worker && tls_worker_index < worker_data.size()) {
            destination = &worker_data[tls_worker_index]->local;
        } else {
            switch (node.options.lane) {
            case ExecutionLane::Worker: destination = &worker_injection; break;
            case ExecutionLane::IO: destination = &io_queue; break;
            case ExecutionLane::Main: destination = &main_queue; break;
            case ExecutionLane::Render: destination = &render_queue; break;
            }
        }
        // The wake predicate is inspected while holding wake_mutex. Publish
        // both the queue entry and its count while holding that mutex, so a
        // worker cannot observe an empty scheduler just before this signal
        // and sleep past it.
        {
            std::lock_guard wake_lock(wake_mutex);
            std::lock_guard queue_lock(destination->mutex);
            queued.fetch_add(1, std::memory_order_release);
            queued_by_lane[laneIndex(lane)].fetch_add(1, std::memory_order_release);
            destination->queues[priorityIndex(node.options.priority)].push_back(std::move(node));
        }
        wake.notify_all();
    }

    [[nodiscard]] bool popWeighted(QueueSet& queues,
                                   ExecutionLane lane,
                                   JobNode& output,
                                   bool from_back = false) {
        std::lock_guard lock(queues.mutex);
        for (std::size_t attempt = 0; attempt < priority_schedule.size(); ++attempt) {
            const JobPriority priority = priority_schedule[queues.schedule_cursor];
            queues.schedule_cursor = (queues.schedule_cursor + 1U) % priority_schedule.size();
            auto& queue = queues.queues[priorityIndex(priority)];
            if (!queue.empty()) {
                if (from_back) {
                    output = std::move(queue.back());
                    queue.pop_back();
                } else {
                    output = std::move(queue.front());
                    queue.pop_front();
                }
                queued.fetch_sub(1, std::memory_order_acq_rel);
                queued_by_lane[laneIndex(lane)].fetch_sub(1, std::memory_order_acq_rel);
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool takeWorkerJob(std::uint32_t index, JobNode& node) {
        WorkerData& worker = *worker_data[index];
        if (worker.local_since_external >= 32U) {
            worker.local_since_external = 0;
            if (popWeighted(worker_injection, ExecutionLane::Worker, node)) {
                counters.injection_pops.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
            if (steal(index, node)) {
                return true;
            }
        }
        if (popWeighted(worker.local, ExecutionLane::Worker, node)) {
            ++worker.local_since_external;
            return true;
        }
        worker.local_since_external = 0;
        if (popWeighted(worker_injection, ExecutionLane::Worker, node)) {
            counters.injection_pops.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        return steal(index, node);
    }

    [[nodiscard]] bool steal(std::uint32_t thief, JobNode& node) {
        for (std::size_t offset = 1; offset < worker_data.size(); ++offset) {
            const std::size_t victim = (static_cast<std::size_t>(thief) + offset) % worker_data.size();
            if (popWeighted(worker_data[victim]->local, ExecutionLane::Worker, node, true)) {
                counters.stolen.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool executeOne(std::uint32_t worker_index) noexcept {
        JobNode node;
        if (!takeWorkerJob(worker_index, node)) {
            return false;
        }
        execute(std::move(node), worker_data[worker_index]->scratch, worker_index,
                ExecutionLane::Worker);
        return true;
    }

    void workerLoop(std::uint32_t worker_index) noexcept {
        tls_system = this;
        tls_worker_index = worker_index;
        tls_lane = ExecutionLane::Worker;
        for (;;) {
            if (executeOne(worker_index)) {
                continue;
            }
            const JobSystemState current = state.load(std::memory_order_acquire);
            if (current == JobSystemState::ClosingCancel || current == JobSystemState::Stopped ||
                (current == JobSystemState::ClosingDrain &&
                 outstanding.load(std::memory_order_acquire) == 0)) {
                break;
            }
            std::unique_lock lock(wake_mutex);
            wake.wait(lock, [this] {
                return queued_by_lane[laneIndex(ExecutionLane::Worker)]
                           .load(std::memory_order_acquire) > 0 ||
                       state.load(std::memory_order_acquire) != JobSystemState::Running;
            });
        }
        clearTls();
    }

    void ioLoop() noexcept {
        tls_system = this;
        tls_worker_index = no_worker;
        tls_lane = ExecutionLane::IO;
        for (;;) {
            JobNode node;
            if (popWeighted(io_queue, ExecutionLane::IO, node)) {
                execute(std::move(node), io_scratch, no_worker, ExecutionLane::IO);
                continue;
            }
            const JobSystemState current = state.load(std::memory_order_acquire);
            if (current == JobSystemState::ClosingCancel || current == JobSystemState::Stopped ||
                (current == JobSystemState::ClosingDrain &&
                 outstanding.load(std::memory_order_acquire) == 0)) {
                break;
            }
            std::unique_lock lock(wake_mutex);
            wake.wait(lock, [this] {
                return queued_by_lane[laneIndex(ExecutionLane::IO)]
                           .load(std::memory_order_acquire) > 0 ||
                       state.load(std::memory_order_acquire) != JobSystemState::Running;
            });
        }
        clearTls();
    }

    void execute(JobNode node,
                 ScratchContext& scratch,
                 std::uint32_t worker_index,
                 ExecutionLane lane) noexcept {
        const bool canceled = state.load(std::memory_order_acquire) ==
                                  JobSystemState::ClosingCancel ||
                              node.options.cancellation.isCancellationRequested() ||
                              (node.group &&
                               node.group->cancellation.isCancellationRequested());
        if (canceled) {
            finish(std::move(node), true, {});
            return;
        }

        counters.started.fetch_add(1, std::memory_order_relaxed);
        node.started = true;
        counters.running_by_class[static_cast<std::size_t>(node.options.work_class)]
            .fetch_add(1, std::memory_order_relaxed);
        std::exception_ptr failure;
        scratch.reset();
        const auto previous_group = tls_group;
        tls_group = node.group.get();
        try {
            JobContext context(system, scratch, worker_index, lane, node.options.work_class,
                               node.id, node.options.cancellation,
                               node.group ? node.group->cancellation.token() : CancelToken{});
            node.function(context);
        } catch (...) {
            failure = std::current_exception();
        }
        tls_group = previous_group;
        scratch.reset();
        finish(std::move(node), false, failure);
    }

    void finish(JobNode node, bool canceled, std::exception_ptr failure) noexcept {
        node.state->finish(canceled, failure);
        if (canceled) {
            counters.canceled.fetch_add(1, std::memory_order_relaxed);
            counters.canceled_by_class[static_cast<std::size_t>(node.options.work_class)]
                .fetch_add(1, std::memory_order_relaxed);
        } else if (failure) {
            counters.failed.fetch_add(1, std::memory_order_relaxed);
        } else {
            counters.completed.fetch_add(1, std::memory_order_relaxed);
            counters.completed_by_class[static_cast<std::size_t>(node.options.work_class)]
                .fetch_add(1, std::memory_order_relaxed);
        }
        if (node.started) {
            counters.running_by_class[static_cast<std::size_t>(node.options.work_class)]
                .fetch_sub(1, std::memory_order_relaxed);
        }
        if (node.continuation) {
            node.continuation();
        }
        if (node.tracked) {
            outstanding.fetch_sub(1, std::memory_order_acq_rel);
        }
        // A completion can let its owner immediately tear down the scheduler.
        // Publish it only after this node is no longer counted as outstanding,
        // otherwise shutdown can observe a completed graph with a permanently
        // retained task count while its workers are being joined.
        if (node.group) {
            node.group->finish(node.id, failure);
        }
        wake.notify_all();
    }

    [[nodiscard]] std::size_t pump(ExecutionLane lane, std::size_t maximum_jobs) {
        if (lane != ExecutionLane::Main && lane != ExecutionLane::Render) {
            return 0;
        }
        if (std::this_thread::get_id() != owner_thread) {
            terminateContract("affinity lane pumped from a non-owner thread");
        }
        QueueSet& source = lane == ExecutionLane::Main ? main_queue : render_queue;
        ScratchContext& scratch = serial_scratch;
        std::size_t executed = 0;
        JobNode node;
        while (executed < maximum_jobs && popWeighted(source, lane, node)) {
            execute(std::move(node), scratch, no_worker, lane);
            ++executed;
        }
        return executed;
    }

    void cancelQueue(QueueSet& source, ExecutionLane lane) noexcept {
        std::vector<JobNode> canceled;
        {
            std::lock_guard lock(source.mutex);
            for (auto& queue : source.queues) {
                while (!queue.empty()) {
                    canceled.push_back(std::move(queue.front()));
                    queue.pop_front();
                }
            }
        }
        queued.fetch_sub(canceled.size(), std::memory_order_acq_rel);
        queued_by_lane[laneIndex(lane)].fetch_sub(canceled.size(), std::memory_order_acq_rel);
        for (JobNode& node : canceled) {
            finish(std::move(node), true, {});
        }
    }

    void cancelAllQueued() noexcept {
        cancelQueue(worker_injection, ExecutionLane::Worker);
        cancelQueue(io_queue, ExecutionLane::IO);
        cancelQueue(main_queue, ExecutionLane::Main);
        cancelQueue(render_queue, ExecutionLane::Render);
        for (auto& worker : worker_data) {
            cancelQueue(worker->local, ExecutionLane::Worker);
        }
    }

    static void clearTls() noexcept {
        tls_system = nullptr;
        tls_worker_index = no_worker;
        tls_lane = ExecutionLane::Worker;
        tls_group = nullptr;
    }

    JobSystem& system;
    SchedulerConfig config;
    std::thread::id owner_thread;
    std::atomic<JobSystemState> state{JobSystemState::Running};
    std::atomic<JobId> next_id{1};
    std::atomic_size_t queued{0};
    std::array<std::atomic_size_t, 4> queued_by_lane{};
    std::atomic_size_t outstanding{0};
    std::mutex wake_mutex;
    std::condition_variable wake;
    QueueSet worker_injection;
    QueueSet io_queue;
    QueueSet main_queue;
    QueueSet render_queue;
    std::vector<std::unique_ptr<WorkerData>> worker_data;
    std::vector<std::thread> workers;
    std::thread io_worker;
    ScratchContext serial_scratch;
    ScratchContext io_scratch;
    Counters counters;

    static thread_local Impl* tls_system;
    static thread_local std::uint32_t tls_worker_index;
    static thread_local ExecutionLane tls_lane;
    static thread_local detail::JobGroupState* tls_group;
};

thread_local JobSystem::Impl* JobSystem::Impl::tls_system = nullptr;
thread_local std::uint32_t JobSystem::Impl::tls_worker_index = no_worker;
thread_local ExecutionLane JobSystem::Impl::tls_lane = ExecutionLane::Worker;
thread_local detail::JobGroupState* JobSystem::Impl::tls_group = nullptr;

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

JobSystem::JobSystem(SchedulerConfig config, WorkerLauncher worker_launcher)
    : impl_(std::make_unique<Impl>(*this, config, std::move(worker_launcher))) {}

JobSystem::JobSystem(std::uint32_t worker_count,
                     std::uint32_t reserved_main_threads,
                     WorkerLauncher worker_launcher)
    : JobSystem(compatibilityConfig(worker_count, reserved_main_threads),
                std::move(worker_launcher)) {}

JobSystem::~JobSystem() {
    if (Impl::tls_system == impl_.get()) {
        terminateContract("scheduler destroyed from one of its workers");
    }
    shutdown(ShutdownMode::Drain);
}

JobHandle JobSystem::submit(JobFunction function, JobOptions options) {
    return impl_->submit(std::move(function), options);
}

JobHandle JobSystem::submit(JobGroup& group, JobFunction function, JobOptions options) {
    if (group.system_ != this || !group.state_) {
        auto state = std::make_shared<detail::JobState>();
        state->finish(true, {});
        return JobHandle(std::move(state));
    }
    return impl_->submit(std::move(function), options, group.state_, true);
}

void JobSystem::wait(const JobHandle& handle) const noexcept {
    if (!handle.valid() || handle.isComplete()) {
        return;
    }
    if (Impl::tls_system == impl_.get() && Impl::tls_lane == ExecutionLane::Worker) {
        while (!handle.isComplete()) {
            if (!impl_->executeOne(Impl::tls_worker_index)) {
                std::this_thread::yield();
            }
        }
        return;
    }
    if (std::this_thread::get_id() == impl_->owner_thread) {
        if (handle.lane() != ExecutionLane::Main &&
            handle.lane() != ExecutionLane::Render) {
            handle.wait();
            return;
        }
        while (!handle.isComplete()) {
            const std::size_t pumped = const_cast<JobSystem*>(this)->pump(ExecutionLane::Main, 1) +
                                       const_cast<JobSystem*>(this)->pump(ExecutionLane::Render, 1);
            if (pumped == 0) {
                std::unique_lock lock(handle.state_->mutex);
                (void)handle.state_->condition.wait_for(
                    lock, std::chrono::milliseconds(1),
                    [&handle] { return handle.state_->complete; });
            }
        }
        return;
    }
    handle.wait();
}

void JobSystem::wait(const JobCompletion& completion) const noexcept {
    if (!completion.state_ || completion.isComplete()) {
        return;
    }
    if (Impl::tls_system == impl_.get() && Impl::tls_lane == ExecutionLane::Worker) {
        while (!completion.isComplete()) {
            if (!impl_->executeOne(Impl::tls_worker_index)) {
                std::this_thread::yield();
            }
        }
        return;
    }
    if (std::this_thread::get_id() == impl_->owner_thread) {
        while (!completion.isComplete()) {
            const std::size_t pumped = const_cast<JobSystem*>(this)->pump(ExecutionLane::Main, 1) +
                                       const_cast<JobSystem*>(this)->pump(ExecutionLane::Render, 1);
            if (pumped == 0) {
                std::unique_lock lock(completion.state_->mutex);
                (void)completion.state_->condition.wait_for(
                    lock, std::chrono::milliseconds(1),
                    [&completion] { return completion.state_->pending == 0; });
            }
        }
        return;
    }
    std::unique_lock lock(completion.state_->mutex);
    completion.state_->condition.wait(lock,
                                      [&completion] { return completion.state_->pending == 0; });
}

void JobSystem::shutdown(ShutdownMode mode) noexcept {
    if (Impl::tls_system == impl_.get() || std::this_thread::get_id() != impl_->owner_thread) {
        terminateContract("scheduler shutdown requires the owner thread");
    }
    const JobSystemState closing = mode == ShutdownMode::Drain
                                       ? JobSystemState::ClosingDrain
                                       : JobSystemState::ClosingCancel;
    {
        // workerLoop evaluates its wake predicate while holding wake_mutex.
        // Change the scheduler state under the same mutex so shutdown cannot
        // notify between that evaluation and the worker blocking on the CV.
        std::lock_guard wake_lock(impl_->wake_mutex);
        JobSystemState expected = JobSystemState::Running;
        if (!impl_->state.compare_exchange_strong(expected, closing, std::memory_order_acq_rel)) {
            return;
        }
    }
    if (mode == ShutdownMode::CancelPending) {
        impl_->cancelAllQueued();
    }
    impl_->wake.notify_all();

    while (impl_->outstanding.load(std::memory_order_acquire) != 0) {
        const std::size_t pumped = pump(ExecutionLane::Main) + pump(ExecutionLane::Render);
        if (pumped == 0) {
            std::this_thread::yield();
        }
    }
    impl_->wake.notify_all();
    for (std::thread& worker : impl_->workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    if (impl_->io_worker.joinable()) {
        impl_->io_worker.join();
    }
    impl_->workers.clear();
    impl_->state.store(JobSystemState::Stopped, std::memory_order_release);
}

std::size_t JobSystem::pump(ExecutionLane lane, std::size_t maximum_jobs) {
    return impl_->pump(lane, maximum_jobs);
}

std::uint32_t JobSystem::workerCount() const noexcept {
    return static_cast<std::uint32_t>(impl_->worker_data.size());
}

bool JobSystem::isWorkerThread() const noexcept {
    return Impl::tls_system == impl_.get() && Impl::tls_lane == ExecutionLane::Worker;
}

SchedulerMode JobSystem::mode() const noexcept {
    return impl_->config.mode;
}

JobSystemState JobSystem::state() const noexcept {
    return impl_->state.load(std::memory_order_acquire);
}

bool JobSystem::isCancellationRequested() const noexcept {
    return state() == JobSystemState::ClosingCancel;
}

SchedulerTelemetry JobSystem::telemetry() const noexcept {
    SchedulerTelemetry result{
        impl_->counters.submitted.load(std::memory_order_relaxed),
        impl_->counters.started.load(std::memory_order_relaxed),
        impl_->queued.load(std::memory_order_relaxed),
        impl_->counters.submitted.load(std::memory_order_relaxed) -
            impl_->counters.completed.load(std::memory_order_relaxed) -
            impl_->counters.canceled.load(std::memory_order_relaxed) -
            impl_->counters.failed.load(std::memory_order_relaxed),
        impl_->counters.completed.load(std::memory_order_relaxed),
        impl_->counters.canceled.load(std::memory_order_relaxed),
        impl_->counters.failed.load(std::memory_order_relaxed),
        impl_->counters.stolen.load(std::memory_order_relaxed),
        impl_->counters.injection_pops.load(std::memory_order_relaxed)};
    for (std::size_t index = 0; index < result.submitted_by_class.size(); ++index) {
        result.submitted_by_class[index] =
            impl_->counters.submitted_by_class[index].load(std::memory_order_relaxed);
        result.running_by_class[index] =
            impl_->counters.running_by_class[index].load(std::memory_order_relaxed);
        result.completed_by_class[index] =
            impl_->counters.completed_by_class[index].load(std::memory_order_relaxed);
        result.canceled_by_class[index] =
            impl_->counters.canceled_by_class[index].load(std::memory_order_relaxed);
    }
    for (std::size_t index = 0; index < result.queued_by_lane.size(); ++index) {
        result.queued_by_lane[index] =
            impl_->queued_by_lane[index].load(std::memory_order_relaxed);
    }
    return result;
}

bool JobContext::isCancellationRequested() const noexcept {
    return system_.isCancellationRequested() || cancellation_.isCancellationRequested() ||
           group_cancellation_.isCancellationRequested();
}

JobGroup::JobGroup(JobSystem& system)
    : system_(&system), state_(std::make_shared<detail::JobGroupState>(system)) {}

JobGroup::JobGroup(JobSystem& system, std::shared_ptr<detail::JobGroupState> state) noexcept
    : system_(&system), state_(std::move(state)) {}

JobGroup::~JobGroup() {
    release();
}

bool JobCompletion::isComplete() const noexcept {
    if (!state_) {
        return true;
    }
    std::lock_guard lock(state_->mutex);
    return state_->pending == 0;
}

bool JobCompletion::failed() const noexcept {
    if (!state_) {
        return false;
    }
    std::lock_guard lock(state_->mutex);
    return static_cast<bool>(state_->first_failure);
}

void JobCompletion::cancel() noexcept {
    if (state_) {
        state_->cancellation.cancel();
    }
}

void JobCompletion::wait() const noexcept {
    if (!state_) {
        return;
    }
    if (system_ != nullptr) {
        system_->wait(*this);
        return;
    }
    std::unique_lock lock(state_->mutex);
    state_->condition.wait(lock, [this] { return state_->pending == 0; });
}

void JobCompletion::then(std::function<void(JobContext&)> continuation,
                         JobOptions options) const {
    if (!state_ || !system_ || !continuation) {
        return;
    }
    state_->addContinuation([system = system_, continuation = std::move(continuation), options]() mutable {
        (void)system->submit(std::move(continuation), options);
    });
}

JobGroup::JobGroup(JobGroup&& other) noexcept
    : system_(std::exchange(other.system_, nullptr)), state_(std::move(other.state_)) {}

JobGroup& JobGroup::operator=(JobGroup&& other) noexcept {
    if (this != &other) {
        release();
        system_ = std::exchange(other.system_, nullptr);
        state_ = std::move(other.state_);
    }
    return *this;
}

JobHandle JobGroup::submit(JobFunction function, JobOptions options) {
    return system_->submit(*this, std::move(function), options);
}

void JobGroup::cancel() noexcept {
    if (state_) {
        state_->cancellation.cancel();
    }
}

void JobGroup::wait() const noexcept {
    if (!state_) {
        return;
    }
    for (;;) {
        {
            std::lock_guard lock(state_->mutex);
            if (state_->pending == 0) {
                return;
            }
        }
        if (JobSystem::Impl::tls_system == system_->impl_.get() &&
            JobSystem::Impl::tls_lane == ExecutionLane::Worker) {
            if (!system_->impl_->executeOne(JobSystem::Impl::tls_worker_index)) {
                std::this_thread::yield();
            }
        } else if (std::this_thread::get_id() == system_->impl_->owner_thread) {
            const std::size_t pumped = system_->pump(ExecutionLane::Main, 1) +
                                       system_->pump(ExecutionLane::Render, 1);
            if (pumped == 0) {
                std::unique_lock lock(state_->mutex);
                (void)state_->condition.wait_for(
                    lock, std::chrono::milliseconds(1), [this] { return state_->pending == 0; });
            }
        } else {
            std::unique_lock lock(state_->mutex);
            state_->condition.wait(lock, [this] { return state_->pending == 0; });
            return;
        }
    }
}

bool JobGroup::isComplete() const noexcept {
    return pending() == 0;
}

bool JobGroup::isCancellationRequested() const noexcept {
    return state_ && state_->cancellation.isCancellationRequested();
}

bool JobGroup::failed() const noexcept {
    return static_cast<bool>(firstFailure());
}

JobId JobGroup::firstFailureId() const noexcept {
    if (!state_) {
        return 0;
    }
    std::lock_guard lock(state_->mutex);
    return state_->first_failure ? state_->first_failure_id : 0;
}

std::exception_ptr JobGroup::firstFailure() const noexcept {
    if (!state_) {
        return {};
    }
    std::lock_guard lock(state_->mutex);
    return state_->first_failure;
}

std::size_t JobGroup::pending() const noexcept {
    if (!state_) {
        return 0;
    }
    std::lock_guard lock(state_->mutex);
    return state_->pending;
}

JobCompletion JobGroup::completion() const noexcept {
    if (!state_ || !system_) {
        return {};
    }
    return JobCompletion(*system_, state_);
}

void JobGroup::release() noexcept {
    if (!state_) {
        return;
    }
    if (pending() != 0) {
        if (JobSystem::Impl::tls_group == state_.get()) {
            std::fputs("JobGroup destroyed from one of its own jobs\n", stderr);
            terminateContract("active group destroyed from one of its own jobs");
        }
        std::fputs("active JobGroup destroyed; canceling and waiting\n", stderr);
        cancel();
        wait();
    }
    state_.reset();
    system_ = nullptr;
}

std::size_t JobGraph::size() const noexcept {
    return nodes_.size();
}

JobGraphNode JobGraphBuilder::add(JobGroup::JobFunction function, JobOptions options) {
    const JobGraphNode id = nodes_.size();
    nodes_.push_back(JobGraph::Node{std::move(function), options, {}, 0});
    return id;
}

void JobGraphBuilder::precedes(JobGraphNode dependency, JobGraphNode continuation) {
    if (dependency >= nodes_.size() || continuation >= nodes_.size() ||
        dependency == continuation) {
        throw std::out_of_range("invalid JobGraph edge");
    }
    auto& edges = nodes_[dependency].continuations;
    if (std::find(edges.begin(), edges.end(), continuation) != edges.end()) {
        return;
    }
    edges.push_back(continuation);
    ++nodes_[continuation].dependency_count;
}

JobGraph JobGraphBuilder::build() && {
    std::vector<std::size_t> remaining;
    remaining.reserve(nodes_.size());
    std::deque<JobGraphNode> ready;
    for (JobGraphNode index = 0; index < nodes_.size(); ++index) {
        remaining.push_back(nodes_[index].dependency_count);
        if (remaining.back() == 0) {
            ready.push_back(index);
        }
    }
    std::size_t visited = 0;
    while (!ready.empty()) {
        const JobGraphNode node = ready.front();
        ready.pop_front();
        ++visited;
        for (const JobGraphNode continuation : nodes_[node].continuations) {
            if (--remaining[continuation] == 0) {
                ready.push_back(continuation);
            }
        }
    }
    if (visited != nodes_.size()) {
        throw std::invalid_argument("JobGraph contains a dependency cycle");
    }
    return JobGraph(std::move(nodes_));
}

JobCompletion JobGraph::start(JobSystem& system) const {
    auto group_state = std::make_shared<detail::JobGroupState>(system);
    // The sentinel keeps the group active while all root nodes are submitted.
    group_state->add();
    if (nodes_.empty()) {
        group_state->finish(0, {});
        return JobCompletion(system, group_state);
    }

    struct GraphRun final : std::enable_shared_from_this<GraphRun> {
        GraphRun(JobSystem& owner,
                 const std::vector<JobGraph::Node>& source,
                 std::shared_ptr<JobGroup> target)
            : nodes(source), group(std::move(target)), remaining(nodes.size()) {
            (void)owner;
            for (std::size_t index = 0; index < nodes.size(); ++index) {
                remaining[index] = nodes[index].dependency_count;
            }
        }

        void schedule(JobGraphNode index) {
            const auto self = shared_from_this();
            (void)group->submit(
                [self, index](JobContext& context) {
                    // A failed node must not release its continuations. The
                    // scheduler records the exception in the group after the
                    // callback returns, so advancing dependencies here would
                    // otherwise race ahead of failure publication.
                    self->nodes[index].function(context);
                    self->completed(index);
                },
                nodes[index].options);
        }

        void completed(JobGraphNode index) {
            for (const JobGraphNode continuation : nodes[index].continuations) {
                if (remaining[continuation].fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    schedule(continuation);
                }
            }
        }

        std::vector<JobGraph::Node> nodes;
        std::shared_ptr<JobGroup> group;
        std::vector<std::atomic_size_t> remaining;
    };

    auto runtime_group = std::shared_ptr<JobGroup>(new JobGroup(system, group_state));
    const auto run = std::make_shared<GraphRun>(system, nodes_, std::move(runtime_group));
    for (JobGraphNode index = 0; index < nodes_.size(); ++index) {
        if (nodes_[index].dependency_count == 0) {
            run->schedule(index);
        }
    }
    group_state->finish(0, {});
    return JobCompletion(system, group_state);
}

JobGroup JobGraph::run(JobSystem& system) const {
    const auto completion = start(system);
    return JobGroup(system, completion.state_);
}

} // namespace genomes::jobs
