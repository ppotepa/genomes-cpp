#include "tools/benchmark/thread_scaling_gate.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

using genomes::benchmark::thread_scaling::Baseline;
using genomes::benchmark::thread_scaling::GateStatus;
using genomes::benchmark::thread_scaling::MachineFingerprint;
using genomes::benchmark::thread_scaling::RunConfig;
using genomes::benchmark::thread_scaling::WorkloadResult;

[[nodiscard]] std::uint64_t mix(std::uint64_t value) noexcept {
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

[[nodiscard]] bool parseUnsigned(const std::string& text, std::uint32_t& result) {
    try {
        std::size_t consumed = 0;
        const auto value = std::stoull(text, &consumed);
        if (consumed != text.size() || value == 0 ||
            value > std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }
        result = static_cast<std::uint32_t>(value);
        return true;
    } catch (...) {
        return false;
    }
}

[[nodiscard]] bool parseWorkers(const std::string& text, std::vector<std::uint32_t>& workers) {
    std::stringstream input(text);
    std::string item;
    while (std::getline(input, item, ',')) {
        std::uint32_t worker_count = 0;
        if (!parseUnsigned(item, worker_count)) {
            return false;
        }
        workers.push_back(worker_count);
    }
    std::sort(workers.begin(), workers.end());
    workers.erase(std::unique(workers.begin(), workers.end()), workers.end());
    return !workers.empty();
}

void printUsage() {
    std::cout << "Usage: genomes_thread_scaling_gate [options]\n"
              << "  --workers 1,2,4       worker counts (default: 1,2,4)\n"
              << "  --repetitions N       measured samples per worker (default: 5)\n"
              << "  --warmups N           warmup samples (default: 1)\n"
              << "  --seed N              deterministic workload seed\n"
              << "  --baseline PATH       versioned JSON baseline for a hard gate\n"
              << "  --machine CLASS       baseline machine class (default: reference)\n"
              << "  --compiler NAME       compiler profile (default: portable)\n"
              << "  --dependencies NAME   dependency profile (default: native)\n"
              << "  --json                emit machine-readable report\n"
              << "  --report PATH         write report to PATH\n";
}

[[nodiscard]] WorkloadResult runDeterministicWorkload(const std::uint32_t worker_count,
                                                       const std::uint64_t seed) {
    constexpr std::uint32_t item_count = 131'072;
    std::vector<std::uint64_t> partial(worker_count, 0);
    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    for (std::uint32_t worker = 0; worker < worker_count; ++worker) {
        workers.emplace_back([&, worker]() {
            const std::uint32_t begin = item_count * worker / worker_count;
            const std::uint32_t end = item_count * (worker + 1) / worker_count;
            std::uint64_t value = 0;
            for (std::uint32_t index = begin; index < end; ++index) {
                value ^= mix(seed ^ static_cast<std::uint64_t>(index));
            }
            partial[worker] = value;
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    std::uint64_t semantic_hash = 0;
    for (const auto value : partial) {
        semantic_hash ^= value;
    }
    return WorkloadResult{0.0, semantic_hash, {}};
}

} // namespace

int main(int argc, char** argv) {
    RunConfig config;
    config.workload = "deterministic_partitioned_batch";
    config.machine = MachineFingerprint{"reference", "portable", "native"};
    config.worker_counts = {1, 2, 4};

    std::string baseline_path;
    bool json = false;
    std::string report_path;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        auto valueFor = [&](const std::string& name, std::string& value) -> bool {
            if (argument == name && index + 1 < argc) {
                value = argv[++index];
                return true;
            }
            return false;
        };
        std::string value;
        if (argument == "--help" || argument == "-h") {
            printUsage();
            return 0;
        }
        if (valueFor("--workers", value)) {
            config.worker_counts.clear();
            if (!parseWorkers(value, config.worker_counts)) {
                std::cerr << "invalid --workers value\n";
                return 2;
            }
        } else if (valueFor("--repetitions", value)) {
            try {
                config.repetitions = std::stoull(value);
            } catch (...) {
                config.repetitions = 0;
            }
            if (config.repetitions == 0) {
                std::cerr << "--repetitions must be greater than zero\n";
                return 2;
            }
        } else if (valueFor("--warmups", value)) {
            try {
                config.warmup_repetitions = std::stoull(value);
            } catch (...) {
                config.warmup_repetitions = 0;
            }
        } else if (valueFor("--seed", value)) {
            try {
                config.seed = std::stoull(value);
            } catch (...) {
                std::cerr << "invalid --seed value\n";
                return 2;
            }
        } else if (valueFor("--baseline", baseline_path)) {
        } else if (valueFor("--machine", config.machine.machine_class)) {
        } else if (valueFor("--compiler", config.machine.compiler)) {
        } else if (valueFor("--dependencies", config.machine.dependency_profile)) {
        } else if (argument == "--json") {
            json = true;
        } else if (valueFor("--report", report_path)) {
        } else {
            std::cerr << "unknown argument: " << argument << "\n";
            printUsage();
            return 2;
        }
    }

    const auto run = genomes::benchmark::thread_scaling::runDeterministic(
        config, runDeterministicWorkload);
    Baseline baseline;
    std::string error;
    const auto workload_report = [&]() {
        return genomes::benchmark::thread_scaling::renderJsonReport(run);
    };
    std::string report;
    GateStatus status = GateStatus::Pass;
    if (!baseline_path.empty()) {
        if (!genomes::benchmark::thread_scaling::loadBaselineJson(baseline_path, baseline,
                                                                   error)) {
            std::cerr << error << '\n';
            return 2;
        }
        const auto gate = genomes::benchmark::thread_scaling::evaluateGate(baseline, run);
        status = gate.status;
        report = json ? genomes::benchmark::thread_scaling::renderJsonReport(run, &gate)
                      : genomes::benchmark::thread_scaling::renderReport(run, &gate);
        if (gate.status == GateStatus::Regression ||
            gate.status == GateStatus::CorrectnessFailure ||
            gate.status == GateStatus::InvalidRun) {
            if (json) {
                std::cout << report;
            } else {
                std::cerr << report;
            }
            return 1;
        }
    } else {
        report = json ? workload_report() : genomes::benchmark::thread_scaling::renderReport(run);
    }
    static_cast<void>(status);
    if (!report_path.empty()) {
        std::ofstream output(report_path, std::ios::out | std::ios::trunc);
        if (!output) {
            std::cerr << "unable to write report: " << report_path << '\n';
            return 2;
        }
        output << report;
    }
    std::cout << report;
    return 0;
}
