#include <genomes/world/WorldGenerationTask.hpp>

#include <genomes/foundation/StableHash.hpp>

#include <exception>
#include <cstdint>
#include <memory>
#include <utility>

namespace genomes::world {

bool WorldGenerationTask::ready() const noexcept {
    if (!state_) {
        return true;
    }
    std::lock_guard lock(state_->mutex);
    return state_->complete;
}

bool WorldGenerationTask::failed() const noexcept {
    if (!state_) {
        return false;
    }
    std::lock_guard lock(state_->mutex);
    return state_->complete && state_->failed;
}

foundation::Error WorldGenerationTask::error() const noexcept {
    if (!state_) {
        return {foundation::ErrorCode::InvalidState, "invalid world generation task"};
    }
    std::lock_guard lock(state_->mutex);
    return state_->error;
}

void WorldGenerationTask::wait() const noexcept {
    handle_.wait();
}

std::optional<WorldPlan> WorldGenerationTask::take_result() {
    if (!state_) {
        return std::nullopt;
    }
    std::lock_guard lock(state_->mutex);
    if (!state_->complete || state_->failed || !state_->plan) {
        return std::nullopt;
    }
    std::optional<WorldPlan> result{std::move(state_->plan)};
    state_->plan.reset();
    return result;
}

WorldGenerationTask WorldGenerationService::submit(const WorldGenerationRequest& request) {
    auto state = std::make_shared<WorldGenerationTask::State>();
    proc::ArtifactKey cache_key{};
    if (cache_) {
        std::uint64_t input_hash = foundation::stableHashFloat(
            static_cast<float>(request.map_size_m));
        input_hash = foundation::stableHashCombine(input_hash,
                                                   foundation::stableHashFloat(request.vegetation));
        input_hash = foundation::stableHashCombine(input_hash,
                                                   foundation::stableHashFloat(request.buildings));
        input_hash = foundation::stableHashCombine(
            input_hash, foundation::stableHashFloat(request.fenced_parcels));
        input_hash = foundation::stableHashCombine(
            input_hash, static_cast<std::uint64_t>(request.hydrology_mode));
        input_hash = foundation::stableHashCombine(
            input_hash, foundation::stableHashFloat(request.river_probability));
        cache_key = {foundation::stable_id("world.plan"), WorldGeneratorVersion, request.seed,
                     input_hash};
        if (const auto cached = cache_->find<WorldPlan>(cache_key)) {
            std::lock_guard lock(state->mutex);
            state->plan = *cached;
            state->complete = true;
            return WorldGenerationTask(std::move(state), {});
        }
    }
    const WorldGenerationRequest copied_request = request;
    jobs::JobHandle handle = jobs_.submit(
        [state = state, copied_request, cache = cache_, cache_key](jobs::JobContext&) {
            try {
                auto generated = WorldGenerator::generate(copied_request);
                std::lock_guard lock(state->mutex);
                if (generated) {
                    if (cache) {
                        try {
                            const WorldPlan& plan = generated.value();
                            const std::size_t deep_bytes = sizeof(WorldPlan) +
                                plan.features.size() * sizeof(WorldFeature) +
                                plan.building_sites.size() * sizeof(BuildingSiteRequest) +
                                plan.city.parcels.size() * sizeof(CityParcel);
                            cache->store<WorldPlan>(
                                cache_key, std::make_shared<const WorldPlan>(generated.value()),
                                {deep_bytes});
                        } catch (...) {
                            // Cache exhaustion or allocation failure is a
                            // cache miss condition, not a world-generation
                            // failure. The generated plan remains authoritative.
                        }
                    }
                    state->plan = std::move(generated.value());
                } else {
                    state->failed = true;
                    state->error = generated.error();
                }
                state->complete = true;
            } catch (...) {
                std::lock_guard lock(state->mutex);
                state->failed = true;
                state->error = {foundation::ErrorCode::Internal,
                                "world generation task threw an exception"};
                state->complete = true;
            }
        });
    if (handle.wasCanceled()) {
        std::lock_guard lock(state->mutex);
        state->failed = true;
        state->error = {foundation::ErrorCode::InvalidState,
                        "world generation service is shutting down"};
        state->complete = true;
    }
    return WorldGenerationTask(std::move(state), std::move(handle));
}

} // namespace genomes::world
