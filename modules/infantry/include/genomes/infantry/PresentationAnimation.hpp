#pragma once

#include <genomes/infantry/AnimationSystem.hpp>
#include <genomes/jobs/JobSystem.hpp>

#include <concepts>
#include <utility>

namespace genomes::infantry {

// Presentation boundary owned by the infantry module. Game scenes can request
// animation work and consume immutable pose snapshots without depending on
// the concrete evaluator implementation or its scheduling internals.
class PresentationAnimation final {
public:
    [[nodiscard]] static foundation::Result<PresentationAnimation, foundation::Error>
    create(std::size_t chunk_size = 64U);

    void bindScheduler(jobs::SchedulerClient jobs) noexcept { jobs_ = jobs; }
    void bindScheduler(jobs::JobSystem& jobs) noexcept {
        jobs_ = jobs::SchedulerClient(jobs);
    }

    [[nodiscard]] foundation::Result<void, foundation::Error> evaluate(
        std::span<AnimationEntity> entities, std::uint64_t simulation_tick,
        float fixed_dt_seconds, jobs::SchedulerClient* jobs = nullptr,
        jobs::CancelToken cancellation = {});

    [[nodiscard]] foundation::Result<void, foundation::Error> evaluate(
        AnimationWorkSet work, jobs::SchedulerClient* jobs = nullptr,
        jobs::CancelToken cancellation = {}) {
        return evaluate(std::span<AnimationEntity>(work.entities), work.simulation_tick,
                        work.fixed_dt_seconds, jobs, cancellation);
    }

    template <class Scheduler>
        requires std::same_as<Scheduler, jobs::JobSystem>
    [[nodiscard]] foundation::Result<void, foundation::Error> evaluate(
        std::span<AnimationEntity> entities, std::uint64_t simulation_tick,
        float fixed_dt_seconds, Scheduler* jobs,
        jobs::CancelToken cancellation = {}) {
        if (jobs == nullptr) {
            return evaluate(entities, simulation_tick, fixed_dt_seconds,
                            static_cast<jobs::SchedulerClient*>(nullptr), cancellation);
        }
        jobs::SchedulerClient client(*jobs);
        return evaluate(entities, simulation_tick, fixed_dt_seconds, &client, cancellation);
    }

    [[nodiscard]] AnimationEvaluationHandle evaluateAsync(
        AnimationWorkSet work, jobs::SchedulerClient& jobs,
        jobs::CancelToken cancellation = {}, PresentationBudget budget = {});

    [[nodiscard]] AnimationEvaluationHandle evaluateAsync(
        AnimationWorkSet work, jobs::CancelToken cancellation = {},
        PresentationBudget budget = {}) {
        return !jobs_.valid()
                   ? AnimationEvaluationHandle{}
                   : implementation_.evaluateAsync(std::move(work), jobs_, cancellation,
                                                   budget);
    }

    [[nodiscard]] const AnimationSnapshot& currentSnapshot() const noexcept {
        return implementation_.currentSnapshot();
    }
    [[nodiscard]] const AnimationEvaluationStats& lastStats() const noexcept {
        return implementation_.lastStats();
    }

private:
    explicit PresentationAnimation(AnimationSystem implementation) noexcept
        : implementation_(std::move(implementation)) {}

    AnimationSystem implementation_;
    jobs::SchedulerClient jobs_{};
};

} // namespace genomes::infantry
