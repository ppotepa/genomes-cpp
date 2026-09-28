#include "tools/benchmark/thread_scaling_gate.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace genomes::benchmark::thread_scaling {
namespace {

constexpr double kEpsilon = 1.0e-12;

[[nodiscard]] double percentile(std::vector<double> values, const double quantile) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    if (values.size() == 1) {
        return values.front();
    }
    const double position = quantile * static_cast<double>(values.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = static_cast<std::size_t>(std::ceil(position));
    const double fraction = position - static_cast<double>(lower);
    return values[lower] + (values[upper] - values[lower]) * fraction;
}

[[nodiscard]] std::uint64_t mixSeed(std::uint64_t value) noexcept {
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

[[nodiscard]] std::uint64_t sampleSeed(const RunConfig& config,
                                       const std::uint32_t worker_count,
                                       const std::size_t repetition,
                                       const bool warmup) noexcept {
    // The logical workload seed is shared by worker counts.  This is what
    // makes semantic hashes comparable; worker_count remains an execution
    // parameter and must not alter the input dataset.
    static_cast<void>(worker_count);
    auto value = config.seed;
    value ^= static_cast<std::uint64_t>(repetition) * 0xA0761D6478BD642FULL;
    if (warmup) {
        value ^= 0xE7037ED1A0B428DBULL;
    }
    return mixSeed(value);
}

[[nodiscard]] bool validDuration(const double value) noexcept {
    return std::isfinite(value) && value > 0.0;
}

[[nodiscard]] bool validTelemetry(const SchedulerTelemetry& telemetry) noexcept {
    return std::isfinite(telemetry.queue_latency_ms) &&
           std::isfinite(telemetry.idle_ms) &&
           std::isfinite(telemetry.critical_path_ms) &&
           telemetry.queue_latency_ms >= 0.0 && telemetry.idle_ms >= 0.0 &&
           telemetry.critical_path_ms >= 0.0;
}

[[nodiscard]] const BaselineWorker* findBaselineWorker(const Baseline& baseline,
                                                       const std::uint32_t worker_count) noexcept {
    const auto found = std::find_if(baseline.workers.begin(), baseline.workers.end(),
                                    [worker_count](const BaselineWorker& worker) {
                                        return worker.worker_count == worker_count;
                                    });
    return found == baseline.workers.end() ? nullptr : &*found;
}

[[nodiscard]] std::string quote(const std::string& value) {
    std::ostringstream output;
    output << '"';
    for (const char character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default: output << character; break;
        }
    }
    output << '"';
    return output.str();
}

// The baseline schema is deliberately small, but a proper recursive reader
// makes malformed or reordered JSON fail safely rather than silently changing
// a gate.  Only the subset needed by the versioned schema is retained.
class JsonValue final {
public:
    enum class Type { Null, Boolean, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string number_text;
    std::string string;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue> object;
};

class JsonReader final {
public:
    explicit JsonReader(const std::string_view source) : source_(source) {}

    [[nodiscard]] bool parse(JsonValue& value, std::string& error) {
        skipWhitespace();
        if (!parseValue(value, error)) {
            return false;
        }
        skipWhitespace();
        if (position_ != source_.size()) {
            error = "unexpected trailing JSON at offset " + std::to_string(position_);
            return false;
        }
        return true;
    }

private:
    [[nodiscard]] bool parseValue(JsonValue& value, std::string& error) {
        skipWhitespace();
        if (position_ >= source_.size()) {
            error = "unexpected end of JSON";
            return false;
        }
        switch (source_[position_]) {
        case '{': return parseObject(value, error);
        case '[': return parseArray(value, error);
        case '"':
            value.type = JsonValue::Type::String;
            return parseString(value.string, error);
        case 't': return parseLiteral("true", JsonValue::Type::Boolean, value, error);
        case 'f': return parseLiteral("false", JsonValue::Type::Boolean, value, error);
        case 'n': return parseLiteral("null", JsonValue::Type::Null, value, error);
        default: return parseNumber(value, error);
        }
    }

