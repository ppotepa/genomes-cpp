#include <genomes/compute/CudaComputeBackend.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    using namespace genomes::compute;

    CudaComputeBackend backend;
    assert(backend.initialize());
    assert(backend.capabilities().compiled == CudaComputeBackend::compiled());
    assert(backend.status() != CudaBackendStatus::RuntimeError ||
           !backend.capabilities().device_available);

    const auto generic_dispatch = backend.dispatch(
        ComputeDispatch{1U, 1U}, [](std::size_t, std::size_t) {});
    assert(!generic_dispatch);
    assert(generic_dispatch.error().code == genomes::foundation::ErrorCode::Unsupported);

    std::vector<std::uint8_t> field(4096U, 0U);
    const auto submission = backend.submitFieldFill(field, 0x5aU);
    if (!backend.capabilities().device_available) {
        // AUTO/OFF builds and CUDA-enabled machines without an NVIDIA device
        // must retain a clean, inspectable fallback state.
        assert(!submission);
        assert(submission.error().code == genomes::foundation::ErrorCode::Unsupported);
        assert(backend.status() == CudaBackendStatus::Disabled ||
               backend.status() == CudaBackendStatus::Unavailable ||
               backend.status() == CudaBackendStatus::RuntimeError);
        return 0;
    }

    assert(submission);
    assert(submission.value().valid());
    const auto completion = backend.wait(submission.value());
    assert(completion);
    assert(std::all_of(field.begin(), field.end(), [](std::uint8_t value) {
        return value == 0x5aU;
    }));

    // A consumed token is intentionally not reusable; this catches accidental
    // host-side completion bookkeeping that could hide a second synchronization.
    const auto consumed = backend.poll(submission.value());
    assert(!consumed);
    assert(consumed.error().code == genomes::foundation::ErrorCode::NotFound);
    return 0;
}
