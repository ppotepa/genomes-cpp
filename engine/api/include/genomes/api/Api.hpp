#pragma once

#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Time.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/execution/SystemSpec.hpp>
#include <genomes/jobs/Cancellation.hpp>
#include <genomes/jobs/JobSystem.hpp>

#include <algorithm>
#include <bit>
#include <compare>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace genomes::simulation {
struct TickContext;
}

namespace genomes::proc {
class ProceduralRuntime;
}

namespace genomes::api {

using ModuleId = foundation::StableId;
using CapabilityId = foundation::StableId;
using ApiId = foundation::StableId;
using StableId = foundation::StableId;

struct ApiVersion final {
    std::uint16_t major{1};
    std::uint16_t minor{0};
    std::uint16_t patch{0};

    friend constexpr auto operator<=>(const ApiVersion&, const ApiVersion&) noexcept = default;
};

enum class ValueType : std::uint8_t {
    Empty,
    Boolean,
    UnsignedInteger,
    SignedInteger,
    FloatingPoint,
    String,
    Bytes,
    Tuple,
};

// Versioned wire data. It deliberately contains only value storage: no ABI
// dependent objects, pointers, references or std::any can cross an API edge.
struct EncodedValue final {
    static constexpr std::uint16_t CurrentFormat = 1;

    std::uint16_t format_version{CurrentFormat};
    ValueType type{ValueType::Empty};
    std::vector<std::uint8_t> bytes;

    [[nodiscard]] static EncodedValue boolean(bool value) {
        return {CurrentFormat, ValueType::Boolean, {static_cast<std::uint8_t>(value)}};
    }

    [[nodiscard]] static EncodedValue unsignedInteger(std::uint64_t value) {
        return integer(ValueType::UnsignedInteger, value);
    }

    [[nodiscard]] static EncodedValue signedInteger(std::int64_t value) {
        return integer(ValueType::SignedInteger, static_cast<std::uint64_t>(value));
    }

    [[nodiscard]] static EncodedValue floatingPoint(double value) {
        return integer(ValueType::FloatingPoint, std::bit_cast<std::uint64_t>(value));
    }

    [[nodiscard]] static EncodedValue string(std::string_view value) {
        return {CurrentFormat, ValueType::String,
                {value.begin(), value.end()}};
    }

    [[nodiscard]] static EncodedValue binary(std::span<const std::uint8_t> value) {
        return {CurrentFormat, ValueType::Bytes, {value.begin(), value.end()}};
    }

    [[nodiscard]] static EncodedValue tuple(std::span<const EncodedValue> values) {
        if (values.size() > 255U ||
            std::any_of(values.begin(), values.end(), [](const EncodedValue& value) {
                return !value.valid();
            })) {
            return {CurrentFormat, ValueType::Tuple, {}};
        }
        std::vector<std::uint8_t> encoded;
        encoded.reserve(1U + values.size() * 10U);
        encoded.push_back(static_cast<std::uint8_t>(values.size()));
        for (const EncodedValue& value : values) {
            encoded.push_back(static_cast<std::uint8_t>(value.type));
            const auto size = static_cast<std::uint32_t>(value.bytes.size());
            for (std::size_t index = 0U; index < sizeof(size); ++index)
                encoded.push_back(static_cast<std::uint8_t>(size >> (index * 8U)));
            encoded.insert(encoded.end(), value.bytes.begin(), value.bytes.end());
        }
        return {CurrentFormat, ValueType::Tuple, std::move(encoded)};
    }

    [[nodiscard]] bool valid() const noexcept {
        if (format_version != CurrentFormat) return false;
        switch (type) {
        case ValueType::Empty: return bytes.empty();
        case ValueType::Boolean: return bytes.size() == 1U && bytes[0] <= 1U;
        case ValueType::UnsignedInteger:
        case ValueType::SignedInteger:
        case ValueType::FloatingPoint: return bytes.size() == sizeof(std::uint64_t);
        case ValueType::String:
        case ValueType::Bytes: return true;
        case ValueType::Tuple: return decodeTuple(bytes, format_version).has_value();
        }
        return false;
    }