    [[nodiscard]] bool parseObject(JsonValue& value, std::string& error) {
        value.type = JsonValue::Type::Object;
        ++position_;
        skipWhitespace();
        if (consume('}')) {
            return true;
        }
        while (position_ < source_.size()) {
            std::string key;
            if (!parseString(key, error)) {
                return false;
            }
            skipWhitespace();
            if (!consume(':')) {
                error = "expected ':' after object key at offset " + std::to_string(position_);
                return false;
            }
            JsonValue child;
            if (!parseValue(child, error)) {
                return false;
            }
            if (!value.object.emplace(std::move(key), std::move(child)).second) {
                error = "duplicate object key";
                return false;
            }
            skipWhitespace();
            if (consume('}')) {
                return true;
            }
            if (!consume(',')) {
                error = "expected ',' or '}' at offset " + std::to_string(position_);
                return false;
            }
            skipWhitespace();
        }
        error = "unterminated object";
        return false;
    }

    [[nodiscard]] bool parseArray(JsonValue& value, std::string& error) {
        value.type = JsonValue::Type::Array;
        ++position_;
        skipWhitespace();
        if (consume(']')) {
            return true;
        }
        while (position_ < source_.size()) {
            JsonValue child;
            if (!parseValue(child, error)) {
                return false;
            }
            value.array.push_back(std::move(child));
            skipWhitespace();
            if (consume(']')) {
                return true;
            }
            if (!consume(',')) {
                error = "expected ',' or ']' at offset " + std::to_string(position_);
                return false;
            }
            skipWhitespace();
        }
        error = "unterminated array";
        return false;
    }

