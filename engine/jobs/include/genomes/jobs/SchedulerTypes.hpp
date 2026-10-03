#pragma once

#include <cstddef>
#include <cstdint>
#include <array>

namespace genomes::jobs {

using JobId = std::uint64_t;

enum class SchedulerMode : std::uint8_t {
    Parallel,
    Serial,
};

enum class ExecutionLane : std::uint8_t {
    Main,
    Worker,
    Render,
    IO,
};

enum class WorkClass : std::uint8_t {
    General,
    Simulation,
    Presentation,
    Procedural,
    BlockingIO,
    Render,
};

enum class JobPriority : std::uint8_t {
    Critical,
    Normal,
    Background,
};

struct SchedulerConfig final {
    SchedulerMode mode{SchedulerMode::Parallel};
    // Zero selects the topology-derived worker count.
    std::uint32_t worker_count{0};
    // Used only by automatic topology selection. The default reserves the
    // application/render thread and one OS/driver core.
    std::uint32_t reserved_threads{2};
    bool enable_io_worker{true};
    std::size_t scratch_capacity{64U * 1024U};
};

// Derive the default CPU worker count from the logical processor topology.
// The application/render thread and one driver/OS slot remain reserved. An
// explicit SchedulerConfig::worker_count bypasses this policy.
[[nodiscard]] constexpr std::uint32_t topologyWorkerCount(
    std::uint32_t logical_processors,
    std::uint32_t reserved_threads = 2U) noexcept {
    if (logical_processors <= 1U) {
        return 0U;
    }
    if (logical_processors == 2U) {
        return 1U;
    }
    return logical_processors > reserved_threads ? logical_processors - reserved_threads : 0U;
}

struct SchedulerTelemetry final {
    std::uint64_t submitted{0};
    std::uint64_t started{0};
    std::uint64_t queued{0};
    std::uint64_t running{0};
    std::uint64_t completed{0};
    std::uint64_t canceled{0};
    std::uint64_t failed{0};
    std::uint64_t stolen{0};
    std::uint64_t injection_pops{0};
    std::array<std::uint64_t, 6> submitted_by_class{};
    std::array<std::uint64_t, 6> running_by_class{};
    std::array<std::uint64_t, 6> completed_by_class{};
    std::array<std::uint64_t, 6> canceled_by_class{};
};

} // namespace genomes::jobs
