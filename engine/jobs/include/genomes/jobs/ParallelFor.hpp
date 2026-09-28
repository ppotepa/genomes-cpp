#pragma once

#include <genomes/jobs/BatchRange.hpp>
#include <genomes/jobs/JobSystem.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
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
    const std::size_t requested_jobs = workers * policy.oversubscription;
    const std::size_t target_jobs = policy.maximum_jobs == 0
                                        ? requested_jobs
                                        : std::max<std::size_t>(1, std::min(requested_jobs,
                                                                             policy.maximum_jobs));
    const std::size_t automatic = (count + target_jobs - 1U) / target_jobs;
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
    const std::size_t batch_count = (end - begin + grain_size - 1U) / grain_size;
    handles.reserve(batch_count);
    using Callback = std::decay_t<Function>;
    Callback callback = std::forward<Function>(function);
    for (std::size_t batch = 0; batch < batch_count; ++batch) {
        const std::size_t range_begin = begin + batch * grain_size;
        const std::size_t range_end = std::min(end, range_begin + grain_size);
        handles.push_back(system.submit(
            [range = BatchRange{range_begin, range_end, batch},
             callback](JobContext&) mutable {
                callback(range);
            }));
    }
    return handles;
}

template <class Function>
void parallelForAndWait(JobSystem& system,
                        std::size_t begin,
                        std::size_t end,
                        std::size_t grain_size,
                        Function&& function) {
    if (end <= begin || grain_size == 0) {
        return;
    }
    auto handles = parallelFor(system, begin, end, grain_size,
                               std::forward<Function>(function));
    for (const JobHandle& handle : handles) {
        system.wait(handle);
    }
}

} // namespace genomes::jobs
