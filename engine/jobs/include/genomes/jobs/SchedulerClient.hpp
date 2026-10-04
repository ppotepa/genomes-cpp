#pragma once

#include <genomes/jobs/JobGroup.hpp>
#include <genomes/jobs/JobHandle.hpp>
#include <genomes/jobs/SchedulerTypes.hpp>

#include <cstdint>
#include <functional>

namespace genomes::jobs {

class JobGraph;
class JobSystem;

// Non-owning execution capability. Domain/runtime code may schedule and
// observe work, but cannot shut down, reconfigure or pump owner-affinity
// queues. JobSystem ownership stays in the composition root.
class SchedulerClient final {
public:
    using JobFunction = std::function<void(JobContext&)>;

    SchedulerClient() = default;
    explicit SchedulerClient(JobSystem& system) noexcept : system_(&system) {}

    [[nodiscard]] bool valid() const noexcept { return system_ != nullptr; }
    [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

    [[nodiscard]] JobHandle submit(JobFunction function, JobOptions options = {}) const;
    void wait(const JobHandle& handle) const noexcept;
    void wait(const JobCompletion& completion) const noexcept;
    [[nodiscard]] JobCompletion start(const JobGraph& graph) const;

    [[nodiscard]] std::uint32_t workerCount() const noexcept;
    [[nodiscard]] SchedulerMode mode() const noexcept;
    [[nodiscard]] bool isWorkerThread() const noexcept;
    [[nodiscard]] bool isCancellationRequested() const noexcept;
    [[nodiscard]] SchedulerTelemetry telemetry() const noexcept;

private:
    JobSystem* system_{nullptr};
};

} // namespace genomes::jobs