    [[nodiscard]] bool parseString(std::string& value, std::string& error) {
        if (!consume('"')) {
            error = "expected string at offset " + std::to_string(position_);
            return false;
        }
        value.clear();
        while (position_ < source_.size()) {
            const char character = source_[position_++];
            if (character == '"') {
                return true;
            }
            if (character == '\\') {
                if (position_ >= source_.size()) {
                    error = "unterminated string escape";
                    return false;
                }
                const char escaped = source_[position_++];
                switch (escaped) {
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case '/': value.push_back('/'); break;
                case 'b': value.push_back('\b'); break;
                case 'f': value.push_back('\f'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default:
                    error = "unsupported string escape at offset " +
                            std::to_string(position_ - 1);
                    return false;
                }
            } else if (static_cast<unsigned char>(character) < 0x20U) {
                error = "control character in string";
                return false;
            } else {
                value.push_back(character);
            }
        }
        error = "unterminated string";
        return false;
    }

    [[nodiscard]] bool parseNumber(JsonValue& value, std::string& error) {
        const auto begin = position_;
        if (source_[position_] == '-') {
            ++position_;
        }
        if (position_ >= source_.size()) {
            error = "incomplete number";
            return false;
        }
        if (source_[position_] == '0') {
            ++position_;
        } else if (source_[position_] >= '1' && source_[position_] <= '9') {
            while (position_ < source_.size() && source_[position_] >= '0' &&
                   source_[position_] <= '9') {
                ++position_;
            }
        } else {
            error = "invalid value at offset " + std::to_string(position_);
            return false;
        }
        if (position_ < source_.size() && source_[position_] == '.') {
            ++position_;
            const auto fraction_begin = position_;
            while (position_ < source_.size() && source_[position_] >= '0' &&
                   source_[position_] <= '9') {
                ++position_;
            }
            if (fraction_begin == position_) {
                error = "fraction has no digits";
                return false;
            }
        }
        if (position_ < source_.size() &&
            (source_[position_] == 'e' || source_[position_] == 'E')) {
            ++position_;
            if (position_ < source_.size() &&
                (source_[position_] == '+' || source_[position_] == '-')) {
                ++position_;
            }
            const auto exponent_begin = position_;
            while (position_ < source_.size() && source_[position_] >= '0' &&
                   source_[position_] <= '9') {
                ++position_;
            }
            if (exponent_begin == position_) {
                error = "exponent has no digits";
                return false;
            }
        }
        try {
            value.type = JsonValue::Type::Number;
            value.number_text = std::string(source_.substr(begin, position_ - begin));
            value.number = std::stod(value.number_text);
        } catch (const std::exception&) {
            error = "invalid number at offset " + std::to_string(begin);
            return false;
        }
        if (!std::isfinite(value.number)) {
            error = "number is not finite";
            return false;
        }
        return true;
    }

    [[nodiscard]] bool parseLiteral(const std::string_view literal,
                                    const JsonValue::Type type,
                                    JsonValue& value,
                                    std::string& error) {
        if (source_.substr(position_, literal.size()) != literal) {
            error = "invalid literal at offset " + std::to_string(position_);
            return false;
        }
        position_ += literal.size();
        value.type = type;
        value.boolean = literal == "true";
        return true;
    }

    void skipWhitespace() noexcept {
        while (position_ < source_.size()) {
            const char character = source_[position_];
            if (character != ' ' && character != '\n' && character != '\r' && character != '\t') {
                break;
            }
            ++position_;
        }
    }

    [[nodiscard]] bool consume(const char expected) noexcept {
        if (position_ >= source_.size() || source_[position_] != expected) {
            return false;
        }
        ++position_;
        return true;
    }

    std::string_view source_;
    std::size_t position_ = 0;
};

[[nodiscard]] const JsonValue* objectValue(const JsonValue& object,
                                            const std::string_view key) noexcept {
    if (object.type != JsonValue::Type::Object) {
        return nullptr;
    }
    const auto found = object.object.find(std::string(key));
    return found == object.object.end() ? nullptr : &found->second;
}

[[nodiscard]] bool requireString(const JsonValue& object,
                                 const std::string_view key,
                                 std::string& result,
                                 std::string& error) {
    const auto* value = objectValue(object, key);
    if (value == nullptr || value->type != JsonValue::Type::String) {
        error = "baseline field '" + std::string(key) + "' must be a string";
        return false;
    }
    result = value->string;
    return true;
}

[[nodiscard]] bool requireNumber(const JsonValue& object,
                                 const std::string_view key,
                                 double& result,
                                 std::string& error) {
    const auto* value = objectValue(object, key);
    if (value == nullptr || value->type != JsonValue::Type::Number ||
        !std::isfinite(value->number)) {
        error = "baseline field '" + std::string(key) + "' must be a finite number";
        return false;
    }
    result = value->number;
    return true;
}

[[nodiscard]] bool requireUnsigned(const JsonValue& object,
                                   const std::string_view key,
                                   std::uint64_t& result,
                                   std::string& error) {
    const auto* json_value = objectValue(object, key);
    if (json_value == nullptr || json_value->type != JsonValue::Type::Number ||
        json_value->number_text.find_first_of(".-+eE") != std::string::npos) {
        error = "baseline field '" + std::string(key) + "' must be an unsigned integer";
        return false;
    }
    try {
        std::size_t consumed = 0;
        result = std::stoull(json_value->number_text, &consumed, 10);
        if (consumed != json_value->number_text.size()) {
            error = "baseline field '" + std::string(key) + "' must be an unsigned integer";
            return false;
        }
    } catch (const std::exception&) {
        error = "baseline field '" + std::string(key) + "' is outside the unsigned range";
        return false;
    }
    return true;
}

[[nodiscard]] bool requireObject(const JsonValue& parent,
                                 const std::string_view key,
                                 const JsonValue*& result,
                                 std::string& error) {
    result = objectValue(parent, key);
    if (result == nullptr || result->type != JsonValue::Type::Object) {
        error = "baseline field '" + std::string(key) + "' must be an object";
        return false;
    }
    return true;
}

[[nodiscard]] bool parseTrend(const std::string& value, ExpectedTrend& trend) {
    if (value == "no_assumption") {
        trend = ExpectedTrend::NoAssumption;
        return true;
    }
    if (value == "parallel_or_flat") {
        trend = ExpectedTrend::ParallelOrFlat;
        return true;
    }
    if (value == "saturating") {
        trend = ExpectedTrend::Saturating;
        return true;
    }
    return false;
}

[[nodiscard]] const char* statusName(const GateStatus status) noexcept {
    switch (status) {
    case GateStatus::Pass: return "pass";
    case GateStatus::NoBaseline: return "no_baseline";
    case GateStatus::InvalidRun: return "invalid_run";
    case GateStatus::CorrectnessFailure: return "correctness_failure";
    case GateStatus::NoisyRun: return "noisy_run";
    case GateStatus::Regression: return "regression";
    }
    return "invalid_run";
}

[[nodiscard]] std::string formatNumber(const double value) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(4) << value;
    return output.str();
}

} // namespace

bool MachineFingerprint::operator==(const MachineFingerprint& other) const noexcept {
    return machine_class == other.machine_class && compiler == other.compiler &&
           dependency_profile == other.dependency_profile;
}

Distribution summarize(const std::vector<double>& values) {
    Distribution result;
    if (values.empty()) {
        return result;
    }
    if (!std::all_of(values.begin(), values.end(), validDuration)) {
        return result;
    }
    result.sample_count = values.size();
    result.minimum_ms = *std::min_element(values.begin(), values.end());
    result.maximum_ms = *std::max_element(values.begin(), values.end());
    result.mean_ms = std::accumulate(values.begin(), values.end(), 0.0) /
                     static_cast<double>(values.size());
    const double variance = std::accumulate(values.begin(), values.end(), 0.0,
                                            [mean = result.mean_ms](const double total,
                                                                    const double value) {
                                                const double delta = value - mean;
                                                return total + delta * delta;
                                            }) /
                            static_cast<double>(values.size());
    result.standard_deviation_ms = std::sqrt(variance);
    result.coefficient_of_variation = result.mean_ms > kEpsilon
                                          ? result.standard_deviation_ms / result.mean_ms
                                          : 0.0;
    result.median_ms = percentile(values, 0.50);
    result.p95_ms = percentile(values, 0.95);
    result.p99_ms = percentile(values, 0.99);
    return result;
}

RunResult runDeterministic(const RunConfig& input_config,
                           const DeterministicWorkload& workload) {
    RunResult result;
    result.config = input_config;
    if (!workload) {
        result.error = "workload callback is empty";
        return result;
    }
    if (input_config.repetitions == 0) {
        result.error = "repetitions must be greater than zero";
        return result;
    }
    if (input_config.worker_counts.empty()) {
        result.error = "at least one worker count is required";
        return result;
    }
    std::sort(result.config.worker_counts.begin(), result.config.worker_counts.end());
    result.config.worker_counts.erase(
        std::unique(result.config.worker_counts.begin(), result.config.worker_counts.end()),
        result.config.worker_counts.end());
    if (std::any_of(result.config.worker_counts.begin(), result.config.worker_counts.end(),
                    [](const std::uint32_t count) { return count == 0; })) {
        result.error = "worker counts must be greater than zero";
        return result;
    }

    std::uint64_t first_hash = 0;
    bool have_hash = false;
    std::map<std::uint32_t, std::vector<double>> timings;
    std::map<std::uint32_t, SchedulerTelemetry> telemetry;
    std::map<std::uint32_t, std::uint64_t> worker_hashes;
    for (const std::uint32_t worker_count : result.config.worker_counts) {
        auto invoke = [&](const std::size_t repetition, const bool warmup) -> bool {
            const auto start = std::chrono::steady_clock::now();
            WorkloadResult measurement;
            try {
                measurement = workload(worker_count,
                                       sampleSeed(result.config, worker_count, repetition, warmup));
            } catch (const std::exception& exception) {
                result.error = std::string("workload threw: ") + exception.what();
                return false;
            } catch (...) {
                result.error = "workload threw a non-standard exception";
                return false;
            }
            const auto elapsed = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start);
            const double elapsed_ms = validDuration(measurement.elapsed_ms)
                                          ? measurement.elapsed_ms
                                          : elapsed.count();
            if (!validDuration(elapsed_ms) || !validTelemetry(measurement.telemetry)) {
                result.error = "workload returned invalid timing or telemetry";
                return false;
            }
            if (!have_hash) {
                first_hash = measurement.semantic_hash;
                have_hash = true;
            }
            const auto hash = measurement.semantic_hash;
            if (hash != first_hash) {
                result.semantic_hash_consistent = false;
            }
            worker_hashes[worker_count] = hash;
            if (!warmup) {
                result.samples.push_back(
                    Sample{worker_count, elapsed_ms, hash, measurement.telemetry});
                timings[worker_count].push_back(elapsed_ms);
                auto& aggregate = telemetry[worker_count];
                aggregate.steals += measurement.telemetry.steals;
                aggregate.failed_steals += measurement.telemetry.failed_steals;
                aggregate.queue_latency_ms += measurement.telemetry.queue_latency_ms;
                aggregate.idle_ms += measurement.telemetry.idle_ms;
                aggregate.critical_path_ms += measurement.telemetry.critical_path_ms;
            }
            return true;
        };
        for (std::size_t repetition = 0; repetition < result.config.warmup_repetitions;
             ++repetition) {
            if (!invoke(repetition, true)) {
                return result;
            }
        }
        for (std::size_t repetition = 0; repetition < result.config.repetitions; ++repetition) {
            if (!invoke(repetition, false)) {
                return result;
            }
        }
    }