    [[nodiscard]] std::optional<bool> asBoolean() const noexcept {
        return type == ValueType::Boolean && valid()
                   ? std::optional<bool>{bytes[0] != 0U} : std::nullopt;
    }

    [[nodiscard]] std::optional<std::uint64_t> asUnsignedInteger() const noexcept {
        return type == ValueType::UnsignedInteger && valid()
                   ? std::optional<std::uint64_t>{decodeInteger()} : std::nullopt;
    }

    [[nodiscard]] std::optional<std::int64_t> asSignedInteger() const noexcept {
        return type == ValueType::SignedInteger && valid()
                   ? std::optional<std::int64_t>{static_cast<std::int64_t>(decodeInteger())}
                   : std::nullopt;
    }

    [[nodiscard]] std::optional<double> asFloatingPoint() const noexcept {
        return type == ValueType::FloatingPoint && valid()
                   ? std::optional<double>{std::bit_cast<double>(decodeInteger())}
                   : std::nullopt;
    }

    [[nodiscard]] std::optional<std::string_view> asString() const noexcept {
        return type == ValueType::String && valid()
                   ? std::optional<std::string_view>{
                         std::string_view{reinterpret_cast<const char*>(bytes.data()), bytes.size()}}
                   : std::nullopt;
    }

    [[nodiscard]] std::optional<std::vector<EncodedValue>> asTuple() const noexcept {
        if (type != ValueType::Tuple || format_version != CurrentFormat) return std::nullopt;
        return decodeTuple(bytes, format_version);
    }

private:
    [[nodiscard]] std::uint64_t decodeInteger() const noexcept {
        std::uint64_t value{0};
        for (std::size_t index = 0U; index < sizeof(value); ++index) {
            value |= static_cast<std::uint64_t>(bytes[index]) << (index * 8U);
        }
        return value;
    }

    [[nodiscard]] static EncodedValue integer(ValueType type, std::uint64_t value) {
        std::vector<std::uint8_t> bytes(sizeof(value));
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            bytes[index] = static_cast<std::uint8_t>(value >> (index * 8U));
        }
        return {CurrentFormat, type, std::move(bytes)};
    }

    [[nodiscard]] static std::optional<std::vector<EncodedValue>> decodeTuple(
        std::span<const std::uint8_t> encoded, std::uint16_t format_version) noexcept {
        if (encoded.empty()) return std::nullopt;
        const std::size_t count = encoded.front();
        std::size_t cursor = 1U;
        std::vector<EncodedValue> values;
        values.reserve(count);
        for (std::size_t index = 0U; index < count; ++index) {
            if (cursor > encoded.size() || encoded.size() - cursor < 5U) return std::nullopt;
            const std::uint8_t raw_type = encoded[cursor++];
            if (raw_type > static_cast<std::uint8_t>(ValueType::Tuple)) return std::nullopt;
            std::uint32_t byte_count{0U};
            for (std::size_t byte = 0U; byte < sizeof(byte_count); ++byte) {
                byte_count |= static_cast<std::uint32_t>(encoded[cursor++]) << (byte * 8U);
            }
            if (byte_count > encoded.size() - cursor) return std::nullopt;
            EncodedValue value{format_version, static_cast<ValueType>(raw_type),
                               {encoded.begin() + static_cast<std::ptrdiff_t>(cursor),
                                encoded.begin() + static_cast<std::ptrdiff_t>(cursor + byte_count)}};
            if (!value.valid()) return std::nullopt;
            cursor += byte_count;
            values.push_back(std::move(value));
        }
        return cursor == encoded.size() ? std::optional<std::vector<EncodedValue>>{std::move(values)}
                                        : std::nullopt;
    }
};

struct CommandEnvelope final {
    ModuleId module{0};
    ApiId verb{0};
    ApiVersion schema_version{};
    foundation::SimulationTick target_tick{};
    StableId source{0};
    std::uint64_t sequence{0};
    std::uint8_t priority{0};
    EncodedValue payload{};
    // Optional producer-assigned deterministic ordering key.  It is appended
    // so existing aggregate initializers remain source-compatible.
    std::uint64_t stable_order{0};

