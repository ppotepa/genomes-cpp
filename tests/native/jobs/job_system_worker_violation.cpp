#include <genomes/jobs/JobSystem.hpp>

#include <cstdlib>
#include <string_view>

namespace {

void shutdownFromWorker() {
    genomes::jobs::JobSystem jobs(1);
    const auto handle = jobs.submit([&jobs](genomes::jobs::JobContext&) {
        jobs.shutdown(genomes::jobs::ShutdownMode::Drain);
    });
    jobs.wait(handle);
}

void destroyFromWorker() {
    // The process is expected to terminate from the worker before the
    // intentionally leaked allocation can be reclaimed by the owner thread.
    using JobSystem = genomes::jobs::JobSystem;
    auto* system = new JobSystem(1);
    auto handle = system->submit([system](genomes::jobs::JobContext&) {
        system->~JobSystem();
    });
    handle.wait();
}

void waitForOwnerAffinityFromWorker() {
    genomes::jobs::JobSystem jobs(1);
    const auto main_job = jobs.submit(
        [](genomes::jobs::JobContext&) {},
        {.lane = genomes::jobs::ExecutionLane::Main});
    const auto worker = jobs.submit([&](genomes::jobs::JobContext&) {
        jobs.wait(main_job);
    });
    jobs.wait(worker);
}

void waitForOwnerAffinityGroupFromWorker() {
    genomes::jobs::JobSystem jobs(1);
    const auto worker = jobs.submit([&](genomes::jobs::JobContext&) {
        genomes::jobs::JobGroup group(jobs);
        (void)group.submit(
            [](genomes::jobs::JobContext&) {},
            {.lane = genomes::jobs::ExecutionLane::Render});
        group.wait();
    });
    jobs.wait(worker);
}

void waitForDynamicallyReleasedAffinityCompletionFromWorker() {
    genomes::jobs::JobSystem jobs(1);
    const auto worker = jobs.submit([&](genomes::jobs::JobContext&) {
        genomes::jobs::JobGraphBuilder builder;
        const auto root = builder.add([](genomes::jobs::JobContext&) {});
        const auto owner = builder.add(
            [](genomes::jobs::JobContext&) {},
            {.lane = genomes::jobs::ExecutionLane::Main});
        builder.precedes(root, owner);
        auto completion = std::move(builder).build().start(jobs);
        completion.wait();
    });
    jobs.wait(worker);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    if (std::string_view(argv[1]) == "--shutdown") {
        shutdownFromWorker();
        return 3;
    }
    if (std::string_view(argv[1]) == "--destructor") {
        destroyFromWorker();
        return 3;
    }
    if (std::string_view(argv[1]) == "--affinity-wait") {
        waitForOwnerAffinityFromWorker();
        return 3;
    }
    if (std::string_view(argv[1]) == "--affinity-group-wait") {
        waitForOwnerAffinityGroupFromWorker();
        return 3;
    }
    if (std::string_view(argv[1]) == "--dynamic-affinity-completion-wait") {
        waitForDynamicallyReleasedAffinityCompletionFromWorker();
        return 3;
    }
    return 2;
}
