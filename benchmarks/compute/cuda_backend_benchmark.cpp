#include <genomes/compute/CudaComputeBackend.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

void run(genomes::compute::CudaComputeBackend& backend, std::size_t elements) {
    std::vector<std::uint8_t> field(elements, 0U);
    const auto cpu_begin = Clock::now();
    std::fill(field.begin(), field.end(), 0x5aU);
    const auto cpu_us = std::chrono::duration_cast<std::chrono::microseconds>(
        Clock::now() - cpu_begin);

    std::fill(field.begin(), field.end(), 0U);
    const auto cuda_begin = Clock::now();
    const auto token = backend.submitFieldFill(field, 0x5aU);
    const auto waited = token ? backend.wait(token.value())
                              : genomes::foundation::Result<void, genomes::foundation::Error>::failure(
                                    {genomes::foundation::ErrorCode::Unsupported,
                                     "CUDA submission unavailable"});
    const auto cuda_us = std::chrono::duration_cast<std::chrono::microseconds>(
        Clock::now() - cuda_begin);
    const bool correct = waited && std::all_of(field.begin(), field.end(), [](auto value) {
        return value == 0x5aU;
    });
    std::cout << "cuda_backend elements=" << elements
              << " cpu_us=" << cpu_us.count()
              << " cuda_e2e_us=" << cuda_us.count()
              << " status=" << (correct ? "ok" : "unavailable_or_error") << '\n';
}

} // namespace

int main() {
    genomes::compute::CudaComputeBackend backend;
    std::cout << "cuda_backend compiled="
              << (genomes::compute::CudaComputeBackend::compiled() ? "yes" : "no")
              << " status=" << genomes::compute::toString(backend.status()) << '\n';
    for (const std::size_t elements : {4'096U, 65'536U, 1'048'576U}) {
        run(backend, elements);
    }
    return 0;
}
