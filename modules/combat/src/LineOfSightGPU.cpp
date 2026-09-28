#include <genomes/combat/VisibilityBatch.hpp>

#include <algorithm>
#include <utility>

namespace genomes::combat {

std::uint64_t VisibilityBatch::worldRevision(
    const world::WorldQuerySnapshot& snapshot) noexcept {
    std::uint64_t revision = 0U;
    for (const world::QueryRegion& region : snapshot.regions()) {
        revision = std::max(revision, region.revision);
    }
    return revision;
}

foundation::Result<std::vector<LOSResult>, foundation::Error> VisibilityBatch::queryCpu(
    const world::WorldQuerySnapshot& snapshot, std::span<const LOSRequest> requests) {
    return LineOfSight::query(snapshot, requests);
}

foundation::Result<VisibilityBatchReport, foundation::Error> VisibilityBatch::execute(
    const world::WorldQuerySnapshot& snapshot, std::span<const LOSRequest> requests,
    VisibilityBatchDispatcher* dispatcher, VisibilityBatchOptions options) {
    const auto cpu = queryCpu(snapshot, requests);
    if (!cpu) {
        return foundation::Result<VisibilityBatchReport, foundation::Error>::failure(cpu.error());
    }

    VisibilityBatchReport report{};
    report.results = cpu.value();
    report.world_revision = worldRevision(snapshot);

    // A partial WorldQuery snapshot is not a safe input to an optional proxy:
    // the CPU result is conservative and authoritative until the world is
    // complete again.
    const bool cpu_complete = std::all_of(
        report.results.begin(), report.results.end(), [](const LOSResult& result) {
            return result.completeness == world::QueryCompleteness::Complete;
        });
    if (!options.enable_gpu || dispatcher == nullptr ||
        requests.size() < options.minimum_gpu_batch || !cpu_complete) {
        report.gpu_state = VisibilityBatchState::Unavailable;
        return foundation::Result<VisibilityBatchReport, foundation::Error>::success(
            std::move(report));
    }

    const auto submitted = dispatcher->submit(requests, report.world_revision);
    if (!submitted) {
        report.gpu_state = VisibilityBatchState::Failed;
        return foundation::Result<VisibilityBatchReport, foundation::Error>::success(
            std::move(report));
    }
    report.gpu_submitted = true;
    const VisibilityBatchToken token = submitted.value();
    const auto completion = options.latency == VisibilityLatencyPolicy::SameTickAwaited
                                ? dispatcher->await(token)
                                : dispatcher->poll(token);
    if (!completion) {
        dispatcher->release(token);
        report.gpu_state = VisibilityBatchState::Failed;
        return foundation::Result<VisibilityBatchReport, foundation::Error>::success(
            std::move(report));
    }

    report.gpu_state = completion.value().state;
    const VisibilityGpuCompletion& gpu = completion.value();
    if (gpu.state == VisibilityBatchState::Stale) {
        report.gpu_state = VisibilityBatchState::Stale;
        report.stale_gpu_result = true;
        dispatcher->release(token);
        return foundation::Result<VisibilityBatchReport, foundation::Error>::success(
            std::move(report));
    }
    if (gpu.state != VisibilityBatchState::Ready) {
        dispatcher->release(token);
        return foundation::Result<VisibilityBatchReport, foundation::Error>::success(
            std::move(report));
    }

    const bool revision_match = gpu.world_revision == report.world_revision &&
                                 gpu.results.size() == requests.size() &&
                                 std::all_of(gpu.results.begin(), gpu.results.end(),
                                             [expected = report.world_revision](
                                                 const VisibilityGpuResult& result) {
                                                 return result.world_revision == expected;
                                             });
    if (!revision_match) {
        report.gpu_state = VisibilityBatchState::Stale;
        report.stale_gpu_result = true;
        dispatcher->release(token);
        return foundation::Result<VisibilityBatchReport, foundation::Error>::success(
            std::move(report));
    }

    bool used_gpu = false;
    for (std::size_t index = 0U; index < gpu.results.size(); ++index) {
        const VisibilityGpuResult& candidate = gpu.results[index];
        const bool complete = candidate.completeness == world::QueryCompleteness::Complete;
        const bool confirm = options.latency == VisibilityLatencyPolicy::SelectiveCpuConfirmation &&
                             (candidate.ambiguous || !complete);
        if (confirm) {
            ++report.cpu_confirmed;
            continue;
        }
        LOSResult& result = report.results[index];
        result.visible = candidate.visible;
        result.completeness = candidate.completeness;
        result.blocker = candidate.blocker;
        result.hit_distance = candidate.hit_distance;
        result.world_revision = candidate.world_revision;
        used_gpu = true;
    }
    if (used_gpu) {
        report.source = report.cpu_confirmed == 0U ? VisibilityBatchSource::GpuAdvisory
                                                    : VisibilityBatchSource::CpuConfirmation;
    }
    dispatcher->release(token);
    return foundation::Result<VisibilityBatchReport, foundation::Error>::success(
        std::move(report));
}

} // namespace genomes::combat