    [[nodiscard]] friend bool operator<(const CommandEnvelope& left,
                                        const CommandEnvelope& right) noexcept {
        if (left.target_tick.value != right.target_tick.value) {
            return left.target_tick.value < right.target_tick.value;
        }
        if (left.priority != right.priority) return left.priority < right.priority;
        if (left.stable_order != 0U || right.stable_order != 0U) {
            if (left.stable_order != right.stable_order) {
                return left.stable_order < right.stable_order;
            }
        }
        return std::tie(left.source, left.sequence) <
               std::tie(right.source, right.sequence);
    }
};

struct CommandReceipt final {
    bool accepted{false};
    std::uint64_t sequence{0};
    foundation::SimulationTick target_tick{};
};

// Shared ingress used by input, AI and future language adapters. It is the
// only mutable part of the API boundary; consumers drain a deterministic,
// tick-scoped value list before authoritative commit.
class CommandQueue final {
public:
    [[nodiscard]] CommandReceipt enqueue(CommandEnvelope command,
                                         foundation::SimulationTick minimum_tick) {
        std::lock_guard lock(mutex_);
        if (command.module == 0 || command.verb == 0 || !command.payload.valid() ||
            command.target_tick < minimum_tick) {
            ++rejected_stale_;
            return {};
        }
        if (command.sequence == 0) {
            command.sequence = next_sequence_++;
        } else {
            next_sequence_ = std::max(next_sequence_, command.sequence + 1U);
        }
        const CommandReceipt receipt{true, command.sequence, command.target_tick};
        commands_.push_back(std::move(command));
        return receipt;
    }

    [[nodiscard]] std::vector<CommandEnvelope> take(foundation::SimulationTick tick) {
        std::lock_guard lock(mutex_);
        std::vector<CommandEnvelope> ready;
        std::vector<CommandEnvelope> pending;
        ready.reserve(commands_.size());
        pending.reserve(commands_.size());
        for (auto& command : commands_) {
            if (command.target_tick == tick) {
                ready.push_back(std::move(command));
            } else if (command.target_tick > tick) {
                pending.push_back(std::move(command));
            } else {
                ++rejected_stale_;
            }
        }
        commands_ = std::move(pending);
        std::sort(ready.begin(), ready.end());
        return ready;
    }

    [[nodiscard]] std::uint64_t rejectedStale() const noexcept {
        std::lock_guard lock(mutex_);
        return rejected_stale_;
    }

private:
    mutable std::mutex mutex_;
    std::vector<CommandEnvelope> commands_;
    std::uint64_t next_sequence_{1};
    std::uint64_t rejected_stale_{0};
};

struct SnapshotView final {
    foundation::SimulationTick tick{};
    std::uint64_t scene_epoch{0};
    std::uint64_t semantic_hash{0};
    std::span<const std::uint8_t> bytes{};
    // A published runtime view keeps its encoded buffer alive while callers
    // read it. `bytes` remains for zero-copy consumers and compatibility.
    std::shared_ptr<const std::vector<std::uint8_t>> owner{};
};

struct SnapshotEntity final {
    std::uint64_t entity{0};
    float position_x{0.0F};
    float position_y{0.0F};
    float position_z{0.0F};
    float velocity_x{0.0F};
    float velocity_y{0.0F};
    float velocity_z{0.0F};
    float heading{0.0F};
    std::uint32_t flags{0};
};

struct EngineTelemetry final {
    foundation::Nanoseconds simulation_duration{};
    foundation::Nanoseconds presentation_duration{};
    foundation::Nanoseconds animation_duration{};
    foundation::Nanoseconds extraction_duration{};
    foundation::Nanoseconds gpu_duration{};
    jobs::SchedulerTelemetry scheduler{};
    std::uint64_t rejected_stale_snapshots{0U};
    std::uint64_t semantic_hash{0U};
};

