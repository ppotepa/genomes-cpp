#pragma once

#include <genomes/compute/ComputeService.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

namespace genomes::compute {

// KernelId is a semantic identifier.  Backend selection must never depend on
// shader names or backend-native handles.
enum class KernelId : std::uint16_t {
    Generic = 0,
    ThreatInfluence,
    VisibilityBatch,
    FlowField,
    FieldAtlas,
};

using ComputeKernelId = KernelId;

enum class ComputeCapability : std::uint32_t {
    None = 0U,
    CpuReference = 1U << 0U,
    DiligentCompute = 1U << 1U,
    CudaCompute = 1U << 2U,
    CudaDeviceMatch = 1U << 3U,
};

[[nodiscard]] constexpr ComputeCapability operator|(ComputeCapability lhs,
                                                     ComputeCapability rhs) noexcept {
    return static_cast<ComputeCapability>(static_cast<std::uint32_t>(lhs) |
                                          static_cast<std::uint32_t>(rhs));
}

[[nodiscard]] constexpr ComputeCapability operator&(ComputeCapability lhs,
                                                     ComputeCapability rhs) noexcept {
    return static_cast<ComputeCapability>(static_cast<std::uint32_t>(lhs) &
                                          static_cast<std::uint32_t>(rhs));
}

// This is a frozen, backend-neutral snapshot taken at service/bootstrap time.
// cuda_compute means that the optional backend is built, enabled and has a
// usable device.  cuda_device_match separately gates interop/device identity;
// no CUDA type is needed to represent either fact.
struct HardwareCapabilities final {
    bool cpu_reference{true};
    bool diligent_compute{false};
    bool cuda_compute{false};
    bool cuda_device_match{false};

    [[nodiscard]] bool supports(ComputeBackend backend) const noexcept;
    [[nodiscard]] bool has(ComputeCapability required) const noexcept;

    [[nodiscard]] static HardwareCapabilities cpuOnly() noexcept {
        return {};
    }

    [[nodiscard]] static HardwareCapabilities diligentOnly() noexcept {
        HardwareCapabilities capabilities{};
        capabilities.diligent_compute = true;
        return capabilities;
    }

    [[nodiscard]] static HardwareCapabilities cuda(bool with_diligent = true) noexcept {
        HardwareCapabilities capabilities{};
        capabilities.diligent_compute = with_diligent;
        capabilities.cuda_compute = true;
        capabilities.cuda_device_match = true;
        return capabilities;
    }
};

struct BackendPolicyEntry final {
    ComputeBackend backend{ComputeBackend::Cpu};
    std::size_t minimum_workload{0U};
    ComputeCapability required_capabilities{ComputeCapability::CpuReference};
    bool correctness_approved{false};
    bool fallback_safe{true};
};

struct KernelBackendPolicy final {
    KernelId kernel{KernelId::Generic};
    std::span<const BackendPolicyEntry> backends{};
    ComputeBackend fallback{ComputeBackend::Cpu};
};

struct PerformanceThreshold final {
    KernelId kernel{KernelId::Generic};
    ComputeBackend backend{ComputeBackend::Cpu};
    std::size_t minimum_workload{0U};
    bool approved{false};
};

// The profile is intentionally broad and versioned.  It is not a machine
// secret or an online autotuner state, and can therefore be reviewed and
// shipped as configuration.
struct ComputePerformanceProfile final {
    std::string_view id{"default"};
    std::uint32_t version{1U};
    std::span<const PerformanceThreshold> thresholds{};

