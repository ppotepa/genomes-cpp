#pragma once

#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>

namespace genomes::jobs {

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

    void wait() const noexcept {
        if (!state_) {
            return;
        }
        std::unique_lock lock(state_->mutex);
        state_->condition.wait(lock, [this] { return state_->complete; });
    }

private:
    friend class JobSystem;

    explicit JobHandle(std::shared_ptr<detail::JobState> state) noexcept
        : state_(std::move(state)) {}

    std::shared_ptr<detail::JobState> state_;
};

} // namespace genomes::jobs
