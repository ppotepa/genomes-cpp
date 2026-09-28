#include "tools/benchmark/thread_scaling_gate.hpp"

#include <cassert>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#ifndef GENOMES_SOURCE_DIR
#define GENOMES_SOURCE_DIR "."
#endif

namespace {

using namespace genomes::benchmark::thread_scaling;

constexpr std::uint64_t kSemanticHash = 0xC0FFEE1234567890ULL;

[[nodiscard]] std::string fixturePath() {
    return std::string(GENOMES_SOURCE_DIR) +
           "/config/performance/thread_scaling_baselines/job_system_reference.json";
}

RunResult fixtureRun(const std::vector<std::uint32_t> workers,
                     const std::size_t repetitions = 5) {
    RunConfig config;
    config.workload = "deterministic_job_batch";
    config.machine = MachineFingerprint{"reference", "portable", "native"};
    config.worker_counts = workers;
    config.repetitions = repetitions;
    config.warmup_repetitions = 0;
    config.seed = 17;
    return runDeterministic(config, [](const std::uint32_t worker_count,
                                       const std::uint64_t seed) {
        static_cast<void>(seed);
        const double elapsed = worker_count == 1 ? 10.0 : worker_count == 2 ? 8.0 : 7.0;
        return WorkloadResult{elapsed, kSemanticHash, {}};
    });
}

} // namespace

int main() {
    const auto distribution = summarize({10.0, 12.0, 11.0, 9.0, 8.0});
    assert(distribution.sample_count == 5);
    assert(distribution.minimum_ms == 8.0);
    assert(distribution.maximum_ms == 12.0);
    assert(distribution.median_ms == 10.0);
    assert(distribution.p95_ms > 11.0 && distribution.p95_ms < 12.0);
    assert(distribution.p99_ms > distribution.p95_ms);

    Baseline baseline;
    std::string error;
    assert(loadBaselineJson(fixturePath(), baseline, error));
    assert(error.empty());
    assert(baseline.expected_trend == ExpectedTrend::NoAssumption);
    assert(baseline.thresholds.median_regression_percent == 10.0);
    assert(baseline.workers.size() == 3);

    const auto pass_run = fixtureRun({1, 2, 4});
    assert(pass_run.error.empty());
    assert(pass_run.semantic_hash_consistent);
    const auto pass = evaluateGate(baseline, pass_run);
    assert(pass.status == GateStatus::Pass);
    assert(pass.hard_gate);
    assert(pass.workers.size() == 3);

    // This deliberately permits a flat/slower curve when each worker count
    // remains inside its own baseline band; no linear speedup is assumed.
    const auto no_speedup_run = [&]() {
        RunConfig config = pass_run.config;
        config.worker_counts = {1, 2, 4};
        return runDeterministic(config, [](const std::uint32_t, const std::uint64_t) {
            return WorkloadResult{10.0, kSemanticHash, {}};
        });
    }();
    assert(evaluateGate(baseline, no_speedup_run).status == GateStatus::Pass);

    RunConfig regression_config = pass_run.config;
    const auto regression_run = runDeterministic(
        regression_config, [](const std::uint32_t worker_count, const std::uint64_t) {
            return WorkloadResult{worker_count == 2 ? 12.0 : 10.0, kSemanticHash, {}};
        });
    assert(evaluateGate(baseline, regression_run).status == GateStatus::Regression);

    const auto correctness_run = runDeterministic(
        regression_config, [](const std::uint32_t worker_count, const std::uint64_t) {
            return WorkloadResult{10.0, kSemanticHash + worker_count, {}};
        });
    assert(evaluateGate(baseline, correctness_run).status == GateStatus::CorrectnessFailure);

    const auto noisy_run = runDeterministic(
        regression_config, [](const std::uint32_t worker_count, const std::uint64_t seed) {
            static_cast<void>(worker_count);
            return WorkloadResult{(seed & 1U) == 0U ? 1.0 : 10.0,
                                  kSemanticHash,
                                  {}};
        });
    assert(evaluateGate(baseline, noisy_run).status == GateStatus::NoisyRun);

    RunConfig unknown_machine_config = pass_run.config;
    unknown_machine_config.machine.machine_class = "unlisted-machine";
    const auto unknown_machine_run = runDeterministic(
        unknown_machine_config, [](const std::uint32_t, const std::uint64_t) {
            return WorkloadResult{10.0, kSemanticHash, {}};
        });
    assert(evaluateGate(baseline, unknown_machine_run).status == GateStatus::NoBaseline);

    const auto text_report = renderReport(pass_run, &pass);
    assert(text_report.find("gate=pass") != std::string::npos);
    const auto json_report = renderJsonReport(pass_run, &pass);
    assert(json_report.find("genomes.thread_scaling_report.v1") != std::string::npos);
    return 0;
}