    result.semantic_hash = first_hash;
    result.semantic_hash_consistent = true;
    for (const auto& [worker_count, hash] : worker_hashes) {
        static_cast<void>(worker_count);
        if (hash != first_hash) {
            result.semantic_hash_consistent = false;
            break;
        }
    }
    for (const auto& [worker_count, values] : timings) {
        auto aggregate = telemetry[worker_count];
        const double divisor = static_cast<double>(values.size());
        aggregate.queue_latency_ms /= divisor;
        aggregate.idle_ms /= divisor;
        aggregate.critical_path_ms /= divisor;
        result.workers.push_back(
            WorkerDistribution{worker_count, worker_hashes[worker_count], summarize(values),
                               aggregate});
    }
    return result;
}

bool parseBaselineJson(const std::string_view json,
                       Baseline& baseline,
                       std::string& error) {
    JsonValue root;
    JsonReader reader(json);
    if (!reader.parse(root, error)) {
        return false;
    }
    if (root.type != JsonValue::Type::Object) {
        error = "baseline root must be an object";
        return false;
    }
    Baseline parsed;
    if (!requireString(root, "schema", parsed.schema, error) ||
        parsed.schema != "genomes.thread_scaling_baseline.v1" ||
        !requireString(root, "workload", parsed.workload, error)) {
        if (error.empty()) {
            error = "unsupported baseline schema";
        }
        return false;
    }
    const JsonValue* fingerprint = nullptr;
    if (!requireObject(root, "machine", fingerprint, error) ||
        !requireString(*fingerprint, "class", parsed.machine.machine_class, error) ||
        !requireString(*fingerprint, "compiler", parsed.machine.compiler, error) ||
        !requireString(*fingerprint, "dependency_profile", parsed.machine.dependency_profile,
                       error)) {
        return false;
    }
    if (!requireUnsigned(root, "semantic_hash", parsed.semantic_hash, error)) {
        return false;
    }
    const JsonValue* thresholds = nullptr;
    if (!requireObject(root, "thresholds", thresholds, error)) {
        return false;
    }
    if (!requireNumber(*thresholds, "median_regression_percent",
                       parsed.thresholds.median_regression_percent, error) ||
        !requireNumber(*thresholds, "p95_regression_percent",
                       parsed.thresholds.p95_regression_percent, error) ||
        !requireNumber(*thresholds, "maximum_coefficient_of_variation",
                       parsed.thresholds.maximum_coefficient_of_variation, error)) {
        return false;
    }
    std::uint64_t minimum_samples = 0;
    if (!requireUnsigned(*thresholds, "minimum_samples", minimum_samples, error)) {
        return false;
    }
    parsed.thresholds.minimum_samples = static_cast<std::size_t>(minimum_samples);
    if (parsed.thresholds.median_regression_percent < 0.0 ||
        parsed.thresholds.p95_regression_percent < 0.0 ||
        parsed.thresholds.maximum_coefficient_of_variation < 0.0 ||
        parsed.thresholds.minimum_samples == 0) {
        error = "baseline thresholds must be non-negative and minimum_samples must be non-zero";
        return false;
    }
    std::string trend;
    if (!requireString(root, "expected_trend", trend, error) ||
        !parseTrend(trend, parsed.expected_trend)) {
        error = "baseline expected_trend is unknown";
        return false;
    }
    const JsonValue* workers = nullptr;
    if (!requireObject(root, "workers", workers, error) || workers->object.empty()) {
        error = "baseline workers must be a non-empty object";
        return false;
    }
    for (const auto& [key, values] : workers->object) {
        std::size_t consumed = 0;
        std::uint64_t worker_count = 0;
        try {
            worker_count = std::stoull(key, &consumed);
        } catch (const std::exception&) {
            error = "baseline worker key is not an unsigned integer: " + key;
            return false;
        }
        if (consumed != key.size() || worker_count == 0 ||
            worker_count > std::numeric_limits<std::uint32_t>::max() ||
            values.type != JsonValue::Type::Array || values.array.empty()) {
            error = "baseline worker entry is invalid: " + key;
            return false;
        }
        BaselineWorker worker;
        worker.worker_count = static_cast<std::uint32_t>(worker_count);
        for (const auto& value : values.array) {
            if (value.type != JsonValue::Type::Number || !validDuration(value.number)) {
                error = "baseline timing must contain positive finite numbers";
                return false;
            }
            worker.elapsed_ms.push_back(value.number);
        }
        parsed.workers.push_back(std::move(worker));
    }
    std::sort(parsed.workers.begin(), parsed.workers.end(),
              [](const BaselineWorker& left, const BaselineWorker& right) {
                  return left.worker_count < right.worker_count;
              });
    baseline = std::move(parsed);
    return true;
}

