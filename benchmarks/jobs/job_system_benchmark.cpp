#include <genomes/jobs/JobSystem.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

namespace {

void run(std::uint32_t worker_count, std::uint32_t job_count) {
    genomes::jobs::JobSystem jobs(worker_count);
    genomes::jobs::JobFence fence;
    fence.add(job_count);

    const auto start = std::chrono::steady_clock::now();
    for (std::uint32_t index = 0; index < job_count; ++index) {
        (void)jobs.submit([&fence](genomes::jobs::JobContext&) { fence.signal(); });
    }
    fence.wait();
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start);
    std::cout << "workers=" << worker_count << " jobs=" << job_count
              << " elapsed_ms=" << elapsed.count() << '\n';
}

} // namespace

int main() {
    constexpr std::uint32_t job_count = 100'000;
    const std::uint32_t hardware = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::uint32_t> worker_counts{1};
    if (hardware >= 2) {
        worker_counts.push_back(2);
    }
    if (hardware >= 4) {
        worker_counts.push_back(4);
    }
    for (const std::uint32_t worker_count : worker_counts) {
        run(worker_count, job_count);
    }
    return 0;
}