    [[nodiscard]] const PerformanceThreshold* threshold(
        KernelId kernel, ComputeBackend backend) const noexcept;
};

enum class SelectionReason : std::uint8_t {
    None,
    Policy,
    Forced,
    BelowCrossover,
    CapabilityUnavailable,
    ProfileRejected,
    CorrectnessNotApproved,
    ForcedUnsupported,
    ForcedFallback,
    RuntimeFailure,
    NoEligibleBackend,
};

using BackendSelectionReason = SelectionReason;

struct BackendSelection final {
    KernelId kernel{KernelId::Generic};
    ComputeBackend backend{ComputeBackend::Cpu};
    ComputeBackend requested_backend{ComputeBackend::Cpu};
    ComputeBackend failed_backend{ComputeBackend::Cpu};
    SelectionReason reason{SelectionReason::None};
    SelectionReason fallback_reason{SelectionReason::None};
    bool used_fallback{false};
};

struct BackendSelectionConfig final {
    // A forced backend bypasses crossover thresholds, but never bypasses
    // capability, profile approval or correctness gating.
    std::optional<ComputeBackend> forced_backend{};
    bool fallback_when_forced_unavailable{false};
};

struct BackendTelemetry final {
    KernelId kernel{KernelId::Generic};
    ComputeBackend backend{ComputeBackend::Cpu};
    std::size_t dispatch_size{0U};
    std::uint64_t latency_ns{0U};
    SelectionReason reason{SelectionReason::None};
    SelectionReason fallback_reason{SelectionReason::None};
    bool used_fallback{false};
};

using BackendTelemetrySink = std::function<void(const BackendTelemetry&)>;

class BackendSelector final {
public:
    [[nodiscard]] static std::span<const KernelBackendPolicy> defaultPolicies() noexcept;
    [[nodiscard]] static const ComputePerformanceProfile& defaultPerformanceProfile() noexcept;

    explicit BackendSelector(
        std::span<const KernelBackendPolicy> policies = defaultPolicies(),
        ComputePerformanceProfile profile = defaultPerformanceProfile(),
        BackendTelemetrySink telemetry = {});

    [[nodiscard]] std::span<const KernelBackendPolicy> policies() const noexcept {
        return policies_;
    }

    [[nodiscard]] const ComputePerformanceProfile& performanceProfile() const noexcept {
        return profile_;
    }

    [[nodiscard]] const KernelBackendPolicy* policy(KernelId kernel) const noexcept;

    [[nodiscard]] foundation::Result<BackendSelection, foundation::Error> select(
        KernelId kernel, std::size_t work_size,
        const HardwareCapabilities& capabilities) const;

    [[nodiscard]] foundation::Result<BackendSelection, foundation::Error> select(
        KernelId kernel, std::size_t work_size,
        const HardwareCapabilities& capabilities,
        const BackendSelectionConfig& config) const;

    // Selects the declared portable fallback after a backend failed before
    // committing output.  The failed backend is never silently retried.
    [[nodiscard]] foundation::Result<BackendSelection, foundation::Error>
    fallbackAfterRuntimeFailure(KernelId kernel, std::size_t work_size,
                                ComputeBackend failed_backend,
                                const HardwareCapabilities& capabilities) const;

    // Dispatch code calls this once it has an end-to-end latency measurement.
    // Selection itself is pure/read-only; telemetry is the only callback side
    // effect and is optional for headless callers.
    void recordTelemetry(const BackendSelection& selection, std::size_t dispatch_size,
                         std::uint64_t latency_ns) const;

private:
    [[nodiscard]] foundation::Result<BackendSelection, foundation::Error> selectInternal(
        KernelId kernel, std::size_t work_size,
        const HardwareCapabilities& capabilities,
        const BackendSelectionConfig& config,
        std::optional<ComputeBackend> excluded_backend) const;

    std::span<const KernelBackendPolicy> policies_{};
    ComputePerformanceProfile profile_{};
    BackendTelemetrySink telemetry_{};
};

[[nodiscard]] std::string_view toString(ComputeBackend backend) noexcept;
[[nodiscard]] std::string_view toString(KernelId kernel) noexcept;
[[nodiscard]] std::string_view toString(SelectionReason reason) noexcept;

} // namespace genomes::compute

