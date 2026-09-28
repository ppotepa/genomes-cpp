#pragma once

#include <genomes/jobs/JobSystem.hpp>
#include <genomes/proc/SeedPath.hpp>

namespace genomes::proc {

struct GenerationContext final {
    SeedPath seed_path{};
    jobs::JobSystem& jobs;

    [[nodiscard]] bool cancellationRequested() const noexcept {
        return jobs.isCancellationRequested();
    }
};

} // namespace genomes::proc
