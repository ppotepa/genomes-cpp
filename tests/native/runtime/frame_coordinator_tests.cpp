#include <genomes/jobs/JobSystem.hpp>
#include <genomes/render/NullRenderer.hpp>
#include <genomes/render/PoseSnapshot.hpp>
#include <genomes/runtime/FrameCoordinator.hpp>
#include <genomes/runtime/RenderCoordinator.hpp>
#include <genomes/runtime/SceneDirector.hpp>
#include <genomes/ui/UiRuntime.hpp>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {

class CountingScene final : public genomes::runtime::Scene {
public:
    explicit CountingScene(std::shared_ptr<std::uint32_t> updates,
                           std::shared_ptr<std::uint64_t> tick,
                           std::shared_ptr<std::uint64_t> epoch,
                           std::shared_ptr<bool> scheduler_seen)
        : updates_(std::move(updates)), tick_(std::move(tick)), epoch_(std::move(epoch)),
          scheduler_seen_(std::move(scheduler_seen)) {}
    [[nodiscard]] genomes::foundation::SceneId id() const noexcept override {
        return genomes::foundation::scene_id("test.frame-coordinator");
    }

    void fixed_update(genomes::runtime::SceneContext& context,
                      const genomes::simulation::TickContext& tick) override {
        *tick_ = tick.tick.value;
        *epoch_ = context.scene_epoch;
        *scheduler_seen_ = context.scheduler != nullptr;
        ++*updates_;
    }

private:
    std::shared_ptr<std::uint32_t> updates_;
    std::shared_ptr<std::uint64_t> tick_;
    std::shared_ptr<std::uint64_t> epoch_;
    std::shared_ptr<bool> scheduler_seen_;
};

class UnhealthyRenderer final : public genomes::render::IRenderer {
public:
    void begin_frame() override { ++begin_calls; }
    void submit(const genomes::render::PresentationSnapshot&,
                const genomes::ui::UiRenderFrame&) override {}
    void end_frame() override { ++end_calls; }
    [[nodiscard]] bool healthy() const noexcept override { return false; }

    std::uint32_t begin_calls{0U};
    std::uint32_t end_calls{0U};
};

class FailingTickScene final : public genomes::runtime::Scene {
public:
    [[nodiscard]] genomes::foundation::SceneId id() const noexcept override {
        return genomes::foundation::scene_id("test.frame-coordinator-failure");
    }

    void fixed_update(genomes::runtime::SceneContext&,
                      const genomes::simulation::TickContext&) override {
        if (++calls_ > 1U) {
            throw std::runtime_error("synthetic tick failure");
        }
    }

private:
    std::uint32_t calls_{0U};
};

} // namespace

