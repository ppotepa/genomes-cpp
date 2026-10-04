#pragma once

#include <genomes/jobs/SchedulerTypes.hpp>

#include <condition_variable>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>

namespace genomes::jobs {

class JobSystem;

namespace detail {

struct JobState final {
    mutable std::mutex mutex;
    std::condition_variable condition;
    bool complete{false};
    bool canceled{false};
    std::exception_ptr failure;

    void finish(bool was_canceled, std::exception_ptr error) noexcept {
        {
            std::lock_guard lock(mutex);
            canceled = was_canceled;
            failure = error;
            complete = true;
        }
        condition.notify_all();
    }
};

} // namespace detail

class JobHandle final {
public:
    JobHandle() = default;

    [[nodiscard]] bool valid() const noexcept {
        return static_cast<bool>(state_);
    }

    [[nodiscard]] bool isComplete() const noexcept {
        if (!state_) {
            return true;
        }
        std::lock_guard lock(state_->mutex);
        return state_->complete;
    }

    [[nodiscard]] bool wasCanceled() const noexcept {
        if (!state_) {
            return false;
        }
        std::lock_guard lock(state_->mutex);
        return state_->canceled;
    }

    [[nodiscard]] bool failed() const noexcept {
        if (!state_) {
            return false;
        }
        std::lock_guard lock(state_->mutex);
        return static_cast<bool>(state_->failure);
    }

    // Always routes through the owning scheduler so worker-side waits retain
    // cooperative helping and owner-affinity contract checks.
    void wait() const noexcept;

    [[nodiscard]] JobId id() const noexcept { return id_; }
    [[nodiscard]] ExecutionLane lane() const noexcept { return lane_; }

    [[nodiscard]] std::exception_ptr failure() const noexcept {
        if (!state_) {
            return {};
        }
        std::lock_guard lock(state_->mutex);
        return state_->failure;
    }

private:
    friend class JobSystem;

    explicit JobHandle(std::shared_ptr<detail::JobState> state,
                       JobSystem* system = nullptr,
                       JobId id = 0,
                       ExecutionLane lane = ExecutionLane::Worker) noexcept
        : state_(std::move(state)), system_(system), id_(id), lane_(lane) {}

    void waitRaw() const noexcept {
        if (!state_) {
            return;
        }
        std::unique_lock lock(state_->mutex);
        state_->condition.wait(lock, [this] { return state_->complete; });
    }

    std::shared_ptr<detail::JobState> state_;
    JobSystem* system_{nullptr};
    JobId id_{0};
    ExecutionLane lane_{ExecutionLane::Worker};
};

} // namespace genomes::jobs
