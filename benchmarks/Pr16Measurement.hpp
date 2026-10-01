#pragma once

// This is deliberately a small benchmark-side reader.  PR16's JSON is an
// input contract for repeatable measurements, not a runtime configuration
// document and not a performance baseline.  Keeping the reader in the
// benchmark target prevents the product/runtime from acquiring a dependency
// on the measurement fixture.

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <exception>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace genomes::benchmark {

struct Pr16MeasurementPlan final {
    std::string workload_id;
    std::string entry_point;
    std::uint64_t seed{0};
    std::size_t entity_count{0};
    double duration_seconds{0.0};
    std::string detail;
    std::string equipment;
    std::size_t warmup_runs{0};
    std::size_t measured_runs{0};
    std::vector<std::string> required_metrics;
    std::string status;
    std::string baseline_status;

    [[nodiscard]] bool requiresBaseline() const noexcept {
        return baseline_status == "NOT_AVAILABLE";
    }

    [[nodiscard]] bool requiresMetric(std::string_view metric) const noexcept {
        return std::find(required_metrics.begin(), required_metrics.end(), metric) !=
               required_metrics.end();
    }
};

struct Pr16MeasurementLoad final {
    std::optional<Pr16MeasurementPlan> plan;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return plan.has_value(); }
};

namespace detail {

[[nodiscard]] inline bool isPositiveInteger(const nlohmann::json& value) noexcept {
    return value.is_number_unsigned() && value.get<std::uint64_t>() > 0U;
}

[[nodiscard]] inline bool isPositiveNumber(const nlohmann::json& value) noexcept {
    return value.is_number() && value.get<double>() > 0.0 &&
           value.get<double>() <= std::numeric_limits<double>::max();
}

[[nodiscard]] inline bool hasRequiredMetrics(const nlohmann::json& determinism,
                                              std::vector<std::string>& output) {
    if (!determinism.contains("required_metrics") ||
        !determinism.at("required_metrics").is_array()) {
        return false;
    }
    for (const auto& item : determinism.at("required_metrics")) {
        if (!item.is_string()) {
            return false;
        }
        const auto metric = item.get<std::string>();
        if (metric.empty() || std::find(output.begin(), output.end(), metric) != output.end()) {
            return false;
        }
        output.push_back(metric);
    }
    constexpr std::string_view required[] = {"median", "p95", "p99", "max",
                                              "semantic_hash", "allocations", "bytes"};
    return std::all_of(std::begin(required), std::end(required), [&](const auto metric) {
        return std::find(output.begin(), output.end(), metric) != output.end();
    });
}

} // namespace detail

