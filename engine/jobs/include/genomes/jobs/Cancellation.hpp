#pragma once

#include <atomic>
#include <memory>

namespace genomes::jobs {

namespace detail {
struct CancelState final {
    std::atomic_bool requested{false};
};
} // namespace detail

class CancelToken final {
public:
    CancelToken() = default;

    [[nodiscard]] bool isCancellationRequested() const noexcept {
        const auto state = state_.lock();
        return state && state->requested.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool valid() const noexcept { return !state_.expired(); }

private:
    friend class CancelSource;
    explicit CancelToken(const std::shared_ptr<detail::CancelState>& state) noexcept : state_(state) {}

    std::weak_ptr<detail::CancelState> state_;
};

class CancelSource final {
public:
    CancelSource() : state_(std::make_shared<detail::CancelState>()) {}

    [[nodiscard]] CancelToken token() const noexcept { return CancelToken(state_); }
    void cancel() noexcept { state_->requested.store(true, std::memory_order_release); }
    [[nodiscard]] bool isCancellationRequested() const noexcept {
        return state_->requested.load(std::memory_order_acquire);
    }

private:
    std::shared_ptr<detail::CancelState> state_;
};

} // namespace genomes::jobs
