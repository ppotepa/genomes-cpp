#pragma once

#include <genomes/foundation/Time.hpp>
#include <genomes/jobs/SchedulerTypes.hpp>
#include <genomes/runtime/RenderCoordinator.hpp>
#include <genomes/simulation/SessionSimulationClock.hpp>

#include <cstddef>
#include <cstdint>
#include <chrono>

namespace genomes::jobs {
class JobSystem;
}

namespace genomes::runtime {

class SceneDirector;

struct CoordinatedFrameResult final {
    simulation::FixedStepAdvanceResult simulation{};
    std::size_t scheduled_ticks{0};
    std::size_t completed_ticks{0};
};

enum class FrameFaultDomain : std::uint8_t {
    None,
    CpuStall,
    GpuFailure,
};

struct FrameCoordinatorTelemetry final {
    std::uint64_t frame_index{0};
    std::size_t scheduled_ticks{0};
    std::size_t completed_ticks{0};
    foundation::Nanoseconds dropped_time{};
    foundation::Nanoseconds cpu_duration{};
    foundation::Nanoseconds simulation_duration{};
    foundation::Nanoseconds presentation_duration{};
    foundation::Nanoseconds gpu_duration{};
    jobs::SchedulerTelemetry scheduler{};
    std::uint64_t rejected_stale_snapshots{0U};
    FrameFaultDomain fault{FrameFaultDomain::None};
};

// Owns frame-stage ordering. The first migration step intentionally executes
// scene ticks synchronously; later execution plans can move tick systems to the
// central scheduler without returning orchestration to GameApplication.
class FrameCoordinator final {
public:
    FrameCoordinator(SceneDirector& director,
                     simulation::SessionSimulationClock& clock,
                     jobs::JobSystem& jobs) noexcept;

    [[nodiscard]] CoordinatedFrameResult advance(
        foundation::Nanoseconds frame_delta,
        double frame_dt_seconds,
        simulation::TickSchedulingMode mode =
            simulation::TickSchedulingMode::Interactive);
    void present();

    [[nodiscard]] const FrameCoordinatorTelemetry& telemetry() const noexcept {
        return telemetry_;
    }

    void markGpuFailure() noexcept {
        telemetry_.fault = FrameFaultDomain::GpuFailure;
    }

private:
    SceneDirector& director_;
    RenderCoordinator render_coordinator_;
    simulation::SessionSimulationClock& clock_;
    jobs::JobSystem& jobs_;
    FrameCoordinatorTelemetry telemetry_{};
};

} // namespace genomes::runtime
