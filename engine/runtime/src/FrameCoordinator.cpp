#include <genomes/runtime/FrameCoordinator.hpp>

#include <genomes/jobs/JobSystem.hpp>
#include <genomes/jobs/JobGraph.hpp>
#include <genomes/runtime/RenderCoordinator.hpp>
#include <genomes/runtime/SceneDirector.hpp>

#include <vector>

namespace genomes::runtime {

FrameCoordinator::FrameCoordinator(SceneDirector& director,
                                   simulation::SessionSimulationClock& clock,
                                   jobs::JobSystem& jobs) noexcept
    : director_(director), render_coordinator_(director), clock_(clock), jobs_(jobs) {}

CoordinatedFrameResult FrameCoordinator::advance(
    foundation::Nanoseconds frame_delta,
    double frame_dt_seconds,
    simulation::TickSchedulingMode mode) {
    if (telemetry_.fault == FrameFaultDomain::GpuFailure) {
        // Device loss is terminal for this application session. Do not let a
        // later frame advance authoritative simulation state after rendering
        // has stopped and the application is preparing controlled shutdown.
        return {};
    }
    const auto started = std::chrono::steady_clock::now();
    const simulation::SessionTickPlan plan = clock_.planBy(frame_delta, mode);
    const auto simulation_started = std::chrono::steady_clock::now();
    CoordinatedFrameResult result;
    result.simulation = plan.advance;
    bool tick_failed = false;
    result.scheduled_ticks = plan.tick_count;
    if (plan.tick_count != 0U) {
        // Keep the interactive tick chain sequential while moving its work
        // through the central scheduler.  A dependency edge between ticks
        // preserves authoritative ordering; the frame waits for the final
        // commit before publishing presentation state.
        jobs::JobGraphBuilder builder;
        std::vector<jobs::JobGraphNode> ticks;
        ticks.reserve(plan.tick_count);
        for (std::size_t index = 0; index < plan.tick_count; ++index) {
            jobs::JobOptions options;
            // Scene orchestration is the owner barrier. Domain systems fan
            // out from it through the central scheduler, so a scene/runtime
            // never synchronously waits for child work while occupying a
            // worker lane.
            options.lane = jobs::ExecutionLane::Main;
            options.work_class = jobs::WorkClass::Simulation;
            ticks.push_back(builder.add(
                [this, tick = plan.ticks[index]](jobs::JobContext&) {
                    director_.fixed_update(tick);
                }, options));
            if (index != 0U) {
                builder.precedes(ticks[index - 1U], ticks[index]);
            }
        }
        auto tick_group = std::move(builder).build().run(jobs_);
        tick_group.wait();
        if (!tick_group.failed()) {
            result.completed_ticks = plan.tick_count;
        } else {
            tick_failed = true;
            telemetry_.fault = FrameFaultDomain::CpuStall;
        }
    }
    telemetry_.simulation_duration = std::chrono::duration_cast<foundation::Nanoseconds>(
        std::chrono::steady_clock::now() - simulation_started);
    // A failed authoritative tick must not advance presentation timing or
    // publish a snapshot derived from partially-mutated simulation state.
    // Keep the last complete snapshot visible while the fault is reported.
    if (!tick_failed) {
        director_.set_presentation_timing(plan.advance.first_tick,
                                          plan.advance.next_tick,
                                          plan.advance.interpolation_alpha);
    }
    (void)jobs_.pump(jobs::ExecutionLane::Main);
    const auto presentation_started = std::chrono::steady_clock::now();
    if (!tick_failed) {
        director_.frame_update(frame_dt_seconds);
    }
    telemetry_.presentation_duration = std::chrono::duration_cast<foundation::Nanoseconds>(
        std::chrono::steady_clock::now() - presentation_started);
    telemetry_.animation_duration = director_.telemetry().animation_duration;
    telemetry_.extraction_duration = director_.telemetry().extraction_duration;
    telemetry_.frame_index += 1U;
    telemetry_.scheduled_ticks = result.scheduled_ticks;
    telemetry_.completed_ticks = result.completed_ticks;
    telemetry_.dropped_time = result.simulation.dropped_time;
    telemetry_.cpu_duration = std::chrono::duration_cast<foundation::Nanoseconds>(
        std::chrono::steady_clock::now() - started);
    telemetry_.scheduler = jobs_.telemetry();
    telemetry_.rejected_stale_snapshots = director_.presentation_api().rejectedStale();
    director_.telemetry().simulation_duration = telemetry_.simulation_duration;
    director_.telemetry().presentation_duration = telemetry_.presentation_duration;
    director_.telemetry().animation_duration = telemetry_.animation_duration;
    director_.telemetry().extraction_duration = telemetry_.extraction_duration;
    director_.telemetry().scheduler = telemetry_.scheduler;
    director_.telemetry().rejected_stale_snapshots = telemetry_.rejected_stale_snapshots;
    if (telemetry_.fault != FrameFaultDomain::GpuFailure && !tick_failed) {
        telemetry_.fault = telemetry_.cpu_duration > std::chrono::milliseconds{100}
                               ? FrameFaultDomain::CpuStall
                               : FrameFaultDomain::None;
    }
    return result;
}

void FrameCoordinator::present() {
    const auto started = std::chrono::steady_clock::now();
    (void)jobs_.pump(jobs::ExecutionLane::Render);
    if (!render_coordinator_.present()) {
        telemetry_.fault = FrameFaultDomain::GpuFailure;
    }
    telemetry_.gpu_duration = std::chrono::duration_cast<foundation::Nanoseconds>(
        std::chrono::steady_clock::now() - started);
    director_.telemetry().gpu_duration = telemetry_.gpu_duration;
}

} // namespace genomes::runtime
