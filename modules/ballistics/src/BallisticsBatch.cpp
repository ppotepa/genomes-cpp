#include <genomes/ballistics/BallisticsBatch.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
#include <string_view>
#include <genomes/jobs/ParallelFor.hpp>

namespace genomes::ballistics {

namespace {

[[nodiscard]] foundation::Error error(foundation::ErrorCode code,
                                      std::string_view message) noexcept {
    return {code, message};
}

} // namespace

foundation::Result<BatchFlightOutput, foundation::Error> BallisticsBatch::integrate(
    std::span<const ProjectileState> projectiles,
    const AmmunitionCatalog& catalog,
    const FlightEnvironment& environment,
    float seconds,
    jobs::JobSystem* jobs,
    std::size_t grain_size) {
    if (!catalog.frozen() || !environment.valid() || !std::isfinite(seconds) || seconds <= 0.0F) {
        return foundation::Result<BatchFlightOutput, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument, "invalid ballistic batch input"));
    }

    BatchFlightOutput output{};
    output.steps.resize(projectiles.size());
    output.status.assign(projectiles.size(), BatchFlightStatus::InvalidState);
    const auto process = [&projectiles, &catalog, &environment, seconds, &output](
                             jobs::BatchRange range) {
        for (std::size_t index = range.begin; index < range.end; ++index) {
            const ProjectileState& projectile = projectiles[index];
            if (!projectile.valid()) {
                output.status[index] = BatchFlightStatus::InvalidState;
                continue;
            }
            const AmmunitionStrategy* strategy = catalog.strategy(projectile.strategy_id);
            if (strategy == nullptr) {
                output.status[index] = BatchFlightStatus::MissingStrategy;
                continue;
            }
            const FlightIntervalInput input{&projectile,
                                            strategy,
                                            &environment,
                                            seconds,
                                            projectile.diameter_m,
                                            projectile.fragment};
            const auto interval = FlightIntegrator::chooseInterval(input);
            if (!interval) {
                output.status[index] = BatchFlightStatus::InvalidInterval;
                continue;
            }
            const auto step = FlightIntegrator::integrate(input, interval.value());
            if (!step) {
                output.status[index] = BatchFlightStatus::InvalidInterval;
                continue;
            }
            output.steps[index] = step.value();
            output.status[index] = BatchFlightStatus::Integrated;
        }
    };

    if (projectiles.empty()) {
        return foundation::Result<BatchFlightOutput, foundation::Error>::success(std::move(output));
    }
    if (jobs == nullptr) {
        process({0, projectiles.size(), 0});
    } else {
        const std::size_t selected_grain = grain_size == 0
                                               ? jobs::chooseParallelGrain(0,
                                                                           projectiles.size(),
                                                                           *jobs)
                                               : grain_size;
        jobs::parallelForAndWait(*jobs, 0, projectiles.size(), selected_grain, process);
    }
    return foundation::Result<BatchFlightOutput, foundation::Error>::success(std::move(output));
}

foundation::Result<BatchFlightOutput, foundation::Error> BallisticsBatch::integrate(
    const ProjectileStorage& storage,
    const AmmunitionCatalog& catalog,
    const FlightEnvironment& environment,
    float seconds,
    jobs::JobSystem* jobs,
    std::size_t grain_size) {
    const std::vector<ProjectileState> states = storage.snapshot();
    return integrate(states, catalog, environment, seconds, jobs, grain_size);
}

std::vector<world::QuerySegmentResult> BallisticsBatch::querySegments(
    const world::WorldQuerySnapshot& snapshot,
    std::span<const world::QuerySegmentRequest> requests,
    bool stable_order) {
    return world::querySegments(snapshot, requests, stable_order);
}

void BallisticsBatch::stableSortEvents(std::vector<BallisticsBatchEvent>& events) noexcept {
    std::stable_sort(events.begin(), events.end(), [](const auto& left, const auto& right) {
        if (left.key < right.key) {
            return true;
        }
        if (right.key < left.key) {
            return false;
        }
        if (left.kind != right.kind) {
            return left.kind < right.kind;
        }
        return left.semantic_id < right.semantic_id;
    });
}

} // namespace genomes::ballistics