// Canonical, ABI-independent snapshot wire format. The returned storage is
// owned by the caller and can safely back SnapshotView::bytes until the next
// published snapshot replaces it.
[[nodiscard]] std::vector<std::uint8_t> encodeSnapshot(
    foundation::SimulationTick tick, std::uint64_t scene_epoch,
    std::uint64_t semantic_hash, std::span<const SnapshotEntity> entities);

struct ApiOperationDescriptor final {
    ApiId id{0};
    ApiVersion schema_version{};
    std::vector<ValueType> arguments;
    std::uint32_t permission{0};
    jobs::ExecutionLane lane{jobs::ExecutionLane::Worker};
    bool deterministic{true};
};

// API introspection and simulation execution share one semantic system
// contract. API-specific schema versioning extends, rather than duplicates,
// the canonical execution::SystemSpec fields.
struct ApiSystemDescriptor final : execution::SystemSpec {
    ApiVersion schema_version{};
};

struct ModuleDescriptor final {
    ModuleId id{0};
    ApiVersion version{};
    std::vector<ModuleId> required_modules;
    std::vector<CapabilityId> required_capabilities;
    std::vector<CapabilityId> provided_capabilities;
};

class ModuleRegistry final {
public:
    [[nodiscard]] foundation::Result<void, foundation::Error> declareCommand(
        ModuleId module, ApiOperationDescriptor descriptor);
    [[nodiscard]] foundation::Result<void, foundation::Error> declareQuery(
        ModuleId module, ApiOperationDescriptor descriptor);
    [[nodiscard]] foundation::Result<void, foundation::Error> declareSystem(
        ModuleId module, ApiSystemDescriptor descriptor);
    [[nodiscard]] foundation::Result<void, foundation::Error> declareResourceRead(
        ModuleId module, CapabilityId resource);
    [[nodiscard]] foundation::Result<void, foundation::Error> declareResourceWrite(
        ModuleId module, CapabilityId resource);
    [[nodiscard]] const ApiOperationDescriptor* findCommand(
        ModuleId module, ApiId id) const noexcept;
    [[nodiscard]] const ApiOperationDescriptor* findQuery(
        ModuleId module, ApiId id) const noexcept;
    [[nodiscard]] const ApiSystemDescriptor* findSystem(ApiId id) const noexcept;

    [[nodiscard]] const std::vector<std::pair<ModuleId, ApiOperationDescriptor>>&
    commands() const noexcept {
        return commands_;
    }
    [[nodiscard]] const std::vector<std::pair<ModuleId, ApiOperationDescriptor>>&
    queries() const noexcept {
        return queries_;
    }
    [[nodiscard]] const std::vector<std::pair<ModuleId, ApiSystemDescriptor>>&
    systems() const noexcept {
        return systems_;
    }
    [[nodiscard]] const std::vector<std::pair<ModuleId, CapabilityId>>&
    resourceReads() const noexcept {
        return resource_reads_;
    }
    [[nodiscard]] const std::vector<std::pair<ModuleId, CapabilityId>>&
    resourceWrites() const noexcept {
        return resource_writes_;
    }
    [[nodiscard]] const std::vector<ApiId>& systemOrder() const noexcept {
        return system_order_;
    }

private:
    friend class ModuleHost;
    std::vector<std::pair<ModuleId, ApiOperationDescriptor>> commands_;
    std::vector<std::pair<ModuleId, ApiOperationDescriptor>> queries_;
    std::vector<std::pair<ModuleId, ApiSystemDescriptor>> systems_;
    std::vector<std::pair<ModuleId, CapabilityId>> resource_reads_;
    std::vector<std::pair<ModuleId, CapabilityId>> resource_writes_;
    std::vector<ApiId> system_order_;
};

class ModuleContext final {
public:
    [[nodiscard]] ModuleId module() const noexcept { return module_; }
    [[nodiscard]] ModuleRegistry& registry() noexcept { return registry_; }
    [[nodiscard]] bool hasCapability(CapabilityId id) const noexcept {
        return std::find(capabilities_.begin(), capabilities_.end(), id) != capabilities_.end();
    }

private:
    friend class ModuleHost;
    ModuleContext(ModuleId module, ModuleRegistry& registry,
                  const std::vector<CapabilityId>& capabilities) noexcept
        : module_(module), registry_(registry), capabilities_(capabilities) {}

