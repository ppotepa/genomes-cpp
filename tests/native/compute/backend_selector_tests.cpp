#include <genomes/compute/BackendSelector.hpp>

#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    using namespace genomes::compute;

    BackendSelector selector;

    // Headless/non-GPU hosts always retain the CPU reference path.
    const auto cpu = selector.select(KernelId::VisibilityBatch, 32U,
                                     HardwareCapabilities::cpuOnly());
    assert(cpu);
    assert(cpu.value().backend == ComputeBackend::Cpu);
    assert(cpu.value().used_fallback);
    assert(cpu.value().fallback_reason == SelectionReason::CapabilityUnavailable);

    // A Diligent-capable AMD/Intel profile is selected only after crossover.
    const HardwareCapabilities diligent = HardwareCapabilities::diligentOnly();
    const auto small_diligent =
        selector.select(KernelId::VisibilityBatch, 2'047U, diligent);
    assert(small_diligent);
    assert(small_diligent.value().backend == ComputeBackend::Cpu);
    assert(small_diligent.value().fallback_reason == SelectionReason::BelowCrossover);
    const auto large_diligent =
        selector.select(KernelId::VisibilityBatch, 2'048U, diligent);
    assert(large_diligent);
    assert(large_diligent.value().backend == ComputeBackend::Diligent);
    assert(!large_diligent.value().used_fallback);

    // CUDA is gated by both availability and matched-device capability.
    const auto cuda = selector.select(KernelId::VisibilityBatch, 8'192U,
                                      HardwareCapabilities::cuda());
    assert(cuda);
    assert(cuda.value().backend == ComputeBackend::Cuda);
    const HardwareCapabilities unmatched_cuda{true, true, true, false};
    const auto unmatched = selector.select(KernelId::VisibilityBatch, 16'384U,
                                           unmatched_cuda);
    assert(unmatched);
    assert(unmatched.value().backend == ComputeBackend::Diligent);

    // A forced backend is explicit and bypasses crossover, but cannot bypass
    // capability/correctness gates.
    const BackendSelectionConfig force_cuda{ComputeBackend::Cuda, false};
    const auto forced = selector.select(KernelId::VisibilityBatch, 1U,
                                        HardwareCapabilities::cuda(), force_cuda);
    assert(forced);
    assert(forced.value().backend == ComputeBackend::Cuda);
    assert(forced.value().reason == SelectionReason::Forced);
    const auto forced_error = selector.select(KernelId::VisibilityBatch, 8'192U, diligent,
                                              force_cuda);
    assert(!forced_error);
    assert(forced_error.error().code == genomes::foundation::ErrorCode::Unsupported);

    const BackendSelectionConfig force_with_fallback{ComputeBackend::Cuda, true};
    const auto forced_fallback = selector.select(KernelId::VisibilityBatch, 8'192U, diligent,
                                                 force_with_fallback);
    assert(forced_fallback);
    assert(forced_fallback.value().backend == ComputeBackend::Cpu);
    assert(forced_fallback.value().reason == SelectionReason::ForcedFallback);
    assert(forced_fallback.value().fallback_reason == SelectionReason::ForcedUnsupported);

    // Runtime failure chooses the declared CPU fallback and records why it
    // happened; it does not silently retry another accelerator.
    const auto runtime_fallback = selector.fallbackAfterRuntimeFailure(
        KernelId::VisibilityBatch, 8'192U, ComputeBackend::Cuda,
        HardwareCapabilities::cuda());
    assert(runtime_fallback);
    assert(runtime_fallback.value().backend == ComputeBackend::Cpu);
    assert(runtime_fallback.value().failed_backend == ComputeBackend::Cuda);
    assert(runtime_fallback.value().reason == SelectionReason::RuntimeFailure);

    BackendTelemetry telemetry{};
    std::uint32_t telemetry_count = 0U;
    BackendSelector instrumented(
        BackendSelector::defaultPolicies(), BackendSelector::defaultPerformanceProfile(),
        [&telemetry, &telemetry_count](const BackendTelemetry& event) {
            telemetry = event;
            ++telemetry_count;
        });
    instrumented.recordTelemetry(runtime_fallback.value(), 8'192U, 42'000U);
    assert(telemetry_count == 1U);
    assert(telemetry.backend == ComputeBackend::Cpu);
    assert(telemetry.dispatch_size == 8'192U);
    assert(telemetry.latency_ns == 42'000U);
    assert(telemetry.fallback_reason == SelectionReason::RuntimeFailure);

    return 0;
}
