#pragma once

#include <genomes/infantry/AnimationSystem.hpp>

#include <utility>

namespace genomes::infantry {

// Presentation boundary owned by the infantry module. Game scenes can request
// animation work and consume immutable pose snapshots without depending on
// the concrete evaluator implementation or its scheduling internals.
class PresentationAnimation final {
public:
    [[nodiscard]] static foundation::Result<PresentationAnimation, foundation::Error>
    create(std::size_t chunk_size = 64U);

    [[nodiscard]] foundation::Result<void, foundation::Error> evaluate(
        std::span<AnimationEntity> entities, std::uint64_t simulation_tick,
        float fixed_dt_seconds, jobs::JobSystem* jobs = nullptr,
        jobs::CancelToken cancellation = {});

    [[nodiscard]] foundation::Result<void, foundation::Error> evaluate(
        AnimationWorkSet work, jobs::JobSystem* jobs = nullptr,
        jobs::CancelToken cancellation = {}) {
        return evaluate(std::span<AnimationEntity>(work.entities), work.simulation_tick,
                        work.fixed_dt_seconds, jobs, cancellation);
    }

    [[nodiscard]] AnimationEvaluationHandle evaluateAsync(
        AnimationWorkSet work, jobs::JobSystem& jobs,
        jobs::CancelToken cancellation = {}, PresentationBudget budget = {});

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
};

} // namespace genomes::infantry
