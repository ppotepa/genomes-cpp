#include <genomes/compute/CudaInterop.hpp>

#include <algorithm>

namespace genomes::compute {
namespace {

[[nodiscard]] InteropDecision rejected(InteropRejectionReason reason) noexcept {
    InteropDecision decision{};
    decision.rejection = reason;
    return decision;
}

} // namespace

DeviceIdentity DeviceIdentity::uuid(
    const std::array<std::uint8_t, 16U>& value) noexcept {
    DeviceIdentity identity{};
    identity.type = DeviceIdentityType::Uuid;
    identity.bytes = value;
    identity.byte_count = 16U;
    return identity;
}

DeviceIdentity DeviceIdentity::luid(
    const std::array<std::uint8_t, 8U>& value) noexcept {
    DeviceIdentity identity{};
    identity.type = DeviceIdentityType::Luid;
    std::copy(value.begin(), value.end(), identity.bytes.begin());
    identity.byte_count = 8U;
    return identity;
}

bool DeviceIdentity::valid() const noexcept {
    const bool expected_size =
        (type == DeviceIdentityType::Uuid && byte_count == 16U) ||
        (type == DeviceIdentityType::Luid && byte_count == 8U);
    return expected_size;
}

DeviceIdentityComparison compareDeviceIdentity(
    const DeviceIdentity& render, const DeviceIdentity& compute) noexcept {
    if (!render.valid()) {
        return DeviceIdentityComparison::MissingRenderIdentity;
    }
    if (!compute.valid()) {
        return DeviceIdentityComparison::MissingComputeIdentity;
    }
    if (render.type != compute.type) {
        return DeviceIdentityComparison::TypeMismatch;
    }
    if (!std::equal(render.bytes.begin(), render.bytes.begin() + render.byte_count,
                   compute.bytes.begin())) {
        return DeviceIdentityComparison::ValueMismatch;
    }
    return DeviceIdentityComparison::Match;
}

bool deviceIdentitiesMatch(const DeviceIdentity& render,
                           const DeviceIdentity& compute) noexcept {
    return compareDeviceIdentity(render, compute) == DeviceIdentityComparison::Match;
}

InteropDecision evaluateInterop(const InteropRequest& request) noexcept {
    const InteropCapabilities& capabilities = request.capabilities;
    if (!capabilities.contract_enabled) {
        return rejected(InteropRejectionReason::ContractDisabled);
    }
    if (!capabilities.cuda_runtime_available ||
        !capabilities.render_backend_available ||
        !capabilities.identity_query_available) {
        return rejected(InteropRejectionReason::RuntimeUnavailable);
    }

    const DeviceIdentityComparison identity =
        compareDeviceIdentity(request.render_device, request.compute_device);
    if (identity != DeviceIdentityComparison::Match) {
        InteropDecision decision{};
        decision.identity = identity;
        switch (identity) {
        case DeviceIdentityComparison::MissingRenderIdentity:
            decision.rejection = InteropRejectionReason::RenderIdentityMissing;
            break;
        case DeviceIdentityComparison::MissingComputeIdentity:
            decision.rejection = InteropRejectionReason::ComputeIdentityMissing;
            break;
        case DeviceIdentityComparison::TypeMismatch:
            decision.rejection = InteropRejectionReason::IdentityTypeMismatch;
            break;
        case DeviceIdentityComparison::ValueMismatch:
            decision.rejection = InteropRejectionReason::DeviceMismatch;
            break;
        case DeviceIdentityComparison::Match:
            break;
        }
        return decision;
    }

    const InteropResourceRequest& resource = request.resource;
    if (resource.render_resource_id == 0U || resource.size_bytes == 0U) {
        return rejected(InteropRejectionReason::InvalidResource);
    }
    if (capabilities.device_generation == 0U ||
        resource.device_generation == 0U ||
        capabilities.device_generation != resource.device_generation) {
        return rejected(InteropRejectionReason::DeviceGenerationMismatch);
    }

    const ExternalMemoryCapabilities& memory = capabilities.external_memory;
    if (!memory.exportable || !memory.importable) {
        return rejected(InteropRejectionReason::ExternalMemoryUnsupported);
    }
    if (!memory.supports(resource.memory_handle)) {
        return rejected(InteropRejectionReason::ExternalMemoryHandleUnsupported);
    }
    if (!memory.supports(resource.resource_type)) {
        return rejected(InteropRejectionReason::ResourceTypeUnsupported);
    }
    if (!memory.supports(resource.format)) {
        return rejected(InteropRejectionReason::ResourceFormatUnsupported);
    }

    const ExternalSemaphoreCapabilities& semaphore = capabilities.external_semaphore;
    if (!semaphore.exportable || !semaphore.importable ||
        (!semaphore.binary && !semaphore.timeline)) {
        return rejected(InteropRejectionReason::ExternalSemaphoreUnsupported);
    }
    if (!semaphore.supports(resource.semaphore_handle)) {
        return rejected(InteropRejectionReason::ExternalSemaphoreHandleUnsupported);
    }
    if (resource.require_timeline_semaphore && !semaphore.timeline) {
        return rejected(InteropRejectionReason::TimelineSemaphoreUnsupported);
    }

    InteropDecision decision{};
    decision.accepted = true;
    decision.zero_copy = true;
    decision.externally_ordered = true;
    decision.identity = DeviceIdentityComparison::Match;
    return decision;
}

std::string_view toString(DeviceIdentityType type) noexcept {
    switch (type) {
    case DeviceIdentityType::None:
        return "none";
    case DeviceIdentityType::Uuid:
        return "uuid";
    case DeviceIdentityType::Luid:
        return "luid";
    }
    return "unknown";
}

std::string_view toString(DeviceIdentityComparison comparison) noexcept {
    switch (comparison) {
    case DeviceIdentityComparison::Match:
        return "match";
    case DeviceIdentityComparison::MissingRenderIdentity:
        return "render_identity_missing";
    case DeviceIdentityComparison::MissingComputeIdentity:
        return "compute_identity_missing";
    case DeviceIdentityComparison::TypeMismatch:
        return "identity_type_mismatch";
    case DeviceIdentityComparison::ValueMismatch:
        return "identity_value_mismatch";
    }
    return "unknown";
}

std::string_view toString(InteropRejectionReason reason) noexcept {
    switch (reason) {
    case InteropRejectionReason::None:
        return "none";
    case InteropRejectionReason::ContractDisabled:
        return "contract_disabled";
    case InteropRejectionReason::RuntimeUnavailable:
        return "runtime_unavailable";
    case InteropRejectionReason::RenderIdentityMissing:
        return "render_identity_missing";
    case InteropRejectionReason::ComputeIdentityMissing:
        return "compute_identity_missing";
    case InteropRejectionReason::IdentityTypeMismatch:
        return "identity_type_mismatch";
    case InteropRejectionReason::DeviceMismatch:
        return "device_mismatch";
    case InteropRejectionReason::DeviceGenerationMismatch:
        return "device_generation_mismatch";
    case InteropRejectionReason::InvalidResource:
        return "invalid_resource";
    case InteropRejectionReason::ExternalMemoryUnsupported:
        return "external_memory_unsupported";
    case InteropRejectionReason::ExternalMemoryHandleUnsupported:
        return "external_memory_handle_unsupported";
    case InteropRejectionReason::ResourceTypeUnsupported:
        return "resource_type_unsupported";
    case InteropRejectionReason::ResourceFormatUnsupported:
        return "resource_format_unsupported";
    case InteropRejectionReason::ExternalSemaphoreUnsupported:
        return "external_semaphore_unsupported";
    case InteropRejectionReason::ExternalSemaphoreHandleUnsupported:
        return "external_semaphore_handle_unsupported";
    case InteropRejectionReason::TimelineSemaphoreUnsupported:
        return "timeline_semaphore_unsupported";
    }
    return "unknown";
}

std::string_view toString(InteropFallback fallback) noexcept {
    switch (fallback) {
    case InteropFallback::DiligentCompute:
        return "diligent_compute";
    case InteropFallback::DiligentCopy:
        return "diligent_copy";
    }
    return "unknown";
}

} // namespace genomes::compute
