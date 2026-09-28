#include <genomes/compute/CudaComputeBackend.hpp>

#include <cuda_runtime_api.h>

#include <array>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace genomes::compute {
namespace {

[[nodiscard]] foundation::Error cudaError(cudaError_t result,
                                           const char* fallback) noexcept {
    const char* message = cudaGetErrorString(result);
    return {foundation::ErrorCode::Internal,
            message == nullptr ? fallback : message};
}

[[nodiscard]] foundation::Error unavailableError() noexcept {
    return {foundation::ErrorCode::Unsupported,
            "no usable NVIDIA CUDA device is available"};
}

[[nodiscard]] std::string uuidString(const cudaUUID_t& uuid) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(32U);
    for (unsigned char byte : uuid.bytes) {
        result.push_back(digits[(byte >> 4U) & 0x0fU]);
        result.push_back(digits[byte & 0x0fU]);
    }
    return result;
}

} // namespace

class CudaComputeBackend::Impl final {
public:
    struct Pending final {
        cudaEvent_t event{nullptr};
        void* device_memory{nullptr};
    };

    ~Impl() {
        if (stream != nullptr) {
            (void)cudaStreamSynchronize(stream);
        }
        for (auto& entry : pending) {
            (void)cudaEventDestroy(entry.second.event);
            (void)cudaFree(entry.second.device_memory);
        }
        pending.clear();
        if (stream != nullptr) {
            (void)cudaStreamDestroy(stream);
        }
    }

    CudaBackendStatus status{CudaBackendStatus::Unavailable};
    CudaCapabilities capabilities{true, false, false, false, false, false, {}};
    bool initialized{false};
    cudaStream_t stream{nullptr};
    std::uint64_t next_token{1U};
    std::unordered_map<std::uint64_t, Pending> pending{};
    mutable std::mutex mutex{};
};

CudaComputeBackend::CudaComputeBackend()
    : impl_{std::make_unique<Impl>()} {
    (void)initialize();
}

CudaComputeBackend::~CudaComputeBackend() = default;
CudaComputeBackend::CudaComputeBackend(CudaComputeBackend&&) noexcept = default;
CudaComputeBackend& CudaComputeBackend::operator=(CudaComputeBackend&&) noexcept = default;

bool CudaComputeBackend::compiled() noexcept {
    return true;
}

CudaBackendStatus CudaComputeBackend::status() const noexcept {
    return impl_->status;
}

const CudaCapabilities& CudaComputeBackend::capabilities() const noexcept {
    return impl_->capabilities;
}

foundation::Result<void, foundation::Error> CudaComputeBackend::initialize() noexcept {
    std::scoped_lock lock(impl_->mutex);
    if (impl_->initialized) {
        return foundation::Result<void, foundation::Error>::success();
    }

    int device_count = 0;
    const cudaError_t count_result = cudaGetDeviceCount(&device_count);
    if (count_result != cudaSuccess || device_count <= 0) {
        if (count_result == cudaErrorNoDevice ||
            count_result == cudaErrorInsufficientDriver || device_count <= 0) {
            (void)cudaGetLastError();
            impl_->status = CudaBackendStatus::Unavailable;
            impl_->capabilities.runtime_available = false;
            impl_->capabilities.device_available = false;
            impl_->capabilities.async_submission = false;
            impl_->capabilities.field_fill = false;
            impl_->initialized = true;
            return foundation::Result<void, foundation::Error>::success();
        }
        impl_->status = CudaBackendStatus::RuntimeError;
        return foundation::Result<void, foundation::Error>::failure(
            cudaError(count_result, "cudaGetDeviceCount failed"));
    }

    const cudaError_t set_result = cudaSetDevice(0);
    if (set_result != cudaSuccess) {
        impl_->status = CudaBackendStatus::RuntimeError;
        return foundation::Result<void, foundation::Error>::failure(
            cudaError(set_result, "cudaSetDevice failed"));
    }

    cudaDeviceProp properties{};
    const cudaError_t properties_result = cudaGetDeviceProperties(&properties, 0);
    if (properties_result != cudaSuccess) {
        impl_->status = CudaBackendStatus::RuntimeError;
        return foundation::Result<void, foundation::Error>::failure(
            cudaError(properties_result, "cudaGetDeviceProperties failed"));
    }

    const cudaError_t stream_result = cudaStreamCreateWithFlags(
        &impl_->stream, cudaStreamNonBlocking);
    if (stream_result != cudaSuccess) {
        impl_->status = CudaBackendStatus::RuntimeError;
        return foundation::Result<void, foundation::Error>::failure(
            cudaError(stream_result, "cudaStreamCreate failed"));
    }

    impl_->capabilities.runtime_available = true;
    impl_->capabilities.device_available = true;
    impl_->capabilities.device_match = true;
    impl_->capabilities.async_submission = true;
    impl_->capabilities.field_fill = true;
    impl_->capabilities.device.name = properties.name;
    impl_->capabilities.device.uuid = uuidString(properties.uuid);
    impl_->capabilities.device.pci_domain = static_cast<std::uint32_t>(properties.pciDomainID);
    impl_->capabilities.device.pci_bus = static_cast<std::uint32_t>(properties.pciBusID);
    impl_->capabilities.device.pci_device = static_cast<std::uint32_t>(properties.pciDeviceID);
    impl_->status = CudaBackendStatus::Ready;
    impl_->initialized = true;
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> CudaComputeBackend::dispatch(
    const ComputeDispatch&, const ComputeRange&) {
    return foundation::Result<void, foundation::Error>::failure(
        {foundation::ErrorCode::Unsupported,
         "CUDA generic callback dispatch is unsupported; use typed field kernels"});
}

foundation::Result<CudaCompletionToken, foundation::Error>
CudaComputeBackend::submitFieldFill(std::span<std::uint8_t> field, std::uint8_t value) {
    if (field.empty()) {
        return foundation::Result<CudaCompletionToken, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "CUDA field must not be empty"});
    }

    std::scoped_lock lock(impl_->mutex);
    if (impl_->status != CudaBackendStatus::Ready) {
        return foundation::Result<CudaCompletionToken, foundation::Error>::failure(
            unavailableError());
    }

    void* device_memory = nullptr;
    cudaError_t result = cudaMalloc(&device_memory, field.size_bytes());
    if (result != cudaSuccess) {
        impl_->status = CudaBackendStatus::RuntimeError;
        return foundation::Result<CudaCompletionToken, foundation::Error>::failure(
            cudaError(result, "cudaMalloc failed"));
    }

    result = cudaMemsetAsync(device_memory, static_cast<int>(value), field.size_bytes(),
                              impl_->stream);
    if (result == cudaSuccess) {
        result = cudaMemcpyAsync(field.data(), device_memory, field.size_bytes(),
                                 cudaMemcpyDeviceToHost, impl_->stream);
    }
    cudaEvent_t event = nullptr;
    if (result == cudaSuccess) {
        result = cudaEventCreateWithFlags(&event, cudaEventDisableTiming);
    }
    if (result == cudaSuccess) {
        result = cudaEventRecord(event, impl_->stream);
    }
    if (result != cudaSuccess) {
        if (event != nullptr) {
            (void)cudaEventDestroy(event);
        }
        (void)cudaFree(device_memory);
        impl_->status = CudaBackendStatus::RuntimeError;
        return foundation::Result<CudaCompletionToken, foundation::Error>::failure(
            cudaError(result, "CUDA field submission failed"));
    }

    const std::uint64_t token_value = impl_->next_token++;
    impl_->pending.emplace(token_value, Impl::Pending{event, device_memory});
    return foundation::Result<CudaCompletionToken, foundation::Error>::success(
        {token_value});
}

