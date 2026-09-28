#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/jobs/JobHandle.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/proc/ArtifactCache.hpp>
#include <genomes/world/WorldPlan.hpp>

#include <memory>
#include <mutex>
#include <optional>

namespace genomes::world {

class WorldGenerationTask final {
public:
    WorldGenerationTask() = default;

    [[nodiscard]] bool valid() const noexcept {
        return static_cast<bool>(state_);
    }

    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] bool failed() const noexcept;
    [[nodiscard]] foundation::Error error() const noexcept;
    void wait() const noexcept;
    [[nodiscard]] std::optional<WorldPlan> take_result();

private:
    struct State final {
        mutable std::mutex mutex;
        bool complete{false};
        bool failed{false};
        foundation::Error error{};
        std::optional<WorldPlan> plan;
    };

    friend class WorldGenerationService;

    WorldGenerationTask(std::shared_ptr<State> state, jobs::JobHandle handle) noexcept
        : state_(std::move(state)), handle_(std::move(handle)) {}

    std::shared_ptr<State> state_;
    jobs::JobHandle handle_;
};

class WorldGenerationService final {
public:
    explicit WorldGenerationService(jobs::JobSystem& jobs,
                                    std::shared_ptr<proc::ArtifactCache> cache = {}) noexcept
        : jobs_{jobs}, cache_{cache} {}

    [[nodiscard]] WorldGenerationTask submit(const WorldGenerationRequest& request);

private:
    jobs::JobSystem& jobs_;
    std::shared_ptr<proc::ArtifactCache> cache_;
};

} // namespace genomes::world
