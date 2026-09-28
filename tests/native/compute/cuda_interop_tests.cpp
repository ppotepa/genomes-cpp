#include <genomes/compute/CudaInterop.hpp>

#include <array>
#include <cassert>
#include <cstdint>

namespace {

using namespace genomes::compute;

DeviceIdentity makeUuid(std::uint8_t seed) {
    std::array<std::uint8_t, 16U> bytes{};
    for (std::size_t index = 0U; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(seed + index);
    }
    return DeviceIdentity::uuid(bytes);
}

DeviceIdentity makeLuid(std::uint8_t seed) {
    std::array<std::uint8_t, 8U> bytes{};
    for (std::size_t index = 0U; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(seed + index);
    }
    return DeviceIdentity::luid(bytes);
}

InteropRequest supportedRequest() {
    InteropRequest request{};
    request.render_device = makeUuid(0x10U);
    request.compute_device = makeUuid(0x10U);
    request.capabilities.contract_enabled = true;
    request.capabilities.cuda_runtime_available = true;
    request.capabilities.render_backend_available = true;
    request.capabilities.identity_query_available = true;
    request.capabilities.device_generation = 4U;
    request.capabilities.external_memory.exportable = true;
    request.capabilities.external_memory.importable = true;
    request.capabilities.external_memory.buffers = true;
    request.capabilities.external_memory.images = true;
    request.capabilities.external_memory.handle_types =
        ExternalMemoryHandleType::OpaqueFileDescriptor |
        ExternalMemoryHandleType::OpaqueWin32Handle;
    request.capabilities.external_memory.formats =
        InteropResourceFormat::RawBytes |
        InteropResourceFormat::R8Uint |
        InteropResourceFormat::Rgba8Unorm;
    request.capabilities.external_semaphore.exportable = true;
    request.capabilities.external_semaphore.importable = true;
    request.capabilities.external_semaphore.binary = true;
    request.capabilities.external_semaphore.timeline = true;
    request.capabilities.external_semaphore.handle_types =
        ExternalSemaphoreHandleType::OpaqueFileDescriptor |
        ExternalSemaphoreHandleType::OpaqueWin32Handle;
    request.resource.render_resource_id = 17U;
    request.resource.size_bytes = 4096U;
    request.resource.device_generation = 4U;
    request.resource.resource_type = InteropResourceType::Buffer;
    request.resource.format = InteropResourceFormat::RawBytes;
    request.resource.memory_handle = ExternalMemoryHandleType::OpaqueFileDescriptor;
    request.resource.semaphore_handle = ExternalSemaphoreHandleType::OpaqueFileDescriptor;
    request.resource.require_timeline_semaphore = true;
    return request;
}

} // namespace

int main() {
    // UUID and LUID are value comparisons; a matching ordinal/name is never
    // enough to authorize interop.
    const DeviceIdentity uuid = makeUuid(0x10U);
    assert(compareDeviceIdentity(uuid, makeUuid(0x10U)) ==
           DeviceIdentityComparison::Match);
    assert(deviceIdentitiesMatch(uuid, makeUuid(0x10U)));
    assert(compareDeviceIdentity(uuid, makeUuid(0x11U)) ==
           DeviceIdentityComparison::ValueMismatch);
    assert(compareDeviceIdentity(uuid, makeLuid(0x10U)) ==
           DeviceIdentityComparison::TypeMismatch);
    assert(compareDeviceIdentity(DeviceIdentity{}, uuid) ==
           DeviceIdentityComparison::MissingRenderIdentity);

    const InteropRequest fixture = supportedRequest();
    const InteropDecision accepted = evaluateInterop(fixture);
    assert(accepted.accepted);
    assert(accepted.zero_copy);
    assert(accepted.externally_ordered);
    assert(accepted.identity == DeviceIdentityComparison::Match);
    assert(!accepted.usesFallback());

    InteropRequest request = fixture;
    request.capabilities.contract_enabled = false;
    assert(evaluateInterop(request).rejection ==
           InteropRejectionReason::ContractDisabled);

    request = fixture;
    request.capabilities.cuda_runtime_available = false;
    assert(evaluateInterop(request).rejection ==
           InteropRejectionReason::RuntimeUnavailable);

    request = fixture;
    request.compute_device = makeUuid(0x22U);
    assert(evaluateInterop(request).rejection == InteropRejectionReason::DeviceMismatch);

    request = fixture;
    request.resource.device_generation = 3U;
    assert(evaluateInterop(request).rejection ==
           InteropRejectionReason::DeviceGenerationMismatch);

    request = fixture;
    request.capabilities.external_memory.importable = false;
    assert(evaluateInterop(request).rejection ==
           InteropRejectionReason::ExternalMemoryUnsupported);

    request = fixture;
    request.resource.memory_handle = ExternalMemoryHandleType::NativeHandle;
    assert(evaluateInterop(request).rejection ==
           InteropRejectionReason::ExternalMemoryHandleUnsupported);

    request = fixture;
    request.resource.format = InteropResourceFormat::Rgba16Float;
    assert(evaluateInterop(request).rejection ==
           InteropRejectionReason::ResourceFormatUnsupported);

    request = fixture;
    request.capabilities.external_semaphore.timeline = false;
    assert(evaluateInterop(request).rejection ==
           InteropRejectionReason::TimelineSemaphoreUnsupported);

    request = fixture;
    request.resource.semaphore_handle = ExternalSemaphoreHandleType::NativeHandle;
    assert(evaluateInterop(request).rejection ==
           InteropRejectionReason::ExternalSemaphoreHandleUnsupported);

    request = fixture;
    request.capabilities.external_semaphore.binary = false;
    request.capabilities.external_semaphore.timeline = false;
    assert(evaluateInterop(request).rejection ==
           InteropRejectionReason::ExternalSemaphoreUnsupported);

    assert(toString(InteropRejectionReason::DeviceMismatch) == "device_mismatch");
    assert(toString(InteropFallback::DiligentCompute) == "diligent_compute");
    return 0;
}
