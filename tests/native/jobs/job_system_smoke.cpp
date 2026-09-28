#include <genomes/jobs/JobSystem.hpp>

#include <atomic>
#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    genomes::jobs::JobSystem jobs(2);
    std::atomic<std::uint32_t> completed{0};
    std::vector<genomes::jobs::JobHandle> handles;
    handles.reserve(1000);

    for (std::uint32_t index = 0; index < 1000; ++index) {
        handles.push_back(jobs.submit([&completed](genomes::jobs::JobContext& context) {
            assert(context.workerIndex() < 2);
            completed.fetch_add(1, std::memory_order_relaxed);
        }));
    }
    for (const auto& handle : handles) {
        jobs.wait(handle);
        assert(handle.isComplete());
        assert(!handle.wasCanceled());
        assert(!handle.failed());
    }
    assert(completed.load(std::memory_order_relaxed) == 1000);

    genomes::jobs::JobSystem single_worker(1);
    std::atomic<std::uint32_t> nested_completed{0};
    const auto outer = single_worker.submit([&](genomes::jobs::JobContext& context) {
        const auto child = context.system().submit([&nested_completed](genomes::jobs::JobContext&) {
            nested_completed.fetch_add(1, std::memory_order_relaxed);
        });
        context.system().wait(child);
    });
    single_worker.wait(outer);
    assert(outer.isComplete());
    assert(nested_completed.load(std::memory_order_relaxed) == 1);

    genomes::jobs::JobFence fence;
    fence.add(32);
    for (std::uint32_t index = 0; index < 32; ++index) {
        (void)jobs.submit([&fence](genomes::jobs::JobContext&) { fence.signal(); });
    }
    fence.wait();
    assert(fence.pending() == 0);
    return 0;
}
