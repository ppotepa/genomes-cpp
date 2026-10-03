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
    return 2;
}
