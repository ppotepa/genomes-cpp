#pragma once

#include <genomes/jobs/Cancellation.hpp>
#include <genomes/jobs/SchedulerTypes.hpp>

#include <cstdint>

namespace genomes::jobs {

class JobSystem;
class ScratchContext;

class JobContext final {
public:
    [[nodiscard]] std::uint32_t workerIndex() const noexcept {
        return worker_index_;
    }

    [[nodiscard]] bool isCancellationRequested() const noexcept;

    [[nodiscard]] ExecutionLane lane() const noexcept { return lane_; }
    [[nodiscard]] WorkClass workClass() const noexcept { return work_class_; }
    [[nodiscard]] JobId jobId() const noexcept { return job_id_; }
    [[nodiscard]] CancelToken cancellation() const noexcept { return cancellation_; }
    [[nodiscard]] ScratchContext& scratch() const noexcept { return scratch_; }

    [[nodiscard]] JobSystem& system() const noexcept {
        return system_;
    }

private:
    friend class JobSystem;

    JobContext(JobSystem& system,
               ScratchContext& scratch,
               std::uint32_t worker_index,
               ExecutionLane lane,
               WorkClass work_class,
               JobId job_id,
               CancelToken cancellation,
               CancelToken group_cancellation) noexcept
        : system_(system), scratch_(scratch), worker_index_(worker_index), lane_(lane),
          work_class_(work_class), job_id_(job_id), cancellation_(cancellation),
          group_cancellation_(group_cancellation) {}

    JobSystem& system_;
    ScratchContext& scratch_;
    std::uint32_t worker_index_{0};
    ExecutionLane lane_{ExecutionLane::Worker};
    WorkClass work_class_{WorkClass::General};
    JobId job_id_{0};
    CancelToken cancellation_{};
    CancelToken group_cancellation_{};
};

} // namespace genomes::jobs
