#include <genomes/combat/AIJobPipeline.hpp>
#include <genomes/jobs/ParallelFor.hpp>

#include <algorithm>
#include <atomic>
#include <iterator>
#include <utility>

namespace genomes::combat {

namespace {

[[nodiscard]] bool canceled(jobs::JobSystem& jobs, std::atomic_bool* token) noexcept {
    return jobs.isCancellationRequested() ||
           (token != nullptr && token->load(std::memory_order_acquire));
}

foundation::Result<void, foundation::Error> runStage(
    jobs::JobSystem& jobs, std::span<TacticalAIEntity> entities, foundation::SimulationTick tick,
    std::size_t batch_size, bool parallel, const AIBatchStage& stage, std::atomic_bool* cancel,
    const char* failure_message) {
    if (!stage) {
        return foundation::Result<void, foundation::Error>::success();
    }
    if (!parallel) {
        const auto result = stage(entities, tick);
        if (!result) {
            return foundation::Result<void, foundation::Error>::failure(result.error());
        }
        return foundation::Result<void, foundation::Error>::success();
    }
    const std::size_t batch_count = entities.size() / batch_size +
                                    (entities.size() % batch_size != 0U ? 1U : 0U);
    std::vector<foundation::Error> errors(batch_count);
    std::atomic_bool failed{false};
    const bool completed = jobs::parallelForAndWait(
        jobs, 0U, entities.size(), batch_size, [&](const jobs::BatchRange& range) {
            if (canceled(jobs, cancel)) {
                failed.store(true, std::memory_order_release);
                return;
            }
            const auto result = stage(entities.subspan(range.begin, range.size()), tick);
            if (!result) {
                errors[range.batch_index] = result.error();
                failed.store(true, std::memory_order_release);
            }
        });
    if (!completed) {
        failed.store(true, std::memory_order_release);
    }
    if (canceled(jobs, cancel)) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "AI pipeline canceled"});
    }
    if (failed.load(std::memory_order_acquire)) {
        for (const foundation::Error& error : errors) {
            if (error.code != foundation::ErrorCode::None) {
                return foundation::Result<void, foundation::Error>::failure(error);
            }
        }
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::Internal, failure_message});
    }
    return foundation::Result<void, foundation::Error>::success();
}

} // namespace

foundation::Result<std::vector<AIIntent>, foundation::Error> AIJobPipeline::evaluate(
    jobs::JobSystem& jobs, TacticalAISystem& model, std::span<TacticalAIEntity> entities,
    foundation::SimulationTick tick, AIJobPipelineConfig config, std::atomic_bool* cancel) {
    stats_ = {};
    stats_.entity_count = entities.size();
    stats_.worker_count = jobs.workerCount();
    stats_.parallel = config.parallel;
    if (!config.valid()) {
        return foundation::Result<std::vector<AIIntent>, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid AI pipeline configuration"});
    }
    if (canceled(jobs, cancel)) {
        stats_.canceled = true;
        return foundation::Result<std::vector<AIIntent>, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "AI pipeline canceled"});
    }
    if (entities.empty()) {
        return foundation::Result<std::vector<AIIntent>, foundation::Error>::success(
            std::vector<AIIntent>{});
    }
    const std::size_t batch_count = entities.size() / config.batch_size +
                                    (entities.size() % config.batch_size != 0U ? 1U : 0U);
    stats_.batch_count = config.parallel ? batch_count : 1U;
    for (const AIBatchStage* stage : {&config.broadphase_stage, &config.line_of_sight_stage,
                                      &config.squad_stage}) {
        const auto stage_result = runStage(jobs, entities, tick, config.batch_size, config.parallel,
                                           *stage, cancel, "AI pipeline stage failed");
        if (!stage_result) {
            stats_.canceled = stage_result.error().code == foundation::ErrorCode::InvalidState &&
                              canceled(jobs, cancel);
            return foundation::Result<std::vector<AIIntent>, foundation::Error>::failure(
                stage_result.error());
        }
    }
    if (!config.parallel) {
        const auto result = model.evaluate(entities, tick);
        if (!result) {
            return foundation::Result<std::vector<AIIntent>, foundation::Error>::failure(
                result.error());
        }
        stats_.intent_count = result.value().size();
        return foundation::Result<std::vector<AIIntent>, foundation::Error>::success(
            std::move(result.value()));
    }

    std::vector<std::vector<AIIntent>> batch_intents(batch_count);
    std::vector<foundation::Error> batch_errors(batch_count);
    std::atomic_bool failed{false};
    const bool completed = jobs::parallelForAndWait(
        jobs, 0U, entities.size(), config.batch_size, [&](const jobs::BatchRange& range) {
            if (canceled(jobs, cancel)) {
                failed.store(true, std::memory_order_release);
                return;
            }
            TacticalAISystem local_model = model;
            const auto result = local_model.evaluate(entities.subspan(range.begin, range.size()), tick);
            if (!result) {
                batch_errors[range.batch_index] = result.error();
                failed.store(true, std::memory_order_release);
                return;
            }
            batch_intents[range.batch_index] = std::move(result.value());
        });
    if (!completed) {
        failed.store(true, std::memory_order_release);
    }
    if (canceled(jobs, cancel)) {
        stats_.canceled = true;
        return foundation::Result<std::vector<AIIntent>, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidState, "AI pipeline canceled"});
    }
    if (failed.load(std::memory_order_acquire)) {
        for (const foundation::Error& error : batch_errors) {
            if (error.code != foundation::ErrorCode::None) {
                return foundation::Result<std::vector<AIIntent>, foundation::Error>::failure(error);
            }
        }
        return foundation::Result<std::vector<AIIntent>, foundation::Error>::failure(
            {foundation::ErrorCode::Internal, "AI model batch failed"});
    }

    std::vector<AIIntent> intents;
    for (std::vector<AIIntent>& batch : batch_intents) {
        intents.insert(intents.end(), std::make_move_iterator(batch.begin()),
                       std::make_move_iterator(batch.end()));
    }
    std::stable_sort(intents.begin(), intents.end(), [](const AIIntent& left, const AIIntent& right) {
        if (left.self != right.self) {
            return left.self < right.self;
        }
        return left.tick.value < right.tick.value;
    });
    stats_.intent_count = intents.size();
    return foundation::Result<std::vector<AIIntent>, foundation::Error>::success(std::move(intents));
}

} // namespace genomes::combat
