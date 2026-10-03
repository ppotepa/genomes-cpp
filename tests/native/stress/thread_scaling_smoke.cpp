#include "tools/benchmark/thread_scaling_gate.hpp"

#include <genomes/jobs/JobSystem.hpp>

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
    config.workload = JobSystemWorkload;
    config.machine = MachineFingerprint{"synthetic-gate-fixture", "portable", "native"};
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
        regression_config, [sample = 0U](const std::uint32_t, const std::uint64_t) mutable {
            return WorkloadResult{(++sample & 1U) == 0U ? 1.0 : 10.0,
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

    // A bad intermediate sample must not be erased by a good final sample,
    // including when the anomaly happens during warmup.
    for (const std::size_t bad_sample : {1U, 2U}) {
        auto config = pass_run.config;
        config.warmup_repetitions = 2;
        const auto inconsistent = runDeterministic(config,
            [sample = std::size_t{0}, bad_sample, seed = config.seed]
            (std::uint32_t, std::uint64_t actual_seed) mutable {
                assert(actual_seed == seed);
                return WorkloadResult{10.0, kSemanticHash + (sample++ == bad_sample ? 1U : 0U), {}};
            });
        assert(!inconsistent.semantic_hash_consistent);
        assert(evaluateGate(baseline, inconsistent).status == GateStatus::CorrectnessFailure);
        auto unrelated = baseline;
        unrelated.machine.machine_class = "different-machine";
        assert(evaluateGate(unrelated, inconsistent).status == GateStatus::CorrectnessFailure);
    }

    // Exercise the same workload as the CLI against a serial scheduler oracle.
    genomes::jobs::JobSystem serial(genomes::jobs::SchedulerConfig{
        .mode = genomes::jobs::SchedulerMode::Serial, .enable_io_worker = false});
    const auto reference = runJobSystemWorkload(serial, 17);
    assert(reference.elapsed_ms > 0.0);
    assert(runJobSystemWorkload(serial, 18).semantic_hash != reference.semantic_hash);
    for (const std::uint32_t workers : {1U, 2U, 4U}) {
        genomes::jobs::JobSystem scheduler(genomes::jobs::SchedulerConfig{
            .worker_count = workers, .enable_io_worker = false});
        auto config = pass_run.config;
        config.worker_counts = {workers};
        config.warmup_repetitions = 1;
        config.repetitions = 2;
        const auto measured = runDeterministic(config,
            [&](std::uint32_t, std::uint64_t seed) { return runJobSystemWorkload(scheduler, seed); });
        assert(measured.error.empty());
        assert(measured.semantic_hash_consistent);
        assert(measured.semantic_hash == reference.semantic_hash);
        assert(scheduler.telemetry().completed > 0);
    }

    const auto text_report = renderReport(pass_run, &pass);
    assert(text_report.find("gate=pass") != std::string::npos);
    const auto json_report = renderJsonReport(pass_run, &pass);
    assert(json_report.find("genomes.thread_scaling_report.v1") != std::string::npos);
    assert(json_report.find("\"elapsed_ms\"") != std::string::npos);
    assert(json_report.find("\"semantic_hash_consistent\": true") != std::string::npos);
    return 0;
}
