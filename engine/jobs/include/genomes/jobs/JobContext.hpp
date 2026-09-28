#pragma once

#include <cstdint>

namespace genomes::jobs {

class JobSystem;

class JobContext final {
public:
    [[nodiscard]] std::uint32_t workerIndex() const noexcept {
        return worker_index_;
    }

    [[nodiscard]] bool isCancellationRequested() const noexcept;

    [[nodiscard]] JobSystem& system() const noexcept {
        return system_;
    }

private:
    friend class JobSystem;

    JobContext(JobSystem& system, std::uint32_t worker_index) noexcept
        : system_(system), worker_index_(worker_index) {}

    JobSystem& system_;
    std::uint32_t worker_index_{0};
};

} // namespace genomes::jobs
