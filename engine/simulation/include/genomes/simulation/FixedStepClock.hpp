#pragma once

#include <genomes/foundation/Time.hpp>
#include <genomes/foundation/WallClock.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace genomes::simulation {

struct FixedStepConfig final {
    std::uint32_t max_steps{8};
    foundation::Nanoseconds max_frame_delta{std::chrono::milliseconds(250)};
};

struct FixedStepAdvanceResult final {
    std::uint32_t steps_executed{0};
    double interpolation_alpha{0.0};
    foundation::Nanoseconds dropped_time{};
    foundation::Nanoseconds consumed_frame_time{};
    foundation::SimulationTick first_tick{};
    foundation::SimulationTick next_tick{};
};

class FixedStepClock final {
public:
    using StepCallback = void (*)(double, foundation::SimulationTick, const void*) noexcept;
    using TimePoint = foundation::WallClock::TimePoint;

    explicit FixedStepClock(FixedStepConfig config = {}) noexcept;

    [[nodiscard]] FixedStepAdvanceResult advance(TimePoint now,
                                                 StepCallback callback = nullptr,
                                                 const void* user_data = nullptr) noexcept;

    [[nodiscard]] FixedStepAdvanceResult advanceBy(
        foundation::Nanoseconds frame_delta,
        StepCallback callback = nullptr,
        const void* user_data = nullptr) noexcept;

    template <class Callback>
    [[nodiscard]] FixedStepAdvanceResult advanceBy(foundation::Nanoseconds frame_delta,
                                                   Callback&& callback) noexcept {
        using CallbackType = std::remove_reference_t<Callback>;
        return advanceBy(frame_delta, &invoke<CallbackType>, &callback);
    }

    template <class Callback>
    [[nodiscard]] FixedStepAdvanceResult advance(TimePoint now, Callback&& callback) noexcept {
        using CallbackType = std::remove_reference_t<Callback>;
        return advance(now, &invoke<CallbackType>, &callback);
    }

    void resetTiming() noexcept;
    void resetAll() noexcept;

    [[nodiscard]] const FixedStepConfig& config() const noexcept {
        return config_;
    }

    [[nodiscard]] foundation::SimulationTick currentTick() const noexcept {
        return current_tick_;
    }

    [[nodiscard]] foundation::Nanoseconds droppedTimeTotal() const noexcept {
        return dropped_time_total_;
    }

private:
    template <class CallbackType>
    static void invoke(double fixed_dt,
                       foundation::SimulationTick tick,
                       const void* user_data) noexcept {
        (*const_cast<CallbackType*>(static_cast<const CallbackType*>(user_data)))(fixed_dt, tick);
    }

    static constexpr std::uint64_t PhaseThreshold = 1'000'000'000ull;
    static constexpr std::uint64_t FixedStepDenominator = 60ull;

    FixedStepConfig config_{};
    std::optional<TimePoint> last_wall_time_;
    std::uint64_t phase_units_{0};
    foundation::Nanoseconds dropped_time_total_{};
    foundation::SimulationTick current_tick_{};
};

} // namespace genomes::simulation