[[nodiscard]] inline Pr16MeasurementLoad loadPr16MeasurementPlan(
    const std::filesystem::path& path, std::string_view workload_id) {
    try {
        std::error_code file_error;
        const auto file_size = std::filesystem::file_size(path, file_error);
        constexpr std::uintmax_t maximum_fixture_bytes = 256U * 1024U;
        if (file_error || file_size == 0U || file_size > maximum_fixture_bytes) {
            return {{}, "measurement fixture exceeds the bounded benchmark read limit"};
        }
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            return {{}, "measurement fixture cannot be opened: " + path.string()};
        }
        const nlohmann::json document = nlohmann::json::parse(input);
        if (!document.is_object() || document.value("schema", "") !=
                                      "genomes.pr16.measurement_inputs.v1" ||
            document.value("status", "") != "PLANNED" ||
            document.value("baseline_status", "") != "NOT_AVAILABLE") {
            return {{}, "measurement fixture is not the planned baseline-required v1 contract"};
        }
        const auto& build = document.at("build");
        if (!build.is_object() || build.value("preset", "") != "release-diligent" ||
            build.value("configuration", "") != "Release" ||
            build.value("backend", "") != "DILIGENT" || build.value("api", "") != "D3D12" ||
            build.value("validation", "") != "OFF") {
            return {{}, "measurement fixture build profile is not release-diligent/D3D12"};
        }
        const auto& determinism = document.at("determinism");
        if (!determinism.is_object() || !determinism.contains("warmup_runs") ||
            !determinism.contains("measured_runs") ||
            !detail::isPositiveInteger(determinism.at("warmup_runs")) ||
            !detail::isPositiveInteger(determinism.at("measured_runs")) ||
            determinism.at("measured_runs").get<std::uint64_t>() < 5U) {
            return {{}, "measurement fixture determinism section is invalid"};
        }
        if (!determinism.contains("seeds") || !determinism.at("seeds").is_array() ||
            determinism.at("seeds").empty() ||
            std::any_of(determinism.at("seeds").begin(), determinism.at("seeds").end(),
                        [](const auto& seed) { return !detail::isPositiveInteger(seed); })) {
            return {{}, "measurement fixture seeds must be nonzero integers"};
        }
        const auto& gate = document.at("acceptance_gate");
        if (!gate.is_object() || gate.value("status", "") != "PLANNED" ||
            !gate.value("requires_user_baseline", false) ||
            !gate.value("d1_must_pass", false) || !gate.value("d2_must_pass", false) ||
            !gate.value("no_optimization_without_baseline", false) ||
            !document.contains("baseline") || !document.at("baseline").is_null()) {
            return {{}, "measurement fixture acceptance gate is not baseline-required"};
        }

        const auto& workloads = document.at("workloads");
        if (!workloads.is_array()) {
            return {{}, "measurement fixture workloads must be an array"};
        }
        const std::string requested_workload{workload_id};
        const auto match = std::find_if(workloads.begin(), workloads.end(), [&](const auto& item) {
            return item.is_object() && item.value("id", "") == requested_workload;
        });
        if (match == workloads.end()) {
            return {{}, "workload is not present in measurement fixture: " +
                             std::string(workload_id)};
        }
        const auto& workload = *match;
        const auto& inputs = workload.at("inputs");
        const auto& correctness = workload.at("d1_correctness");
        const auto& performance = workload.at("d2_performance");
        if (!inputs.is_object() || !correctness.is_object() || !performance.is_object() ||
            !correctness.value("required", false) ||
            !performance.contains("target_p95") || !performance.at("target_p95").is_null() ||
            !performance.contains("allowed_regression_percent") ||
            !performance.at("allowed_regression_percent").is_null() ||
            !inputs.contains("seed") || !detail::isPositiveInteger(inputs.at("seed")) ||
            !inputs.contains("entity_count") || !detail::isPositiveInteger(inputs.at("entity_count")) ||
            !inputs.contains("duration_seconds") ||
            !detail::isPositiveNumber(inputs.at("duration_seconds"))) {
            return {{}, "measurement workload inputs or baseline gate is invalid"};
        }
        Pr16MeasurementPlan result{};
        result.workload_id = std::string(workload_id);
        result.entry_point = workload.value("entry_point", "");
        result.seed = inputs.at("seed").get<std::uint64_t>();
        result.entity_count = inputs.at("entity_count").get<std::size_t>();
        result.duration_seconds = inputs.at("duration_seconds").get<double>();
        result.detail = inputs.value("detail", "");
        result.equipment = inputs.value("equipment", "");
        result.warmup_runs = determinism.at("warmup_runs").get<std::size_t>();
        result.measured_runs = determinism.at("measured_runs").get<std::size_t>();
        result.status = document.at("status").get<std::string>();
        result.baseline_status = document.at("baseline_status").get<std::string>();
        if (!detail::hasRequiredMetrics(determinism, result.required_metrics)) {
            return {{}, "measurement fixture does not declare the complete metric contract"};
        }
        return {std::move(result), {}};
    } catch (const std::exception& exception) {
        return {{}, std::string("measurement fixture parse failure: ") + exception.what()};
    }
}

struct Pr16Samples final {
    std::vector<double> values;

    [[nodiscard]] double percentile(double fraction) const noexcept {
        if (values.empty()) {
            return 0.0;
        }
        const auto index = static_cast<std::size_t>(
            fraction * static_cast<double>(values.size() - 1U));
        return values[index];
    }

    [[nodiscard]] double maximum() const noexcept {
        return values.empty() ? 0.0 : values.back();
    }
};

[[nodiscard]] inline Pr16Samples sortedPr16Samples(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return {std::move(values)};
}

[[nodiscard]] inline std::string pr16MetricList(const Pr16MeasurementPlan& plan) {
    std::string result;
    for (std::size_t index = 0U; index < plan.required_metrics.size(); ++index) {
        if (index != 0U) {
            result += ',';
        }
        result += plan.required_metrics[index];
    }
    return result;
}

} // namespace genomes::benchmark
