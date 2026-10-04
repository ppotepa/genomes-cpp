#pragma once

#include <genomes/jobs/BatchRange.hpp>
#include <genomes/jobs/JobSystem.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace genomes::jobs {

struct ParallelForPolicy final {
    std::size_t grain_size{0};
    std::size_t minimum_grain{1};
    std::size_t maximum_jobs{0};
    std::size_t oversubscription{4};

    [[nodiscard]] bool valid() const noexcept {
        return minimum_grain > 0 && oversubscription > 0;
    }
};

[[nodiscard]] inline std::size_t chooseParallelGrain(std::size_t begin,
                                                     std::size_t end,
                                                     const JobSystem& system,
                                                     ParallelForPolicy policy = {}) noexcept {
    if (end <= begin || !policy.valid()) {
        return 1;
    }
    if (policy.grain_size > 0) {
        return std::max(policy.minimum_grain, policy.grain_size);
    }
    const std::size_t count = end - begin;
    const std::size_t workers = std::max<std::size_t>(1, system.workerCount() + 1U);
    const std::size_t requested_jobs =
        workers > std::numeric_limits<std::size_t>::max() / policy.oversubscription
            ? std::numeric_limits<std::size_t>::max()
            : workers * policy.oversubscription;
    const std::size_t target_jobs = policy.maximum_jobs == 0
                                        ? requested_jobs
                                        : std::max<std::size_t>(1, std::min(requested_jobs,
                                                                             policy.maximum_jobs));
    const std::size_t automatic =
        count / target_jobs + (count % target_jobs != 0U ? 1U : 0U);
    return std::max(policy.minimum_grain, automatic);
}

// Partitions [begin,end) into stable, non-overlapping ranges.  Completion order
// is intentionally not part of the contract; batch_index is the deterministic
// identity callers must use for stable merge/reduction.
template <class Function>
[[nodiscard]] std::vector<JobHandle> parallelFor(JobSystem& system,
                                                  std::size_t begin,
                                                  std::size_t end,
                                                  std::size_t grain_size,
                                                  Function&& function) {
    std::vector<JobHandle> handles;
    if (end <= begin || grain_size == 0) {
        return handles;
    }
    const std::size_t count = end - begin;
    const std::size_t batch_count = count / grain_size + (count % grain_size != 0U ? 1U : 0U);
    handles.reserve(batch_count);
    using Callback = std::decay_t<Function>;
    Callback callback = std::forward<Function>(function);
    for (std::size_t batch = 0; batch < batch_count; ++batch) {
        const std::size_t range_begin = begin + batch * grain_size;
        const std::size_t range_end = range_begin + std::min(end - range_begin, grain_size);
        handles.push_back(system.submit(
            [range = BatchRange{range_begin, range_end, batch},
             callback](JobContext&) mutable {
                callback(range);
            }));
    }
    return handles;
}

template <class Function>
[[nodiscard]] std::vector<JobHandle> parallelFor(JobGroup& group,
                                                  std::size_t begin,
                                                  std::size_t end,
                                                  std::size_t grain_size,
                                                  Function&& function,
                                                  JobOptions options = {}) {
    std::vector<JobHandle> handles;
    if (end <= begin || grain_size == 0) {
        return handles;
    }
    const std::size_t count = end - begin;
    const std::size_t batch_count = count / grain_size + (count % grain_size != 0U ? 1U : 0U);
    handles.reserve(batch_count);
    using Callback = std::decay_t<Function>;
    Callback callback = std::forward<Function>(function);
    for (std::size_t batch = 0; batch < batch_count; ++batch) {
        const std::size_t range_begin = begin + batch * grain_size;
        const std::size_t range_end = range_begin + std::min(end - range_begin, grain_size);
        handles.push_back(group.submit(
            [range = BatchRange{range_begin, range_end, batch}, callback](JobContext&) mutable {
                callback(range);
            },
            options));
    }
    return handles;
}

template <class Function>
[[nodiscard]] bool parallelForAndWait(JobSystem& system,
                                      std::size_t begin,
                                      std::size_t end,
                                      std::size_t grain_size,
                                      Function&& function) {
    if (end <= begin || grain_size == 0) {
        return true;
    }
    if (system.isWorkerThread()) {
        // A worker must not synchronously wait for child batches. The caller
        // can submit the batches and attach a continuation to their group.
        return false;
    }
    JobGroup group(system);
    const auto handles = parallelFor(group, begin, end, grain_size,
                                     std::forward<Function>(function));
    group.wait();
    if (group.failed()) {
        return false;
    }
    return std::all_of(handles.begin(), handles.end(), [](const JobHandle& handle) {
        return !handle.wasCanceled();
    });
}

} // namespace genomes::jobs
