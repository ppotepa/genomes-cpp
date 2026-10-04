#pragma once

#include <genomes/jobs/Cancellation.hpp>
#include <genomes/jobs/JobContext.hpp>
#include <genomes/jobs/ScratchContext.hpp>
#include <genomes/proc/SeedPath.hpp>

namespace genomes::proc {

class ArtifactReader;
class GenerationDiagnostics;

class GenerationContext final {
public:
    explicit GenerationContext(SeedPath seed_path,
                               jobs::JobContext* job = nullptr,
                               jobs::CancelToken cancellation = {},
                               jobs::CancelToken superseded = {},
                               ArtifactReader* artifacts = nullptr,
                               GenerationDiagnostics* diagnostics = nullptr,
                               jobs::ScratchContext* inline_scratch = nullptr) noexcept
        : seed_path(seed_path), job_(job), cancellation_(cancellation),
          superseded_(superseded), artifacts_(artifacts), diagnostics_(diagnostics),
          inline_scratch_(inline_scratch) {}

    [[nodiscard]] bool cancellationRequested() const noexcept {
        return cancellation_.isCancellationRequested() ||
               superseded_.isCancellationRequested() ||
               (job_ != nullptr && job_->isCancellationRequested());
    }

    [[nodiscard]] jobs::ScratchContext* scratch() const noexcept {
        return job_ == nullptr ? inline_scratch_ : &job_->scratch();
    }
    [[nodiscard]] jobs::JobContext* job() const noexcept { return job_; }
    [[nodiscard]] jobs::CancelToken cancellationToken() const noexcept {
        return cancellation_;
    }
    [[nodiscard]] jobs::CancelToken supersededToken() const noexcept {
        return superseded_;
    }
    [[nodiscard]] ArtifactReader* artifacts() const noexcept { return artifacts_; }
    [[nodiscard]] GenerationDiagnostics* diagnostics() const noexcept { return diagnostics_; }

    SeedPath seed_path{};

private:
    jobs::JobContext* job_{nullptr};
    jobs::CancelToken cancellation_{};
    jobs::CancelToken superseded_{};
    ArtifactReader* artifacts_{nullptr};
    GenerationDiagnostics* diagnostics_{nullptr};
    jobs::ScratchContext* inline_scratch_{nullptr};
};

} // namespace genomes::proc
