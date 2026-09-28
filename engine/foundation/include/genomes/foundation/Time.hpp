#pragma once

#include <chrono>
#include <compare>
#include <cstdint>

namespace genomes::foundation {

using Nanoseconds = std::chrono::nanoseconds;
using SteadyClock = std::chrono::steady_clock;

[[nodiscard]] constexpr Nanoseconds secondsToDuration(double seconds) noexcept {
    return std::chrono::duration_cast<Nanoseconds>(std::chrono::duration<double>(seconds));
}

[[nodiscard]] constexpr double durationToSeconds(Nanoseconds duration) noexcept {
    return std::chrono::duration<double>(duration).count();
}

// Tick zero is a valid initial simulation state, so this is deliberately a
// value type rather than StrongId (whose zero value means invalid).
struct SimulationTick final {
    std::uint64_t value{0};

    constexpr void increment() noexcept {
        ++value;
    }

    friend constexpr auto operator<=>(const SimulationTick&, const SimulationTick&) noexcept =
        default;
};

} // namespace genomes::foundation