    ModuleId module_;
    ModuleRegistry& registry_;
    const std::vector<CapabilityId>& capabilities_;
};

class ModuleHost final {
public:
    using Registration = std::function<foundation::Result<void, foundation::Error>(
        ModuleRegistry&, ModuleContext&)>;

    [[nodiscard]] foundation::Result<void, foundation::Error> registerModule(
        ModuleDescriptor descriptor, Registration registration);
    [[nodiscard]] foundation::Result<void, foundation::Error> finalize();

    [[nodiscard]] bool frozen() const noexcept { return frozen_; }
    [[nodiscard]] const std::vector<ModuleId>& loadOrder() const noexcept { return load_order_; }
    [[nodiscard]] const ModuleRegistry& registry() const noexcept { return registry_; }
    [[nodiscard]] ModuleRegistry& registry() noexcept { return registry_; }

private:
    struct Entry final {
        ModuleDescriptor descriptor;
        Registration registration;
    };

    std::vector<Entry> entries_;
    std::vector<ModuleId> load_order_;
    ModuleRegistry registry_;
    bool frozen_{false};
};

[[nodiscard]] inline foundation::Result<void, foundation::Error> registerDomainModule(
    ModuleHost& host, ModuleId id, std::vector<ModuleId> required_modules,
    std::vector<CapabilityId> provided_capabilities, ModuleHost::Registration registration) {
    return host.registerModule(
        {id, {}, std::move(required_modules),
         {foundation::stable_id("core.scheduler")}, std::move(provided_capabilities)},
        std::move(registration));
}

class CommandSink {
public:
    virtual ~CommandSink() = default;
    [[nodiscard]] virtual CommandReceipt submit(CommandEnvelope command) = 0;
};

// Language-neutral command adapter. A console, JS-like shell or Lua bridge
// can translate its surface syntax into the same envelope without exposing
// ECS pointers or scheduler objects to the language runtime.
class TextCommandAdapter final {
public:
    explicit TextCommandAdapter(const ModuleRegistry& registry) noexcept
        : registry_(registry) {}

    [[nodiscard]] foundation::Result<CommandEnvelope, foundation::Error> parse(
        std::string_view expression, foundation::SimulationTick target_tick,
        StableId source, std::uint8_t priority = 0U) const;

private:
    const ModuleRegistry& registry_;
};

class QueryApi {
public:
    virtual ~QueryApi() = default;
    [[nodiscard]] virtual SnapshotView snapshotView() const noexcept = 0;
    [[nodiscard]] virtual SnapshotView query(ApiId query, const EncodedValue& arguments) const = 0;
};

class SimulationFacade : public CommandSink, public QueryApi {
public:
    ~SimulationFacade() override = default;
    [[nodiscard]] virtual bool advance(const simulation::TickContext& tick) noexcept = 0;
};

class PresentationFacade {
public:
    virtual ~PresentationFacade() = default;
    virtual void requestSnapshot(SnapshotView snapshot) = 0;
};

class CoreControlApi {
public:
    virtual ~CoreControlApi() = default;
    virtual bool startScene(foundation::SceneId scene) = 0;
    virtual void requestQuit() noexcept = 0;
};

class GenerationService {
public:
    virtual ~GenerationService() = default;
    [[nodiscard]] virtual jobs::CancelToken cancellation() const noexcept = 0;
    [[nodiscard]] virtual std::uint64_t sceneEpoch() const noexcept = 0;
};

struct EngineServices final {
    jobs::JobSystem* scheduler{nullptr};
    proc::ProceduralRuntime* procedural_runtime{nullptr};
    GenerationService* generation{nullptr};
    SimulationFacade* simulation{nullptr};
    PresentationFacade* presentation{nullptr};
    CoreControlApi* core{nullptr};
    ModuleHost* modules{nullptr};
    EngineTelemetry* telemetry{nullptr};
    jobs::CancelToken cancellation{};
    std::uint64_t scene_epoch{0};
};

} // namespace genomes::api
