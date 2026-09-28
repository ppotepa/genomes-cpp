#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace genomes::benchmark::thread_scaling {

// A fingerprint is intentionally explicit.  A baseline from another machine,
// compiler, or dependency profile is informational and must never become a
// hard regression gate by accident.
struct MachineFingerprint final {
    std::string machine_class;
    std::string compiler;
    std::string dependency_profile;

    [[nodiscard]] bool operator==(const MachineFingerprint& other) const noexcept;
};

struct SchedulerTelemetry final {
    std::uint64_t steals = 0;
    std::uint64_t failed_steals = 0;
    double queue_latency_ms = 0.0;
    double idle_ms = 0.0;
    double critical_path_ms = 0.0;
};

struct Sample final {
    std::uint32_t worker_count = 0;
    double elapsed_ms = 0.0;
    std::uint64_t semantic_hash = 0;
    SchedulerTelemetry telemetry{};
};

struct Distribution final {
    std::size_t sample_count = 0;
    double minimum_ms = 0.0;
    double median_ms = 0.0;
    double p95_ms = 0.0;
    double p99_ms = 0.0;
    double maximum_ms = 0.0;
    double mean_ms = 0.0;
    double standard_deviation_ms = 0.0;
    double coefficient_of_variation = 0.0;
};

[[nodiscard]] Distribution summarize(const std::vector<double>& values);

struct WorkerDistribution final {
    std::uint32_t worker_count = 0;
    std::uint64_t semantic_hash = 0;
    Distribution timing{};
    SchedulerTelemetry telemetry{};
};

struct WorkloadResult final {
    // A workload may provide a deterministic duration for fixture tests.  A
    // non-positive value asks the runner to measure the complete callback,
    // including scheduler, merge, and barrier work, with steady_clock.
    double elapsed_ms = 0.0;
    std::uint64_t semantic_hash = 0;
    SchedulerTelemetry telemetry{};
};

struct RunConfig final {
    std::string workload;
    MachineFingerprint machine;
    std::vector<std::uint32_t> worker_counts;
    std::size_t repetitions = 5;
    std::size_t warmup_repetitions = 1;
    std::uint64_t seed = 0x4D595DF4D0F33173ULL;
};

using DeterministicWorkload =
    std::function<WorkloadResult(std::uint32_t worker_count, std::uint64_t seed)>;

struct RunResult final {
    RunConfig config;
    std::vector<Sample> samples;
    std::vector<WorkerDistribution> workers;
    std::uint64_t semantic_hash = 0;
    bool semantic_hash_consistent = false;
    std::string error;
};

// Worker counts are sorted and de-duplicated before execution.  The callback
// receives a stable seed derived from RunConfig::seed, worker count, warmup,
// and repetition, so the workload does not depend on wall-clock state.
[[nodiscard]] RunResult runDeterministic(const RunConfig& config,
                                         const DeterministicWorkload& workload);

enum class ExpectedTrend {
    NoAssumption,
    ParallelOrFlat,
    Saturating,
};

struct RegressionThresholds final {
    double median_regression_percent = 10.0;
    double p95_regression_percent = 20.0;
    double maximum_coefficient_of_variation = 0.25;
    std::size_t minimum_samples = 3;
};

struct BaselineWorker final {
    std::uint32_t worker_count = 0;
    std::vector<double> elapsed_ms;
};

struct Baseline final {
    std::string schema = "genomes.thread_scaling_baseline.v1";
    std::string workload;
    MachineFingerprint machine;
    std::uint64_t semantic_hash = 0;
    RegressionThresholds thresholds{};
    ExpectedTrend expected_trend = ExpectedTrend::NoAssumption;
    std::vector<BaselineWorker> workers;
};

// Strictly parses the small versioned JSON baseline schema used by this gate.
// No third-party JSON dependency is required by the portable benchmark path.
[[nodiscard]] bool parseBaselineJson(std::string_view json,
                                     Baseline& baseline,
                                     std::string& error);

[[nodiscard]] bool loadBaselineJson(const std::string& path,
                                    Baseline& baseline,
                                    std::string& error);

enum class GateStatus {
    Pass,
    NoBaseline,
    InvalidRun,
    CorrectnessFailure,
    NoisyRun,
    Regression,
};

struct WorkerGateResult final {
    std::uint32_t worker_count = 0;
    Distribution measured{};
    Distribution baseline{};
    bool passed = false;
    std::string reason;
};

struct GateResult final {
    GateStatus status = GateStatus::InvalidRun;
    bool hard_gate = false;
    std::string reason;
    std::vector<WorkerGateResult> workers;

    [[nodiscard]] bool passed() const noexcept { return status == GateStatus::Pass; }
};

// Matching includes workload and the complete machine fingerprint.  A
// mismatch returns NoBaseline, never a false failure against another machine.
[[nodiscard]] GateResult evaluateGate(const Baseline& baseline,
                                      const RunResult& run);

[[nodiscard]] std::string renderReport(const RunResult& run,
                                       const GateResult* gate = nullptr);

[[nodiscard]] std::string renderJsonReport(const RunResult& run,
                                           const GateResult* gate = nullptr);

// Snake-case aliases keep the utility convenient for small standalone tools.
[[nodiscard]] inline Distribution summarize_samples(const std::vector<double>& values) {
    return summarize(values);
}

[[nodiscard]] inline RunResult run_deterministic(const RunConfig& config,
                                                 const DeterministicWorkload& workload) {
    return runDeterministic(config, workload);
}

[[nodiscard]] inline bool parse_baseline_json(std::string_view json,
                                              Baseline& baseline,
                                              std::string& error) {
    return parseBaselineJson(json, baseline, error);
}

[[nodiscard]] inline GateResult evaluate_gate(const Baseline& baseline,
                                              const RunResult& run) {
    return evaluateGate(baseline, run);
}

} // namespace genomes::benchmark::thread_scaling