int main() {
    genomes::render::NullRenderer renderer;
    genomes::ui::UiRuntime ui;
    genomes::render::PresentationSnapshot presentation;
    genomes::jobs::JobSystem jobs{2};
    // The director receives no local scheduler; it must resolve the shared
    // process scheduler fallback for the scene context.
    genomes::runtime::SceneDirector director(renderer, ui, presentation);
    const auto scene_id = genomes::foundation::scene_id("test.frame-coordinator");
    auto updates = std::make_shared<std::uint32_t>(0U);
    auto tick = std::make_shared<std::uint64_t>(0U);
    auto epoch = std::make_shared<std::uint64_t>(0U);
    auto scheduler_seen = std::make_shared<bool>(false);
    assert(director.register_scene(scene_id, [updates, tick, epoch, scheduler_seen] {
        return std::make_unique<CountingScene>(updates, tick, epoch, scheduler_seen);
    }));
    assert(director.start(scene_id));

    genomes::simulation::SessionSimulationClock clock;
    genomes::runtime::FrameCoordinator coordinator(director, clock, jobs);
    const auto result = coordinator.advance(
        std::chrono::milliseconds{17}, 1.0 / 60.0,
        genomes::simulation::TickSchedulingMode::DeterministicCapture);
    assert(result.scheduled_ticks == 1U);
    assert(result.completed_ticks == 1U);
    assert(*updates == 1U);
    assert(*tick == 1U);
    assert(*epoch == 1U);
    assert(presentation.snapshot_generation != 0U);
    assert(*scheduler_seen);
    assert(coordinator.telemetry().frame_index == 1U);
    assert(coordinator.telemetry().scheduled_ticks == 1U);
    assert(coordinator.telemetry().completed_ticks == 1U);
    assert(coordinator.telemetry().fault == genomes::runtime::FrameFaultDomain::None);

    // Capture/headless scheduling must drain every bounded catch-up tick
    // before the frame is considered complete.  The session clock caps this
    // 250 ms input at eight ticks.
    const auto catch_up = coordinator.advance(
        std::chrono::milliseconds{250}, 1.0 / 60.0,
        genomes::simulation::TickSchedulingMode::DeterministicCapture);
    assert(catch_up.scheduled_ticks == 8U);
    assert(catch_up.completed_ticks == 8U);
    assert(*updates == 9U);
    assert(*tick == 9U);
    assert(coordinator.telemetry().scheduled_ticks == 8U);
    assert(coordinator.telemetry().completed_ticks == 8U);

    const auto published_before_gpu_failure = presentation.snapshot_generation;

    coordinator.markGpuFailure();
    assert(coordinator.telemetry().fault == genomes::runtime::FrameFaultDomain::GpuFailure);
    const auto after_gpu_failure = coordinator.advance(
        std::chrono::milliseconds{250}, 1.0 / 60.0,
        genomes::simulation::TickSchedulingMode::Interactive);
    assert(after_gpu_failure.completed_ticks == 0U);
    assert(coordinator.telemetry().frame_index == 2U);
    assert(coordinator.telemetry().fault == genomes::runtime::FrameFaultDomain::GpuFailure);

    UnhealthyRenderer unhealthy_renderer;
    genomes::ui::UiRuntime unhealthy_ui;
    genomes::render::PresentationSnapshot unhealthy_presentation;
    genomes::jobs::JobSystem unhealthy_jobs{1};
    genomes::runtime::SceneDirector unhealthy_director(
        unhealthy_renderer, unhealthy_ui, unhealthy_presentation, &unhealthy_jobs);
    genomes::runtime::RenderCoordinator render_coordinator(unhealthy_director);
    assert(!render_coordinator.present());
    assert(unhealthy_renderer.begin_calls == 0U);
    assert(unhealthy_renderer.end_calls == 0U);
    genomes::runtime::FrameCoordinator unhealthy_coordinator(
        unhealthy_director, clock, unhealthy_jobs);
    unhealthy_coordinator.present();
    assert(unhealthy_coordinator.telemetry().fault ==
           genomes::runtime::FrameFaultDomain::GpuFailure);

    genomes::render::NullRenderer failing_renderer;
    genomes::ui::UiRuntime failing_ui;
    genomes::render::PresentationSnapshot failing_presentation;
    genomes::jobs::JobSystem failing_jobs{2};
    genomes::runtime::SceneDirector failing_director(
        failing_renderer, failing_ui, failing_presentation, &failing_jobs);
    const auto failing_id = genomes::foundation::scene_id("test.frame-coordinator-failure");
    assert(failing_director.register_scene(
        failing_id, [] { return std::make_unique<FailingTickScene>(); }));
    assert(failing_director.start(failing_id));
    genomes::simulation::SessionSimulationClock failing_clock;
    genomes::runtime::FrameCoordinator failing_coordinator(
        failing_director, failing_clock, failing_jobs);
    const auto first_failing_scene_frame = failing_coordinator.advance(
        std::chrono::milliseconds{17}, 1.0 / 60.0,
        genomes::simulation::TickSchedulingMode::DeterministicCapture);
    assert(first_failing_scene_frame.completed_ticks == 1U);
    const auto published_before_failed_tick = failing_presentation.snapshot_generation;
    assert(published_before_gpu_failure != 0U);
    const auto failed_frame = failing_coordinator.advance(
        std::chrono::milliseconds{17}, 1.0 / 60.0,
        genomes::simulation::TickSchedulingMode::DeterministicCapture);
    assert(failed_frame.scheduled_ticks == 1U);
    assert(failed_frame.completed_ticks == 0U);
    assert(failing_coordinator.telemetry().fault ==
           genomes::runtime::FrameFaultDomain::CpuStall);
    assert(failing_presentation.snapshot_generation == published_before_failed_tick);

    // The failed coordinator never publishes a replacement snapshot; the
    // active presentation remains the last complete one.

    // A pose result from the previous scene must not become readable again
    // after the scene epoch advances, even when a reader still holds it.
    genomes::render::PoseSnapshotExchange poses;
    auto pose_write = poses.acquireWrite();
    assert(pose_write);
    pose_write.value().snapshot().metadata.scene_epoch = 11U;
    pose_write.value().snapshot().metadata.tick = 7U;
    assert(poses.publish(std::move(pose_write.value())));
    auto old_pose = poses.acquireLatestRead();
    assert(old_pose);
    std::optional<genomes::render::PoseSnapshotExchange::ReadLease> old_pose_lease(
        std::move(old_pose.value()));
    poses.rejectBeforeSceneEpoch(12U);
    assert(!poses.acquireLatestRead());
    old_pose_lease.reset();
    assert(!poses.acquireLatestRead());
    return 0;
}
