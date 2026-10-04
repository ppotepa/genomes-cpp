#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/proc/ArtifactCache.hpp>
#include <genomes/proc/GenerationClient.hpp>
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
                           proc::GeneratorRegistry registry,
                           proc::ProceduralRuntime* shared_runtime = nullptr);
    explicit WorldGenerationService(proc::GenerationClient generation);

    [[nodiscard]] WorldGenerationTask submit(
        const WorldGenerationRequest& request,
        proc::GenerationChannel* channel = nullptr);
    [[nodiscard]] const proc::GeneratorRegistry& registry() const noexcept {
        return generation_.registry();
    }
    [[nodiscard]] proc::GenerationClient generation() const noexcept { return generation_; }
    [[nodiscard]] bool ownsRuntime() const noexcept { return owned_runtime_ != nullptr; }

private:
    [[nodiscard]] static proc::GeneratorRegistry makeRegistry();

    std::shared_ptr<proc::ArtifactCache> cache_;
    proc::GeneratorRegistry registry_;
    std::unique_ptr<proc::ProceduralRuntime> owned_runtime_;
    proc::GenerationClient generation_{};
};

} // namespace genomes::world
