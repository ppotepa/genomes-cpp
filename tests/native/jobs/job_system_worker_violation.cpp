#include <genomes/jobs/JobSystem.hpp>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string_view>

namespace {

constexpr int kExpectedTerminateExitCode = 86;

[[noreturn]] void expectedTerminate() noexcept {
    constexpr std::string_view marker = "genomes.jobs.worker_shutdown.terminate\n";
    (void)std::fwrite(marker.data(), 1, marker.size(), stdout);
    (void)std::fflush(stdout);
    std::_Exit(kExpectedTerminateExitCode);
}

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
    std::set_terminate(expectedTerminate);
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
