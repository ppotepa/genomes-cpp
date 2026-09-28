#include <genomes/combat/VisibilityBatch.hpp>

#include <cassert>
#include <optional>
#include <utility>
#include <vector>

namespace {

using namespace genomes;

world::QueryRegion makeRegion(world::WorldId world_id,
                              std::vector<world::QueryCandidate> candidates) {
    const world::RegionCoord coordinate{0, 0, 0};
    const world::RegionId id = world::regionId(world_id, coordinate);
    return {coordinate, id, 3U, true, std::move(candidates), nullptr, nullptr};
}

class FakeDispatcher final : public combat::VisibilityBatchDispatcher {
public:
    bool fail_submit{false};
    combat::VisibilityGpuCompletion completion{};
    std::uint64_t submitted_revision{0U};
    std::size_t submit_count{0U};
    std::size_t await_count{0U};
    std::size_t release_count{0U};

    [[nodiscard]] foundation::Result<combat::VisibilityBatchToken, foundation::Error> submit(
        std::span<const combat::LOSRequest> requests, std::uint64_t world_revision) override {
        if (fail_submit) {
            return foundation::Result<combat::VisibilityBatchToken, foundation::Error>::failure(
                {foundation::ErrorCode::Unsupported, "fake GPU unavailable"});
        }
        ++submit_count;
        submitted_revision = world_revision;
        completion.token = 7U;
        completion.results.resize(requests.size());
        return foundation::Result<combat::VisibilityBatchToken, foundation::Error>::success(7U);
    }

    [[nodiscard]] foundation::Result<combat::VisibilityGpuCompletion, foundation::Error> poll(
        combat::VisibilityBatchToken token) override {
        completion.token = token;
        return foundation::Result<combat::VisibilityGpuCompletion, foundation::Error>::success(
            completion);
    }

    [[nodiscard]] foundation::Result<combat::VisibilityGpuCompletion, foundation::Error> await(
        combat::VisibilityBatchToken token) override {
        ++await_count;
        return poll(token);
    }

    void release(combat::VisibilityBatchToken) noexcept override { ++release_count; }
};

combat::VisibilityGpuResult gpuResult(bool visible, bool ambiguous, std::uint64_t revision,
                                      std::optional<foundation::StableId> blocker = {}) {
    combat::VisibilityGpuResult result{};
    result.visible = visible;
    result.ambiguous = ambiguous;
    result.completeness = world::QueryCompleteness::Complete;
    result.blocker = blocker;
    result.hit_distance = visible ? 10.0F : 4.0F;
    result.world_revision = revision;
    return result;
}

} // namespace

int main() {
    using namespace genomes;
    const world::WorldId world_id{3U};
    const world::RegionId region_id = world::regionId(world_id, {0, 0, 0});
    const world::QueryCandidate target{99U, {{-0.5F, 0.5F, 9.0F}, {0.5F, 1.5F, 9.8F}},
                                       world::QuerySourceKind::Dynamic, region_id, 3U};
    const world::QueryCandidate blocker{42U, {{-0.5F, 0.5F, 4.0F}, {0.5F, 1.5F, 5.0F}},
                                         world::QuerySourceKind::Static, region_id, 3U};
    auto created = world::WorldQuerySnapshot::create(
        world_id, {100.0}, {makeRegion(world_id, {target, blocker})});
    assert(created);
    const auto& snapshot = created.value();
    const combat::LOSRequest request{1U, 99U, {0.0F, 1.0F, 0.0F}, {0.0F, 1.0F, 10.0F}};
    const std::vector<combat::LOSRequest> requests{request, request};

    const auto scalar = combat::LineOfSight::query(snapshot, requests);
    const auto cpu = combat::VisibilityBatch::queryCpu(snapshot, requests);
    assert(scalar && cpu && scalar.value().size() == cpu.value().size());
    assert(scalar.value()[0].visible == cpu.value()[0].visible);
    assert(!cpu.value()[0].visible && cpu.value()[0].blocker.value() == 42U);

    combat::VisibilityBatchOptions options{};
    options.minimum_gpu_batch = 1U;
    const auto unavailable = combat::VisibilityBatch::execute(snapshot, requests, nullptr, options);
    assert(unavailable && !unavailable.value().gpu_submitted &&
           unavailable.value().source == combat::VisibilityBatchSource::CpuWorldQuery);

    FakeDispatcher ready;
    ready.completion.state = combat::VisibilityBatchState::Ready;
    ready.completion.world_revision = combat::VisibilityBatch::worldRevision(snapshot);
    ready.completion.results = {
        gpuResult(true, false, ready.completion.world_revision),
        gpuResult(true, false, ready.completion.world_revision)};
    const auto gpu = combat::VisibilityBatch::execute(snapshot, requests, &ready, options);
    assert(gpu && gpu.value().source == combat::VisibilityBatchSource::GpuAdvisory &&
           gpu.value().results[0].visible && ready.release_count == 1U);

    FakeDispatcher ambiguous;
    ambiguous.completion.state = combat::VisibilityBatchState::Ready;
    ambiguous.completion.world_revision = combat::VisibilityBatch::worldRevision(snapshot);
    ambiguous.completion.results = {
        gpuResult(true, true, ambiguous.completion.world_revision),
        gpuResult(true, false, ambiguous.completion.world_revision)};
    const auto confirmed = combat::VisibilityBatch::execute(snapshot, requests, &ambiguous, options);
    assert(confirmed && confirmed.value().source == combat::VisibilityBatchSource::CpuConfirmation &&
           confirmed.value().cpu_confirmed == 1U && !confirmed.value().results[0].visible &&
           confirmed.value().results[0].blocker.value() == 42U);

    FakeDispatcher pending;
    pending.completion.state = combat::VisibilityBatchState::Pending;
    const auto pending_result = combat::VisibilityBatch::execute(snapshot, requests, &pending, options);
    assert(pending_result && pending_result.value().gpu_state == combat::VisibilityBatchState::Pending &&
           !pending_result.value().stale_gpu_result && !pending_result.value().results[0].visible);

    FakeDispatcher stale;
    stale.completion.state = combat::VisibilityBatchState::Ready;
    stale.completion.world_revision = 2U;
    stale.completion.results = {gpuResult(true, false, 2U), gpuResult(true, false, 2U)};
    const auto stale_result = combat::VisibilityBatch::execute(snapshot, requests, &stale, options);
    assert(stale_result && stale_result.value().stale_gpu_result &&
           stale_result.value().source == combat::VisibilityBatchSource::CpuWorldQuery &&
           !stale_result.value().results[0].visible);

    FakeDispatcher awaited;
    awaited.completion.state = combat::VisibilityBatchState::Ready;
    awaited.completion.world_revision = combat::VisibilityBatch::worldRevision(snapshot);
    awaited.completion.results = {
        gpuResult(true, false, awaited.completion.world_revision),
        gpuResult(true, false, awaited.completion.world_revision)};
    options.latency = combat::VisibilityLatencyPolicy::SameTickAwaited;
    const auto same_tick = combat::VisibilityBatch::execute(snapshot, requests, &awaited, options);
    assert(same_tick && awaited.await_count == 1U && same_tick.value().results[0].visible);

    FakeDispatcher unavailable_gpu;
    unavailable_gpu.fail_submit = true;
    options.latency = combat::VisibilityLatencyPolicy::SelectiveCpuConfirmation;
    const auto fallback = combat::VisibilityBatch::execute(snapshot, requests, &unavailable_gpu, options);
    assert(fallback && fallback.value().gpu_state == combat::VisibilityBatchState::Failed &&
           !fallback.value().results[0].visible);
    return 0;
}