bool loadBaselineJson(const std::string& path, Baseline& baseline, std::string& error) {
    std::ifstream input(path, std::ios::in | std::ios::binary);
    if (!input) {
        error = "unable to open baseline: " + path;
        return false;
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.good() && !input.eof()) {
        error = "unable to read baseline: " + path;
        return false;
    }
    return parseBaselineJson(contents.str(), baseline, error);
}

GateResult evaluateGate(const Baseline& baseline, const RunResult& run) {
    GateResult result;
    if (!run.error.empty() || run.samples.empty() || run.workers.empty()) {
        result.status = GateStatus::InvalidRun;
        result.reason = run.error.empty() ? "run has no samples" : run.error;
        return result;
    }
    if (run.config.workload != baseline.workload || run.config.machine != baseline.machine) {
        result.status = GateStatus::NoBaseline;
        result.reason = "workload or machine fingerprint does not match baseline";
        return result;
    }
    if (!run.semantic_hash_consistent || run.semantic_hash != baseline.semantic_hash) {
        result.status = GateStatus::CorrectnessFailure;
        result.reason = "semantic hash differs across worker counts or from baseline";
        return result;
    }

    bool missing_worker = false;
    bool noisy = false;
    bool regression = false;
    for (const auto& measured : run.workers) {
        const auto* baseline_worker = findBaselineWorker(baseline, measured.worker_count);
        if (baseline_worker == nullptr || baseline_worker->elapsed_ms.empty()) {
            missing_worker = true;
            continue;
        }
        WorkerGateResult worker;
        worker.worker_count = measured.worker_count;
        worker.measured = measured.timing;
        worker.baseline = summarize(baseline_worker->elapsed_ms);
        if (worker.measured.sample_count < baseline.thresholds.minimum_samples) {
            worker.reason = "fewer samples than minimum_samples";
            noisy = true;
        } else if (worker.measured.coefficient_of_variation >
                   baseline.thresholds.maximum_coefficient_of_variation) {
            worker.reason = "measured coefficient of variation exceeds noise threshold";
            noisy = true;
        } else {
            const double median_limit = worker.baseline.median_ms *
                                        (1.0 + baseline.thresholds.median_regression_percent / 100.0);
            const double p95_limit = worker.baseline.p95_ms *
                                    (1.0 + baseline.thresholds.p95_regression_percent / 100.0);
            if (worker.measured.median_ms > median_limit + kEpsilon) {
                worker.reason = "median exceeds explicit regression threshold";
                regression = true;
            } else if (worker.measured.p95_ms > p95_limit + kEpsilon) {
                worker.reason = "p95 exceeds explicit regression threshold";
                regression = true;
            } else {
                worker.passed = true;
                worker.reason = "within median and p95 thresholds";
            }
        }
        result.workers.push_back(std::move(worker));
    }
    if (missing_worker) {
        result.status = GateStatus::NoBaseline;
        result.reason = "baseline does not contain every measured worker count";
        return result;
    }
    if (noisy) {
        result.status = GateStatus::NoisyRun;
        result.reason = "timing noise or insufficient repetitions invalidates hard gate";
        return result;
    }
    result.hard_gate = true;
    if (regression) {
        result.status = GateStatus::Regression;
        result.reason = "one or more worker counts exceeded an explicit threshold";
        return result;
    }
    result.status = GateStatus::Pass;
    result.reason = "all worker counts passed; expected trend is informational only";
    return result;
}

