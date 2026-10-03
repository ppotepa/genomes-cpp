#include <genomes/infantry/PresentationAnimation.hpp>

namespace genomes::infantry {

foundation::Result<PresentationAnimation, foundation::Error>
PresentationAnimation::create(std::size_t chunk_size) {
    auto implementation = AnimationSystem::create(chunk_size);
    if (!implementation) {
        return foundation::Result<PresentationAnimation, foundation::Error>::failure(
            implementation.error());
    }
    return foundation::Result<PresentationAnimation, foundation::Error>::success(
        PresentationAnimation{std::move(implementation.value())});
}

foundation::Result<void, foundation::Error> PresentationAnimation::evaluate(
    std::span<AnimationEntity> entities, std::uint64_t simulation_tick,
    float fixed_dt_seconds, jobs::JobSystem* jobs, jobs::CancelToken cancellation) {
    return implementation_.evaluate(entities, simulation_tick, fixed_dt_seconds,
                                    jobs, cancellation);
}

AnimationEvaluationHandle PresentationAnimation::evaluateAsync(
    AnimationWorkSet work, jobs::JobSystem& jobs, jobs::CancelToken cancellation,
    PresentationBudget budget) {
    return implementation_.evaluateAsync(std::move(work), jobs, cancellation, budget);
}

} // namespace genomes::infantry
