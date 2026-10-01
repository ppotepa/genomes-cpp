#include <genomes/jobs/JobSystem.hpp>

#include <atomic>
#include <cassert>
#include <latch>
#include <thread>

namespace {

void overSignalIsRejectedWithoutMutation() {
    genomes::jobs::JobFence fence(2);
    assert(!fence.signal(3));
    assert(fence.pending() == 2);
    assert(fence.signal());
    assert(fence.pending() == 1);
    assert(fence.signal());
    assert(fence.pending() == 0);
    assert(!fence.signal());
}

void completionWakesEveryWaiter() {
    genomes::jobs::JobFence fence(1);
    std::latch waiters_ready{2};
    std::atomic<unsigned int> awakened{0};
    std::thread first([&] {
        waiters_ready.count_down();
        fence.wait();
        awakened.fetch_add(1, std::memory_order_relaxed);
    });
    std::thread second([&] {
        waiters_ready.count_down();
        fence.wait();
        awakened.fetch_add(1, std::memory_order_relaxed);
    });

    waiters_ready.wait();
    assert(fence.signal());
    first.join();
    second.join();
    assert(awakened.load(std::memory_order_relaxed) == 2);
}

} // namespace

int main() {
    overSignalIsRejectedWithoutMutation();
    completionWakesEveryWaiter();
    return 0;
}