std::string renderReport(const RunResult& run, const GateResult* gate) {
    std::ostringstream output;
    output << "thread_scaling_gate\n";
    output << "workload=" << run.config.workload << "\n";
    output << "machine=" << run.config.machine.machine_class << " compiler="
           << run.config.machine.compiler << " dependencies="
           << run.config.machine.dependency_profile << "\n";
    output << "semantic_hash=" << run.semantic_hash
           << " consistent=" << (run.semantic_hash_consistent ? "true" : "false") << "\n";
    if (!run.error.empty()) {
        output << "error=" << run.error << "\n";
    }
    output << "workers\n";
    for (const auto& worker : run.workers) {
        output << "  " << worker.worker_count << " median_ms="
               << formatNumber(worker.timing.median_ms) << " p95_ms="
               << formatNumber(worker.timing.p95_ms) << " p99_ms="
               << formatNumber(worker.timing.p99_ms) << " cv="
               << formatNumber(worker.timing.coefficient_of_variation) << " samples="
               << worker.timing.sample_count << "\n";
    }
    if (gate != nullptr) {
        output << "gate=" << statusName(gate->status)
               << " hard_gate=" << (gate->hard_gate ? "true" : "false")
               << " reason=" << gate->reason << "\n";
        for (const auto& worker : gate->workers) {
            output << "  gate " << worker.worker_count << "="
                   << (worker.passed ? "pass" : "fail") << " (" << worker.reason << ")\n";
        }
    }
    return output.str();
}

