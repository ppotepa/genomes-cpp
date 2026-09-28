#include <genomes/compute/CudaComputeBackend.hpp>

#include <memory>

namespace genomes::compute {

class CudaComputeBackend::Impl final {
public:
    CudaBackendStatus status{CudaBackendStatus::Disabled};
    CudaCapabilities capabilities{};
};

CudaComputeBackend::CudaComputeBackend()
    : impl_{std::make_unique<Impl>()} {}

CudaComputeBackend::~CudaComputeBackend() = default;
CudaComputeBackend::CudaComputeBackend(CudaComputeBackend&&) noexcept = default;
CudaComputeBackend& CudaComputeBackend::operator=(CudaComputeBackend&&) noexcept = default;

bool CudaComputeBackend::compiled() noexcept {
    return false;
}

CudaBackendStatus CudaComputeBackend::status() const noexcept {
    return impl_->status;
}

const CudaCapabilities& CudaComputeBackend::capabilities() const noexcept {
    return impl_->capabilities;
}

foundation::Result<void, foundation::Error> CudaComputeBackend::initialize() noexcept {
    // The stub is intentionally a successful no-op.  Callers can keep their
    // normal CPU/Diligent fallback path without special build-time branches.
    return foundation::Result<void, foundation::Error>::success();
}

foundation::Result<void, foundation::Error> CudaComputeBackend::dispatch(
    const ComputeDispatch&, const ComputeRange&) {
    return foundation::Result<void, foundation::Error>::failure(
        {foundation::ErrorCode::Unsupported,
         "CUDA backend is not compiled; use the CPU or Diligent backend"});
}

foundation::Result<CudaCompletionToken, foundation::Error>
CudaComputeBackend::submitFieldFill(std::span<std::uint8_t> field, std::uint8_t) {
    if (field.empty()) {
        return foundation::Result<CudaCompletionToken, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "CUDA field must not be empty"});
    }
    return foundation::Result<CudaCompletionToken, foundation::Error>::failure(
        {foundation::ErrorCode::Unsupported,
         "CUDA backend is not compiled; field fill is unavailable"});
}

foundation::Result<CudaCompletion, foundation::Error>
CudaComputeBackend::poll(CudaCompletionToken token) {
    if (!token.valid()) {
        return foundation::Result<CudaCompletion, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid CUDA completion token"});
    }
    return foundation::Result<CudaCompletion, foundation::Error>::failure(
        {foundation::ErrorCode::Unsupported,
         "CUDA backend is not compiled; completion token is unavailable"});
}

foundation::Result<void, foundation::Error>
CudaComputeBackend::wait(CudaCompletionToken token) {
    if (!token.valid()) {
        return foundation::Result<void, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid CUDA completion token"});
    }
    return foundation::Result<void, foundation::Error>::failure(
        {foundation::ErrorCode::Unsupported,
         "CUDA backend is not compiled; completion token is unavailable"});
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
