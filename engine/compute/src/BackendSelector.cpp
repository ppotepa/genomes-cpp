#include <genomes/compute/BackendSelector.hpp>

#include <utility>

namespace genomes::compute {
namespace {

constexpr BackendPolicyEntry kGenericBackends[] = {
    {ComputeBackend::Cuda, 8'192U,
     ComputeCapability::CudaCompute | ComputeCapability::CudaDeviceMatch, true, true},
    {ComputeBackend::Diligent, 2'048U, ComputeCapability::DiligentCompute, true, true},
    {ComputeBackend::Cpu, 0U, ComputeCapability::CpuReference, true, true},
};

constexpr BackendPolicyEntry kThreatBackends[] = {
    {ComputeBackend::Cuda, 16'384U,
     ComputeCapability::CudaCompute | ComputeCapability::CudaDeviceMatch, true, true},
    {ComputeBackend::Diligent, 4'096U, ComputeCapability::DiligentCompute, true, true},
    {ComputeBackend::Cpu, 0U, ComputeCapability::CpuReference, true, true},
};

constexpr BackendPolicyEntry kVisibilityBackends[] = {
    {ComputeBackend::Cuda, 8'192U,
     ComputeCapability::CudaCompute | ComputeCapability::CudaDeviceMatch, true, true},
    {ComputeBackend::Diligent, 2'048U, ComputeCapability::DiligentCompute, true, true},
    {ComputeBackend::Cpu, 0U, ComputeCapability::CpuReference, true, true},
};

constexpr BackendPolicyEntry kFlowBackends[] = {
    {ComputeBackend::Cuda, 16'384U,
     ComputeCapability::CudaCompute | ComputeCapability::CudaDeviceMatch, true, true},
    {ComputeBackend::Diligent, 8'192U, ComputeCapability::DiligentCompute, true, true},
    {ComputeBackend::Cpu, 0U, ComputeCapability::CpuReference, true, true},
};

constexpr BackendPolicyEntry kFieldBackends[] = {
    {ComputeBackend::Cuda, 8'192U,
     ComputeCapability::CudaCompute | ComputeCapability::CudaDeviceMatch, true, true},
    {ComputeBackend::Diligent, 4'096U, ComputeCapability::DiligentCompute, true, true},
    {ComputeBackend::Cpu, 0U, ComputeCapability::CpuReference, true, true},
};

constexpr KernelBackendPolicy kPolicies[] = {
    {KernelId::Generic, kGenericBackends, ComputeBackend::Cpu},
    {KernelId::ThreatInfluence, kThreatBackends, ComputeBackend::Cpu},
    {KernelId::VisibilityBatch, kVisibilityBackends, ComputeBackend::Cpu},
    {KernelId::FlowField, kFlowBackends, ComputeBackend::Cpu},
    {KernelId::FieldAtlas, kFieldBackends, ComputeBackend::Cpu},
};

constexpr PerformanceThreshold kThresholds[] = {
    {KernelId::Generic, ComputeBackend::Cuda, 8'192U, true},
    {KernelId::Generic, ComputeBackend::Diligent, 2'048U, true},
    {KernelId::ThreatInfluence, ComputeBackend::Cuda, 16'384U, true},
    {KernelId::ThreatInfluence, ComputeBackend::Diligent, 4'096U, true},
    {KernelId::VisibilityBatch, ComputeBackend::Cuda, 8'192U, true},
    {KernelId::VisibilityBatch, ComputeBackend::Diligent, 2'048U, true},
    {KernelId::FlowField, ComputeBackend::Cuda, 16'384U, true},
    {KernelId::FlowField, ComputeBackend::Diligent, 8'192U, true},
    {KernelId::FieldAtlas, ComputeBackend::Cuda, 8'192U, true},
    {KernelId::FieldAtlas, ComputeBackend::Diligent, 4'096U, true},
};

constexpr ComputePerformanceProfile kDefaultProfile{
    "default-conservative", 1U, kThresholds};

[[nodiscard]] foundation::Error error(foundation::ErrorCode code,
                                       std::string_view message) noexcept {
    return foundation::Error{code, message};
}

[[nodiscard]] bool entryIsApproved(const BackendPolicyEntry& entry,
                                   KernelId kernel,
                                   const ComputePerformanceProfile& profile) noexcept {
    if (!entry.correctness_approved) {
        return false;
    }
    const PerformanceThreshold* threshold = profile.threshold(kernel, entry.backend);
    return threshold == nullptr || threshold->approved;
}

[[nodiscard]] std::size_t effectiveMinimum(const BackendPolicyEntry& entry,
                                           KernelId kernel,
                                           const ComputePerformanceProfile& profile) noexcept {
    const PerformanceThreshold* threshold = profile.threshold(kernel, entry.backend);
    if (threshold == nullptr) {
        return entry.minimum_workload;
    }
    return threshold->minimum_workload > entry.minimum_workload
               ? threshold->minimum_workload
               : entry.minimum_workload;
}

} // namespace

bool HardwareCapabilities::supports(ComputeBackend backend) const noexcept {
    switch (backend) {
    case ComputeBackend::Cpu:
        return cpu_reference;
    case ComputeBackend::Diligent:
        return diligent_compute;
    case ComputeBackend::Cuda:
        return cuda_compute && cuda_device_match;
    }
    return false;
}

bool HardwareCapabilities::has(ComputeCapability required) const noexcept {
    ComputeCapability available = ComputeCapability::None;
    if (cpu_reference) {
        available = available | ComputeCapability::CpuReference;
    }
    if (diligent_compute) {
        available = available | ComputeCapability::DiligentCompute;
    }
    if (cuda_compute) {
        available = available | ComputeCapability::CudaCompute;
    }
    if (cuda_device_match) {
        available = available | ComputeCapability::CudaDeviceMatch;
    }
    return (available & required) == required;
}

const PerformanceThreshold* ComputePerformanceProfile::threshold(
    KernelId kernel, ComputeBackend backend) const noexcept {
    for (const PerformanceThreshold& candidate : thresholds) {
        if (candidate.kernel == kernel && candidate.backend == backend) {
            return &candidate;
        }
    }
    return nullptr;
}

std::span<const KernelBackendPolicy> BackendSelector::defaultPolicies() noexcept {
    return kPolicies;
}

const ComputePerformanceProfile& BackendSelector::defaultPerformanceProfile() noexcept {
    return kDefaultProfile;
}

BackendSelector::BackendSelector(std::span<const KernelBackendPolicy> policies,
                                 ComputePerformanceProfile profile,
                                 BackendTelemetrySink telemetry)
    : policies_{policies}, profile_{profile}, telemetry_{std::move(telemetry)} {}

const KernelBackendPolicy* BackendSelector::policy(KernelId kernel) const noexcept {
    for (const KernelBackendPolicy& candidate : policies_) {
        if (candidate.kernel == kernel) {
            return &candidate;
        }
    }
    return nullptr;
}

foundation::Result<BackendSelection, foundation::Error> BackendSelector::select(
    KernelId kernel, std::size_t work_size,
    const HardwareCapabilities& capabilities) const {
    return selectInternal(kernel, work_size, capabilities, {}, std::nullopt);
}

foundation::Result<BackendSelection, foundation::Error> BackendSelector::select(
    KernelId kernel, std::size_t work_size,
    const HardwareCapabilities& capabilities,
    const BackendSelectionConfig& config) const {
    return selectInternal(kernel, work_size, capabilities, config, std::nullopt);
}

foundation::Result<BackendSelection, foundation::Error> BackendSelector::selectInternal(
    KernelId kernel, std::size_t work_size,
    const HardwareCapabilities& capabilities,
    const BackendSelectionConfig& config,
    std::optional<ComputeBackend> excluded_backend) const {
    if (work_size == 0U) {
        return foundation::Result<BackendSelection, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument, "compute workload is empty"));
    }

    const KernelBackendPolicy* selected_policy = policy(kernel);
    if (selected_policy == nullptr || selected_policy->backends.empty()) {
        return foundation::Result<BackendSelection, foundation::Error>::failure(
            error(foundation::ErrorCode::NotFound, "compute kernel policy is not registered"));
    }

    const auto fallbackEntry = [&]() -> const BackendPolicyEntry* {
        for (const BackendPolicyEntry& candidate : selected_policy->backends) {
            if (candidate.backend == selected_policy->fallback) {
                return &candidate;
            }
        }
        return nullptr;
    };

    if (config.forced_backend.has_value()) {
        const ComputeBackend forced = *config.forced_backend;
        for (const BackendPolicyEntry& candidate : selected_policy->backends) {
            if (candidate.backend != forced) {
                continue;
            }
            const bool available = capabilities.has(candidate.required_capabilities) &&
                                   capabilities.supports(candidate.backend);
            if (available && entryIsApproved(candidate, kernel, profile_)) {
                return foundation::Result<BackendSelection, foundation::Error>::success(
                    BackendSelection{kernel, forced, forced, ComputeBackend::Cpu,
                                     SelectionReason::Forced, SelectionReason::None, false});
            }
            if (!config.fallback_when_forced_unavailable) {
                return foundation::Result<BackendSelection, foundation::Error>::failure(
                    error(foundation::ErrorCode::Unsupported,
                          "forced compute backend is unavailable or not approved"));
            }
            const BackendPolicyEntry* fallback = fallbackEntry();
            if (fallback == nullptr ||
                !capabilities.has(fallback->required_capabilities) ||
                !capabilities.supports(fallback->backend) ||
                !entryIsApproved(*fallback, kernel, profile_)) {
                return foundation::Result<BackendSelection, foundation::Error>::failure(
                    error(foundation::ErrorCode::Unsupported,
                          "forced backend unavailable and CPU fallback is unavailable"));
            }
            return foundation::Result<BackendSelection, foundation::Error>::success(
                BackendSelection{kernel, fallback->backend, forced, ComputeBackend::Cpu,
                                 SelectionReason::ForcedFallback,
                                 SelectionReason::ForcedUnsupported, true});
        }
        return foundation::Result<BackendSelection, foundation::Error>::failure(
            error(foundation::ErrorCode::Unsupported,
                  "forced compute backend is not allowed for this kernel"));
    }

    SelectionReason fallbackReason = SelectionReason::NoEligibleBackend;
    for (const BackendPolicyEntry& candidate : selected_policy->backends) {
        if (excluded_backend.has_value() && candidate.backend == *excluded_backend) {
            continue;
        }
        if (!capabilities.has(candidate.required_capabilities) ||
            !capabilities.supports(candidate.backend)) {
            fallbackReason = SelectionReason::CapabilityUnavailable;
            continue;
        }
        if (!entryIsApproved(candidate, kernel, profile_)) {
            fallbackReason = !candidate.correctness_approved
                                 ? SelectionReason::CorrectnessNotApproved
                                 : SelectionReason::ProfileRejected;
            continue;
        }
        if (work_size < effectiveMinimum(candidate, kernel, profile_)) {
            fallbackReason = SelectionReason::BelowCrossover;
            continue;
        }
        const bool isFallback = candidate.backend == selected_policy->fallback;
        return foundation::Result<BackendSelection, foundation::Error>::success(
            BackendSelection{kernel, candidate.backend, candidate.backend, ComputeBackend::Cpu,
                             isFallback ? fallbackReason : SelectionReason::Policy,
                             isFallback ? fallbackReason : SelectionReason::None, isFallback});
    }

    return foundation::Result<BackendSelection, foundation::Error>::failure(
        error(foundation::ErrorCode::Unsupported,
              "no capable and approved compute backend is available"));
}

foundation::Result<BackendSelection, foundation::Error>
BackendSelector::fallbackAfterRuntimeFailure(
    KernelId kernel, std::size_t work_size, ComputeBackend failed_backend,
    const HardwareCapabilities& capabilities) const {
    const KernelBackendPolicy* selected_policy = policy(kernel);
    if (selected_policy == nullptr) {
        return foundation::Result<BackendSelection, foundation::Error>::failure(
            error(foundation::ErrorCode::NotFound, "compute kernel policy is not registered"));
    }
    const BackendPolicyEntry* failed = nullptr;
    for (const BackendPolicyEntry& candidate : selected_policy->backends) {
        if (candidate.backend == failed_backend) {
            failed = &candidate;
            break;
        }
    }
    if (failed == nullptr || !failed->fallback_safe) {
        return foundation::Result<BackendSelection, foundation::Error>::failure(
            error(foundation::ErrorCode::Unsupported,
                  "runtime failure cannot use a declared safe fallback"));
    }

    const BackendPolicyEntry* fallback = nullptr;
    for (const BackendPolicyEntry& candidate : selected_policy->backends) {
        if (candidate.backend == selected_policy->fallback) {
            fallback = &candidate;
            break;
        }
    }
    if (fallback == nullptr || !capabilities.has(fallback->required_capabilities) ||
        !capabilities.supports(fallback->backend) ||
        !entryIsApproved(*fallback, kernel, profile_)) {
        return foundation::Result<BackendSelection, foundation::Error>::failure(
            error(foundation::ErrorCode::Unsupported,
                  "runtime failure fallback is unavailable"));
    }
    if (work_size == 0U) {
        return foundation::Result<BackendSelection, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument, "compute workload is empty"));
    }
    return foundation::Result<BackendSelection, foundation::Error>::success(
        BackendSelection{kernel, fallback->backend, fallback->backend, failed_backend,
                         SelectionReason::RuntimeFailure, SelectionReason::RuntimeFailure,
                         true});
}

void BackendSelector::recordTelemetry(const BackendSelection& selection,
                                      std::size_t dispatch_size,
                                      std::uint64_t latency_ns) const {
    if (!telemetry_) {
        return;
    }
    telemetry_(BackendTelemetry{selection.kernel, selection.backend, dispatch_size, latency_ns,
                                selection.reason, selection.fallback_reason,
                                selection.used_fallback});
}

std::string_view toString(ComputeBackend backend) noexcept {
    switch (backend) {
    case ComputeBackend::Cpu:
        return "CPU";
    case ComputeBackend::Diligent:
        return "Diligent";
    case ComputeBackend::Cuda:
        return "CUDA";
    }
    return "Unknown";
}

std::string_view toString(KernelId kernel) noexcept {
    switch (kernel) {
    case KernelId::Generic:
        return "generic";
    case KernelId::ThreatInfluence:
        return "threat_influence";
    case KernelId::VisibilityBatch:
        return "visibility_batch";
    case KernelId::FlowField:
        return "flow_field";
    case KernelId::FieldAtlas:
        return "field_atlas";
    }
    return "unknown";
}

std::string_view toString(SelectionReason reason) noexcept {
    switch (reason) {
    case SelectionReason::None:
        return "none";
    case SelectionReason::Policy:
        return "policy";
    case SelectionReason::Forced:
        return "forced";
    case SelectionReason::BelowCrossover:
        return "below_crossover";
    case SelectionReason::CapabilityUnavailable:
        return "capability_unavailable";
    case SelectionReason::ProfileRejected:
        return "profile_rejected";
    case SelectionReason::CorrectnessNotApproved:
        return "correctness_not_approved";
    case SelectionReason::ForcedUnsupported:
        return "forced_unsupported";
    case SelectionReason::ForcedFallback:
        return "forced_fallback";
    case SelectionReason::RuntimeFailure:
        return "runtime_failure";
    case SelectionReason::NoEligibleBackend:
        return "no_eligible_backend";
    }
    return "unknown";
}

} // namespace genomes::compute