foundation::Result<CudaCompletion, foundation::Error>
CudaComputeBackend::poll(CudaCompletionToken token) {
    if (!token.valid()) {
        return foundation::Result<CudaCompletion, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid CUDA completion token"});
    }

    std::scoped_lock lock(impl_->mutex);
    const auto iterator = impl_->pending.find(token.value);
    if (iterator == impl_->pending.end()) {
        return foundation::Result<CudaCompletion, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "CUDA completion token not found"});
    }

    const cudaError_t result = cudaEventQuery(iterator->second.event);
    if (result == cudaErrorNotReady) {
        return foundation::Result<CudaCompletion, foundation::Error>::success(
            {token, CudaCompletionStatus::Pending});
    }
    if (result != cudaSuccess) {
        (void)cudaEventDestroy(iterator->second.event);
        (void)cudaFree(iterator->second.device_memory);
        impl_->pending.erase(iterator);
        impl_->status = CudaBackendStatus::RuntimeError;
        return foundation::Result<CudaCompletion, foundation::Error>::failure(
            cudaError(result, "cudaEventQuery failed"));
    }

    (void)cudaEventDestroy(iterator->second.event);
    (void)cudaFree(iterator->second.device_memory);
    impl_->pending.erase(iterator);
    return foundation::Result<CudaCompletion, foundation::Error>::success(
        {token, CudaCompletionStatus::Complete});
}

foundation::Result<void, foundation::Error>
CudaComputeBackend::wait(CudaCompletionToken token) {
    if (!token.valid()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid CUDA completion token"});
    }

    std::scoped_lock lock(impl_->mutex);
    const auto iterator = impl_->pending.find(token.value);
    if (iterator == impl_->pending.end()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "CUDA completion token not found"});
    }
    const cudaError_t result = cudaEventSynchronize(iterator->second.event);
    (void)cudaEventDestroy(iterator->second.event);
    (void)cudaFree(iterator->second.device_memory);
    impl_->pending.erase(iterator);
    if (result != cudaSuccess) {
        impl_->status = CudaBackendStatus::RuntimeError;
        return foundation::Result<void, foundation::Error>::failure(
            cudaError(result, "cudaEventSynchronize failed"));
    }
    return foundation::Result<void, foundation::Error>::success();
}

const char* toString(CudaBackendStatus status) noexcept {
    switch (status) {
    case CudaBackendStatus::Disabled:
        return "disabled";
    case CudaBackendStatus::Unavailable:
        return "unavailable";
    case CudaBackendStatus::Ready:
        return "ready";
    case CudaBackendStatus::RuntimeError:
        return "runtime_error";
    }
    return "unknown";
}

const char* toString(CudaCompletionStatus status) noexcept {
    switch (status) {
    case CudaCompletionStatus::Pending:
        return "pending";
    case CudaCompletionStatus::Complete:
        return "complete";
    }
    return "unknown";
}

} // namespace genomes::compute