std::string renderJsonReport(const RunResult& run, const GateResult* gate) {
    std::ostringstream output;
    output << "{\n  \"schema\": \"genomes.thread_scaling_report.v1\",\n";
    output << "  \"workload\": " << quote(run.config.workload) << ",\n";
    output << "  \"machine\": {\"class\": " << quote(run.config.machine.machine_class)
           << ", \"compiler\": " << quote(run.config.machine.compiler)
           << ", \"dependency_profile\": " << quote(run.config.machine.dependency_profile)
           << "},\n";
    output << "  \"semantic_hash\": " << run.semantic_hash << ",\n";
    output << "  \"semantic_hash_consistent\": "
           << (run.semantic_hash_consistent ? "true" : "false") << ",\n";
    output << "  \"workers\": [\n";
    for (std::size_t index = 0; index < run.workers.size(); ++index) {
        const auto& worker = run.workers[index];
        output << "    {\"worker_count\": " << worker.worker_count
               << ", \"samples\": " << worker.timing.sample_count
               << ", \"median_ms\": " << worker.timing.median_ms
               << ", \"p95_ms\": " << worker.timing.p95_ms
               << ", \"p99_ms\": " << worker.timing.p99_ms
               << ", \"coefficient_of_variation\": "
               << worker.timing.coefficient_of_variation << "}";
        output << (index + 1 == run.workers.size() ? "\n" : ",\n");
    }
    output << "  ]";
    if (gate != nullptr) {
        output << ",\n  \"gate\": {\"status\": " << quote(statusName(gate->status))
               << ", \"hard_gate\": " << (gate->hard_gate ? "true" : "false")
               << ", \"reason\": " << quote(gate->reason) << "}\n";
    } else {
        output << "\n";
    }
    output << "}\n";
    return output.str();
}

} // namespace genomes::benchmark::thread_scaling
