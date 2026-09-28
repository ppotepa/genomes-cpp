#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/jobs/JobSystem.hpp>

#include <cstddef>
#include <functional>

namespace genomes::compute {

enum class ComputeBackend : unsigned char {
    Cpu,
    Diligent,
    Cuda,
};

struct ComputeDispatch final {
    std::size_t element_count{0};
    std::size_t grain_size{256};

    [[nodiscard]] bool valid() const noexcept {
        return element_count > 0 && grain_size > 0;
    }
};

using ComputeRange = std::function<void(std::size_t begin, std::size_t end)>;

class ComputeService {
public:
    virtual ~ComputeService() = default;

    [[nodiscard]] virtual ComputeBackend backend() const noexcept = 0;
    [[nodiscard]] virtual foundation::Result<void, foundation::Error> dispatch(
        const ComputeDispatch&, const ComputeRange&) = 0;
};

// CPU is the correctness path. It uses the shared engine JobSystem when one
// is supplied and falls back to a deterministic single-range call otherwise.
class CpuComputeService final : public ComputeService {
public:
    explicit CpuComputeService(jobs::JobSystem* jobs = nullptr) noexcept : jobs_{jobs} {}

    [[nodiscard]] ComputeBackend backend() const noexcept override {
        return ComputeBackend::Cpu;
    }

    [[nodiscard]] foundation::Result<void, foundation::Error> dispatch(
        const ComputeDispatch&, const ComputeRange&) override;

private:
    jobs::JobSystem* jobs_{nullptr};
};

} // namespace genomes::compute
