#pragma once

#include <genomes/jobs/Cancellation.hpp>
#include <genomes/jobs/JobContext.hpp>
#include <genomes/jobs/JobHandle.hpp>
#include <genomes/jobs/SchedulerTypes.hpp>

#include <cstddef>
#include <exception>
#include <functional>
#include <memory>

namespace genomes::jobs {

class JobSystem;

namespace detail {
struct JobGroupState;
}

struct JobOptions final {
    ExecutionLane lane{ExecutionLane::Worker};
    WorkClass work_class{WorkClass::General};
    JobPriority priority{JobPriority::Normal};
    CancelToken cancellation{};
};

class JobCompletion final {
public:
    JobCompletion() = default;

    [[nodiscard]] bool valid() const noexcept { return static_cast<bool>(state_); }
    [[nodiscard]] bool isComplete() const noexcept;
    [[nodiscard]] bool failed() const noexcept;
    [[nodiscard]] bool poll() const noexcept { return isComplete(); }
    void cancel() noexcept;
    void wait() const noexcept;

    // Register work without making the caller's worker wait for the group.
    // The continuation is submitted to the same scheduler after completion.
    void then(std::function<void(JobContext&)> continuation,
              JobOptions options = {}) const;

private:
    friend class JobGroup;
    friend class JobGraph;
    friend class JobSystem;

    JobCompletion(JobSystem& system, std::shared_ptr<detail::JobGroupState> state) noexcept
        : system_(&system), state_(std::move(state)) {}

    JobSystem* system_{nullptr};
    std::shared_ptr<detail::JobGroupState> state_;
};

class JobGroup final {
public:
    using JobFunction = std::function<void(JobContext&)>;

    explicit JobGroup(JobSystem& system);
    ~JobGroup();

    JobGroup(const JobGroup&) = delete;
    JobGroup& operator=(const JobGroup&) = delete;
    JobGroup(JobGroup&& other) noexcept;
    JobGroup& operator=(JobGroup&& other) noexcept;

    [[nodiscard]] JobHandle submit(JobFunction function, JobOptions options = {});
    void cancel() noexcept;
    void wait() const noexcept;

    [[nodiscard]] bool isComplete() const noexcept;
    [[nodiscard]] bool isCancellationRequested() const noexcept;
    [[nodiscard]] bool failed() const noexcept;
    [[nodiscard]] JobId firstFailureId() const noexcept;
    [[nodiscard]] std::exception_ptr firstFailure() const noexcept;
    [[nodiscard]] std::size_t pending() const noexcept;
    [[nodiscard]] JobCompletion completion() const noexcept;

private:
    friend class JobSystem;
    friend class JobGraph;

    JobGroup(JobSystem& system, std::shared_ptr<detail::JobGroupState> state) noexcept;
    void release() noexcept;

    JobSystem* system_{nullptr};
    std::shared_ptr<detail::JobGroupState> state_;
};

} // namespace genomes::jobs
