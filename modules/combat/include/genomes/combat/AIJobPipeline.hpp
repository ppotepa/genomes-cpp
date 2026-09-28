#pragma once

#include <genomes/combat/TacticalAI.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/jobs/JobSystem.hpp>

#include <atomic>
#include <cstddef>
#include <functional>
#include <span>
#include <vector>

namespace genomes::combat {

using AIBatchStage = std::function<foundation::Result<void, foundation::Error>(
    std::span<TacticalAIEntity>, foundation::SimulationTick)>;

struct AIJobPipelineConfig final {
    std::size_t batch_size{128U};
    bool parallel{true};
    AIBatchStage broadphase_stage{};
    AIBatchStage line_of_sight_stage{};
    AIBatchStage squad_stage{};

    [[nodiscard]] bool valid() const noexcept { return batch_size > 0U; }
};

struct AIJobPipelineStats final {
    std::size_t entity_count{0U};
    std::size_t batch_count{0U};
    std::size_t intent_count{0U};
    std::uint32_t worker_count{0U};
    bool canceled{false};
    bool parallel{false};
};

class AIJobPipeline final {
public:
    [[nodiscard]] foundation::Result<std::vector<AIIntent>, foundation::Error> evaluate(
        jobs::JobSystem& jobs, TacticalAISystem& model, std::span<TacticalAIEntity> entities,
        foundation::SimulationTick tick, AIJobPipelineConfig config = {},
        std::atomic_bool* cancel = nullptr);

    [[nodiscard]] const AIJobPipelineStats& stats() const noexcept { return stats_; }

private:
    AIJobPipelineStats stats_{};
};

} // namespace genomes::combat
