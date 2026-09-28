#pragma once

#include <genomes/combat/LineOfSight.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace genomes::combat {

// The dispatcher is deliberately expressed in semantic data only.  A render
// backend may implement it with a compute buffer, a ray query, or no GPU at
// all; combat never receives a backend-native handle.
using VisibilityBatchToken = std::uint64_t;

enum class VisibilityLatencyPolicy : std::uint8_t {
    // The dispatcher may synchronize at an explicit queue barrier.  It must
    // not spin while waiting.  A pending/failed result uses CPU LOS.
    SameTickAwaited,
    // A completion can be consumed without waiting.  A completion from a
    // different world revision is never compatible and uses CPU LOS.
    PreviousCompatibleRevision,
    // Use GPU results that are complete and unambiguous; CPU-confirm the
    // remaining entries.  This is the default for an optional accelerator.
    SelectiveCpuConfirmation,
};

enum class VisibilityBatchState : std::uint8_t {
    Pending,
    Ready,
    Stale,
    Unavailable,
    Failed,
};

enum class VisibilityBatchSource : std::uint8_t {
    CpuWorldQuery,
    GpuAdvisory,
    CpuConfirmation,
};

struct VisibilityGpuResult final {
    bool visible{false};
    bool ambiguous{true};
    world::QueryCompleteness completeness{world::QueryCompleteness::Missing};
    std::optional<foundation::StableId> blocker{};
    float hit_distance{0.0F};
    std::uint64_t world_revision{0U};
};

struct VisibilityGpuCompletion final {
    VisibilityBatchToken token{0U};
    VisibilityBatchState state{VisibilityBatchState::Pending};
    std::uint64_t world_revision{0U};
    std::vector<VisibilityGpuResult> results{};
};

// Optional GPU integration point.  poll() is non-blocking.  await() is the
// only operation allowed to cross a same-tick barrier and has a conservative
// poll() default for backends that do not expose a blocking fence operation.
class VisibilityBatchDispatcher {
public:
    virtual ~VisibilityBatchDispatcher() = default;

    [[nodiscard]] virtual foundation::Result<VisibilityBatchToken, foundation::Error> submit(
        std::span<const LOSRequest> requests, std::uint64_t world_revision) = 0;

    [[nodiscard]] virtual foundation::Result<VisibilityGpuCompletion, foundation::Error> poll(
        VisibilityBatchToken token) = 0;

    [[nodiscard]] virtual foundation::Result<VisibilityGpuCompletion, foundation::Error> await(
        VisibilityBatchToken token) {
        return poll(token);
    }

    virtual void release(VisibilityBatchToken) noexcept = 0;
};

struct VisibilityBatchOptions final {
    VisibilityLatencyPolicy latency{VisibilityLatencyPolicy::SelectiveCpuConfirmation};
    // The default intentionally keeps small AI batches on the CPU.  A GPU
    // backend can lower this after end-to-end upload/dispatch/readback tests.
    std::size_t minimum_gpu_batch{4096U};
    bool enable_gpu{true};
};

struct VisibilityBatchReport final {
    std::vector<LOSResult> results{};
    VisibilityBatchSource source{VisibilityBatchSource::CpuWorldQuery};
    VisibilityBatchState gpu_state{VisibilityBatchState::Unavailable};
    std::uint64_t world_revision{0U};
    std::size_t cpu_confirmed{0U};
    bool gpu_submitted{false};
    bool stale_gpu_result{false};
};

class VisibilityBatch final {
public:
    [[nodiscard]] static std::uint64_t worldRevision(
        const world::WorldQuerySnapshot&) noexcept;

    // This is the deterministic semantic reference path.  Results retain
    // input order and are computed from the immutable WorldQuery snapshot.
    [[nodiscard]] static foundation::Result<std::vector<LOSResult>, foundation::Error> queryCpu(
        const world::WorldQuerySnapshot&, std::span<const LOSRequest>);

    // CPU LOS is always evaluated first.  A dispatcher can replace only
    // complete, revision-matching, non-ambiguous entries according to the
    // selected latency policy; every other case remains CPU-authoritative.
    [[nodiscard]] static foundation::Result<VisibilityBatchReport, foundation::Error> execute(
        const world::WorldQuerySnapshot&, std::span<const LOSRequest>,
        VisibilityBatchDispatcher* dispatcher = nullptr,
        VisibilityBatchOptions options = {});
};

} // namespace genomes::combat
