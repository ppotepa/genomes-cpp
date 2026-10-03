#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/proc/ArtifactCache.hpp>
#include <genomes/proc/ProceduralRuntime.hpp>
#include <genomes/world/WorldPlan.hpp>

#include <memory>
#include <mutex>
#include <optional>

namespace genomes::world {

[[nodiscard]] foundation::Result<void, foundation::Error> registerWorldGenerator(
    proc::GeneratorRegistry::Builder& builder);

class WorldGenerationTask final {
public:
    WorldGenerationTask() = default;

    [[nodiscard]] bool valid() const noexcept { return ticket_.valid(); }
    [[nodiscard]] bool ready() const noexcept { return ticket_.complete(); }
    [[nodiscard]] bool failed() const noexcept;
    [[nodiscard]] foundation::Error error() const noexcept;
    void cancel() noexcept { ticket_.cancel(); }
    void wait() const noexcept { ticket_.wait(); }
    [[nodiscard]] std::optional<WorldPlan> take_result();

private:
    struct ConsumptionState final {
        std::mutex mutex;
        bool consumed{false};
    };

    friend class WorldGenerationService;
    explicit WorldGenerationTask(proc::GenerationTicket<WorldPlan> ticket) noexcept
        : ticket_(std::move(ticket)), consumption_(std::make_shared<ConsumptionState>()) {}

    proc::GenerationTicket<WorldPlan> ticket_;
    std::shared_ptr<ConsumptionState> consumption_;
};

class WorldGenerationService final {
public:
    explicit WorldGenerationService(jobs::JobSystem& jobs,
                                    std::shared_ptr<proc::ArtifactCache> cache = {});
    WorldGenerationService(jobs::JobSystem& jobs,
                           std::shared_ptr<proc::ArtifactCache> cache,
                           proc::GeneratorRegistry registry);

    [[nodiscard]] WorldGenerationTask submit(
        const WorldGenerationRequest& request,
        proc::GenerationChannel* channel = nullptr);
    [[nodiscard]] const proc::GeneratorRegistry& registry() const noexcept { return registry_; }

private:
    [[nodiscard]] static proc::GeneratorRegistry makeRegistry();

    std::shared_ptr<proc::ArtifactCache> cache_;
    proc::GeneratorRegistry registry_;
    proc::ProceduralRuntime runtime_;
};

} // namespace genomes::world
