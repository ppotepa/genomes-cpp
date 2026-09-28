#pragma once

#include <genomes/compute/ComputeService.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace genomes::compute {

enum class CudaBackendStatus : std::uint8_t {
    Disabled,
    Unavailable,
    Ready,
    RuntimeError,
};

enum class CudaCompletionStatus : std::uint8_t {
    Pending,
    Complete,
};

// This identity is deliberately made of ordinary value types.  It can be
// logged and compared by generic code without including a CUDA header.
struct CudaDeviceIdentity final {
    std::string name{};
    std::string uuid{};
    std::uint32_t pci_domain{0U};
    std::uint32_t pci_bus{0U};
    std::uint32_t pci_device{0U};

    [[nodiscard]] bool valid() const noexcept {
        return !name.empty();
    }
};

struct CudaCapabilities final {
    bool compiled{false};
    bool runtime_available{false};
    bool device_available{false};
    bool device_match{false};
    bool async_submission{false};
    bool field_fill{false};
    CudaDeviceIdentity device{};
};

struct CudaCompletionToken final {
    std::uint64_t value{0U};

    [[nodiscard]] bool valid() const noexcept {
        return value != 0U;
    }
};

struct CudaCompletion final {
    CudaCompletionToken token{};
    CudaCompletionStatus status{CudaCompletionStatus::Pending};
};

// A small, end-to-end field operation for the CUDA spike.  The field remains
// owned by the caller; submitFieldFill records a private stream/event and
// copies the result back into this span before its token becomes complete.
class CudaComputeBackend final : public ComputeService {
public:
    CudaComputeBackend();
    ~CudaComputeBackend() override;

    CudaComputeBackend(const CudaComputeBackend&) = delete;
    CudaComputeBackend& operator=(const CudaComputeBackend&) = delete;
    CudaComputeBackend(CudaComputeBackend&&) noexcept;
    CudaComputeBackend& operator=(CudaComputeBackend&&) noexcept;

    [[nodiscard]] ComputeBackend backend() const noexcept override {
        return ComputeBackend::Cuda;
    }

    // The generic callback API cannot safely capture a host lambda as a CUDA
    // kernel.  CUDA work enters through the typed operations below.
    [[nodiscard]] foundation::Result<void, foundation::Error> dispatch(
        const ComputeDispatch&, const ComputeRange&) override;

    [[nodiscard]] static bool compiled() noexcept;
    [[nodiscard]] CudaBackendStatus status() const noexcept;
    [[nodiscard]] const CudaCapabilities& capabilities() const noexcept;

    // Initialization is idempotent.  An absent runtime/device is a normal
    // unavailable state and does not make a CUDA-free application fail.
    [[nodiscard]] foundation::Result<void, foundation::Error> initialize() noexcept;

    [[nodiscard]] foundation::Result<CudaCompletionToken, foundation::Error>
    submitFieldFill(std::span<std::uint8_t> field, std::uint8_t value);

    [[nodiscard]] foundation::Result<CudaCompletion, foundation::Error>
    poll(CudaCompletionToken token);

    [[nodiscard]] foundation::Result<void, foundation::Error>
    wait(CudaCompletionToken token);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] const char* toString(CudaBackendStatus status) noexcept;
[[nodiscard]] const char* toString(CudaCompletionStatus status) noexcept;

} // namespace genomes::compute
