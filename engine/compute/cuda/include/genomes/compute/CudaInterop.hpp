#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <array>

namespace genomes::compute {

// These are serialized/value representations of identifiers queried by a
// backend adapter.  They intentionally do not mirror any CUDA, Vulkan,
// D3D12, or Diligent native structure.
enum class DeviceIdentityType : std::uint8_t {
    None,
    Uuid,
    Luid,
};

struct DeviceIdentity final {
    DeviceIdentityType type{DeviceIdentityType::None};
    std::array<std::uint8_t, 16U> bytes{};
    std::uint8_t byte_count{0U};

    [[nodiscard]] static DeviceIdentity uuid(
        const std::array<std::uint8_t, 16U>& value) noexcept;
    [[nodiscard]] static DeviceIdentity luid(
        const std::array<std::uint8_t, 8U>& value) noexcept;

    [[nodiscard]] static DeviceIdentity fromUuid(
        const std::array<std::uint8_t, 16U>& value) noexcept {
        return uuid(value);
    }

    [[nodiscard]] static DeviceIdentity fromLuid(
        const std::array<std::uint8_t, 8U>& value) noexcept {
        return luid(value);
    }

    [[nodiscard]] bool valid() const noexcept;
    friend bool operator==(const DeviceIdentity&, const DeviceIdentity&) noexcept = default;
};

enum class DeviceIdentityComparison : std::uint8_t {
    Match,
    MissingRenderIdentity,
    MissingComputeIdentity,
    TypeMismatch,
    ValueMismatch,
};

[[nodiscard]] DeviceIdentityComparison compareDeviceIdentity(
    const DeviceIdentity& render, const DeviceIdentity& compute) noexcept;

[[nodiscard]] bool deviceIdentitiesMatch(const DeviceIdentity& render,
                                         const DeviceIdentity& compute) noexcept;

// Handle kinds are transport descriptors only.  The native object and its
// ownership remain private to the render/CUDA adapter that implements them.
enum class ExternalMemoryHandleType : std::uint8_t {
    None = 0U,
    OpaqueFileDescriptor = 1U << 0U,
    OpaqueWin32Handle = 1U << 1U,
    NativeHandle = 1U << 2U,
};

[[nodiscard]] constexpr ExternalMemoryHandleType operator|(
    ExternalMemoryHandleType lhs, ExternalMemoryHandleType rhs) noexcept {
    return static_cast<ExternalMemoryHandleType>(
        static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

[[nodiscard]] constexpr bool hasFlag(ExternalMemoryHandleType value,
                                      ExternalMemoryHandleType flag) noexcept {
    return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(flag)) != 0U;
}

enum class ExternalSemaphoreHandleType : std::uint8_t {
    None = 0U,
    OpaqueFileDescriptor = 1U << 0U,
    OpaqueWin32Handle = 1U << 1U,
    NativeHandle = 1U << 2U,
};

[[nodiscard]] constexpr ExternalSemaphoreHandleType operator|(
    ExternalSemaphoreHandleType lhs, ExternalSemaphoreHandleType rhs) noexcept {
    return static_cast<ExternalSemaphoreHandleType>(
        static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

[[nodiscard]] constexpr bool hasFlag(ExternalSemaphoreHandleType value,
                                      ExternalSemaphoreHandleType flag) noexcept {
    return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(flag)) != 0U;
}

enum class InteropResourceType : std::uint8_t {
    Buffer,
    Image,
};

enum class InteropResourceFormat : std::uint8_t {
    Unknown = 0U,
    RawBytes = 1U << 0U,
    R8Uint = 1U << 1U,
    Rgba8Unorm = 1U << 2U,
    Rgba16Float = 1U << 3U,
};

[[nodiscard]] constexpr InteropResourceFormat operator|(
    InteropResourceFormat lhs, InteropResourceFormat rhs) noexcept {
    return static_cast<InteropResourceFormat>(
        static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

[[nodiscard]] constexpr bool hasFlag(InteropResourceFormat value,
                                      InteropResourceFormat flag) noexcept {
    return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(flag)) != 0U;
}

struct ExternalMemoryCapabilities final {
    bool exportable{false};
    bool importable{false};
    bool buffers{false};
    bool images{false};
    ExternalMemoryHandleType handle_types{ExternalMemoryHandleType::None};
    InteropResourceFormat formats{InteropResourceFormat::Unknown};

    [[nodiscard]] bool supports(ExternalMemoryHandleType handle) const noexcept {
        return handle != ExternalMemoryHandleType::None && hasFlag(handle_types, handle);
    }

    [[nodiscard]] bool supports(InteropResourceType resource) const noexcept {
        return resource == InteropResourceType::Buffer ? buffers : images;
    }

    [[nodiscard]] bool supports(InteropResourceFormat format) const noexcept {
        return format != InteropResourceFormat::Unknown && hasFlag(formats, format);
    }
};

struct ExternalSemaphoreCapabilities final {
    bool exportable{false};
    bool importable{false};
    bool binary{false};
    bool timeline{false};
    ExternalSemaphoreHandleType handle_types{ExternalSemaphoreHandleType::None};

    [[nodiscard]] bool supports(ExternalSemaphoreHandleType handle) const noexcept {
        return handle != ExternalSemaphoreHandleType::None && hasFlag(handle_types, handle);
    }
};

// This snapshot is supplied by a backend adapter.  It is deliberately
// capability based so an OFF build and a headless test can use the same gate.
struct InteropCapabilities final {
    bool contract_enabled{false};
    bool cuda_runtime_available{false};
    bool render_backend_available{false};
    bool identity_query_available{false};
    std::uint64_t device_generation{0U};
    ExternalMemoryCapabilities external_memory{};
    ExternalSemaphoreCapabilities external_semaphore{};
};

struct InteropResourceRequest final {
    std::uint64_t render_resource_id{0U};
    std::size_t size_bytes{0U};
    std::uint64_t device_generation{0U};
    InteropResourceType resource_type{InteropResourceType::Buffer};
    InteropResourceFormat format{InteropResourceFormat::Unknown};
    ExternalMemoryHandleType memory_handle{ExternalMemoryHandleType::None};
    ExternalSemaphoreHandleType semaphore_handle{ExternalSemaphoreHandleType::None};
    bool require_timeline_semaphore{false};
};

struct InteropRequest final {
    DeviceIdentity render_device{};
    DeviceIdentity compute_device{};
    InteropCapabilities capabilities{};
    InteropResourceRequest resource{};
};

enum class InteropRejectionReason : std::uint8_t {
    None,
    ContractDisabled,
    RuntimeUnavailable,
    RenderIdentityMissing,
    ComputeIdentityMissing,
    IdentityTypeMismatch,
    DeviceMismatch,
    DeviceGenerationMismatch,
    InvalidResource,
    ExternalMemoryUnsupported,
    ExternalMemoryHandleUnsupported,
    ResourceTypeUnsupported,
    ResourceFormatUnsupported,
    ExternalSemaphoreUnsupported,
    ExternalSemaphoreHandleUnsupported,
    TimelineSemaphoreUnsupported,
};

enum class InteropFallback : std::uint8_t {
    DiligentCompute,
    DiligentCopy,
};

struct InteropDecision final {
    bool accepted{false};
    bool zero_copy{false};
    bool externally_ordered{false};
    DeviceIdentityComparison identity{DeviceIdentityComparison::MissingRenderIdentity};
    InteropRejectionReason rejection{InteropRejectionReason::None};
    InteropFallback fallback{InteropFallback::DiligentCompute};

    [[nodiscard]] bool usesFallback() const noexcept {
        return !accepted;
    }
};

// Pure contract evaluation.  A true result means an adapter may attempt the
// import; it does not itself open an OS handle, create a resource, or submit a
// queue/stream operation.
[[nodiscard]] InteropDecision evaluateInterop(const InteropRequest& request) noexcept;

[[nodiscard]] std::string_view toString(DeviceIdentityType type) noexcept;
[[nodiscard]] std::string_view toString(DeviceIdentityComparison comparison) noexcept;
[[nodiscard]] std::string_view toString(InteropRejectionReason reason) noexcept;
[[nodiscard]] std::string_view toString(InteropFallback fallback) noexcept;

} // namespace genomes::compute
