#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

namespace genomes::foundation {

// Memory is reported by ownership category rather than by allocator call.  A
// category represents the owner that is expected to release the bytes, which
// keeps this API useful for CPU containers as well as opaque GPU resources.
enum class MemoryCategory : std::uint8_t {
    PersistentSemantic = 0,
    EcsRuntime,
    ArtifactCache,
    GenerationScratch,
    RenderCpu,
    GpuBuffers,
    GpuTextures,
    Physics,
    Navigation,
    ReferenceDebug,
    Unknown,
};

inline constexpr std::size_t MemoryCategoryCount =
    static_cast<std::size_t>(MemoryCategory::Unknown) + 1U;

// A resource can be CPU resident or VRAM resident.  VRAM values are never
// inferred from process heap values: callers must opt into this location.
enum class MemoryLocation : std::uint8_t {
    Automatic = 0,
    Cpu,
    Vram,
    VRAM = Vram,
};

enum class MemoryLifetime : std::uint8_t {
    Automatic = 0,
    Persistent,
    Transient,
    Cache,
};

enum class MemorySource : std::uint8_t {
    Automatic = 0,
    NotApplicable,
    Estimated,
    DriverReported,
};

[[nodiscard]] constexpr std::size_t memoryCategoryIndex(
    MemoryCategory category) noexcept {
    return static_cast<std::size_t>(category);
}

[[nodiscard]] constexpr bool isMemoryCategory(MemoryCategory category) noexcept {
    return memoryCategoryIndex(category) < MemoryCategoryCount;
}

[[nodiscard]] std::string_view toString(MemoryCategory category) noexcept;
[[nodiscard]] std::string_view toString(MemoryLocation location) noexcept;
[[nodiscard]] std::string_view toString(MemoryLifetime lifetime) noexcept;
[[nodiscard]] std::string_view toString(MemorySource source) noexcept;

struct MemoryCounter final {
    std::uint64_t currentBytes{0};
    std::uint64_t highWaterBytes{0};
    std::uint64_t allocationCount{0};
    std::uint64_t releaseCount{0};
    std::uint64_t liveAllocations{0};

    [[nodiscard]] bool empty() const noexcept {
        return currentBytes == 0U && liveAllocations == 0U;
    }
};

// All limits are inclusive. A zero limit means that the particular
// declaration is not configured (and is therefore unbounded).
struct MemoryBudget final {
    std::uint64_t steadyStateBytes{0};
    std::uint64_t warningHighWaterBytes{0};
    std::uint64_t hardLimitBytes{0};

    [[nodiscard]] constexpr bool configured() const noexcept {
        return steadyStateBytes != 0U || warningHighWaterBytes != 0U ||
               hardLimitBytes != 0U;
    }

    [[nodiscard]] static constexpr MemoryBudget limits(
        std::uint64_t steadyState,
        std::uint64_t warningHighWater,
        std::uint64_t hardLimit) noexcept {
        return {steadyState, warningHighWater, hardLimit};
    }
};

struct MemoryBudgetProfile final {
    std::string profileId{"unbounded"};
    std::uint32_t version{1U};

    std::array<MemoryBudget, MemoryCategoryCount> categoryBudgets{};
    MemoryBudget cpu{};
    MemoryBudget vram{};
    MemoryBudget transient{};
    MemoryBudget cache{};

    [[nodiscard]] static MemoryBudgetProfile defaults();

    [[nodiscard]] const MemoryBudget& budget(MemoryCategory category) const
        noexcept;
    void setBudget(MemoryCategory category, MemoryBudget budget) noexcept;
};

struct MemoryAllocationInfo final {
    MemoryCategory category{MemoryCategory::Unknown};
    std::uint64_t bytes{0U};
    MemoryLocation location{MemoryLocation::Automatic};
    MemoryLifetime lifetime{MemoryLifetime::Automatic};
    MemorySource source{MemorySource::Automatic};
};

struct MemorySnapshot final {
    std::uint64_t sequence{0U};
    std::array<MemoryCounter, MemoryCategoryCount> categories{};
    MemoryCounter cpu{};
    MemoryCounter vram{};
    MemoryCounter vramEstimated{};
    MemoryCounter vramDriverReported{};
    MemoryCounter transient{};
    MemoryCounter cache{};
    std::uint64_t underflowCount{0U};

    [[nodiscard]] const MemoryCounter& category(
        MemoryCategory value) const noexcept;

    [[nodiscard]] std::uint64_t totalCurrentBytes() const noexcept {
        return cpu.currentBytes + vram.currentBytes;
    }

    [[nodiscard]] std::uint64_t totalHighWaterBytes() const noexcept {
        return cpu.highWaterBytes + vram.highWaterBytes;
    }
};

enum class MemoryBudgetTarget : std::uint8_t {
    Category = 0,
    Cpu,
    Vram,
    Transient,
    Cache,
};

struct MemoryBudgetViolation final {
    MemoryBudgetTarget target{MemoryBudgetTarget::Category};
    MemoryCategory category{MemoryCategory::Unknown};
    MemoryCounter observed{};
    MemoryBudget budget{};
    bool warningExceeded{false};
    bool hardLimitExceeded{false};
};

inline constexpr std::size_t MemoryBudgetTargetCount = MemoryCategoryCount + 4U;

struct MemoryBudgetReport final {
    bool warningExceeded{false};
    bool hardLimitExceeded{false};
    std::size_t violationCount{0U};
    std::array<MemoryBudgetViolation, MemoryBudgetTargetCount> violations{};

    [[nodiscard]] bool passed() const noexcept {
        return !hardLimitExceeded;
    }

    [[nodiscard]] bool withinHardLimits() const noexcept {
        return passed();
    }
};

struct MemoryRegressionPolicy final {
    std::uint64_t transientToleranceBytes{0U};
    std::uint64_t allowedPersistentGrowthBytes{0U};
    std::uint64_t allowedCacheGrowthBytes{0U};
};

struct MemoryRegressionReport final {
    bool passed{true};
    bool transientReclaimed{true};
    bool persistentWithinAllowance{true};
    bool cacheWithinAllowance{true};
    std::uint64_t transientDeltaBytes{0U};
    std::uint64_t persistentDeltaBytes{0U};
    std::uint64_t cacheDeltaBytes{0U};

    [[nodiscard]] bool leakDetected() const noexcept {
        return !transientReclaimed || !persistentWithinAllowance ||
               !cacheWithinAllowance;
    }
};

class MemoryTelemetry;

// A lease is useful at ownership boundaries where a matching release would
// otherwise be easy to miss. It is deliberately move-only and releases at
// most once, making it suitable for temporary generation/scratch buffers.
class MemoryLease final {
public:
    MemoryLease() noexcept = default;
    ~MemoryLease();

    MemoryLease(const MemoryLease&) = delete;
    MemoryLease& operator=(const MemoryLease&) = delete;

    MemoryLease(MemoryLease&& other) noexcept;
    MemoryLease& operator=(MemoryLease&& other) noexcept;

    [[nodiscard]] bool active() const noexcept { return owner_ != nullptr; }
    [[nodiscard]] std::uint64_t bytes() const noexcept { return info_.bytes; }
    void reset() noexcept;

private:
    friend class MemoryTelemetry;
    MemoryLease(MemoryTelemetry* owner, MemoryAllocationInfo info) noexcept
        : owner_(owner), info_(info) {}

    MemoryTelemetry* owner_{nullptr};
    MemoryAllocationInfo info_{};
};

class MemoryTelemetry final {
public:
    explicit MemoryTelemetry(MemoryBudgetProfile profile =
                                 MemoryBudgetProfile::defaults());
    ~MemoryTelemetry() = default;

    MemoryTelemetry(const MemoryTelemetry&) = delete;
    MemoryTelemetry& operator=(const MemoryTelemetry&) = delete;

    // acquire/release only account for an ownership transition; they do not
    // enforce a budget. Use checkBudgets() at a test or diagnostic boundary so
    // production code can continue to collect a useful over-budget snapshot.
    [[nodiscard]] bool acquire(MemoryAllocationInfo allocation) noexcept;
    [[nodiscard]] bool acquire(MemoryCategory category,
                               std::uint64_t bytes) noexcept;
    [[nodiscard]] bool acquire(MemoryCategory category,
                               std::uint64_t bytes,
                               MemoryLocation location,
                               MemoryLifetime lifetime =
                                   MemoryLifetime::Automatic,
                               MemorySource source =
                                   MemorySource::Automatic) noexcept;

    [[nodiscard]] bool release(MemoryAllocationInfo allocation) noexcept;
    [[nodiscard]] bool release(MemoryCategory category,
                               std::uint64_t bytes) noexcept;
    [[nodiscard]] bool release(MemoryCategory category,
                               std::uint64_t bytes,
                               MemoryLocation location,
                               MemoryLifetime lifetime =
                                   MemoryLifetime::Automatic,
                               MemorySource source =
                                   MemorySource::Automatic) noexcept;

    [[nodiscard]] bool recordAllocation(MemoryAllocationInfo allocation) noexcept {
        return acquire(allocation);
    }

    [[nodiscard]] bool recordRelease(MemoryAllocationInfo allocation) noexcept {
        return release(allocation);
    }

    [[nodiscard]] MemoryLease lease(MemoryAllocationInfo allocation) noexcept;
    [[nodiscard]] MemoryLease lease(MemoryCategory category,
                                    std::uint64_t bytes) noexcept;

    [[nodiscard]] MemorySnapshot snapshot() const noexcept;

    [[nodiscard]] MemoryBudgetProfile profile() const;
    void setProfile(MemoryBudgetProfile profile);

    [[nodiscard]] MemoryBudgetReport checkBudgets() const;
    [[nodiscard]] MemoryBudgetReport checkBudgets(
        const MemorySnapshot& snapshot) const;

    [[nodiscard]] MemoryRegressionReport checkRegression(
        const MemorySnapshot& baseline,
        const MemorySnapshot& final,
        MemoryRegressionPolicy policy = {}) const noexcept;

    [[nodiscard]] MemoryRegressionReport checkForLeaks(
        const MemorySnapshot& baseline,
        const MemorySnapshot& final,
        MemoryRegressionPolicy policy = {}) const noexcept {
        return checkRegression(baseline, final, policy);
    }

    // Starts a new high-water measurement window without disturbing current
    // ownership counts. This is useful for repeated START NEW/stream loops.
    void resetHighWater() noexcept;

private:
    struct CounterStorage final {
        std::atomic<std::uint64_t> currentBytes{0U};
        std::atomic<std::uint64_t> highWaterBytes{0U};
        std::atomic<std::uint64_t> allocationCount{0U};
        std::atomic<std::uint64_t> releaseCount{0U};
        std::atomic<std::uint64_t> liveAllocations{0U};
    };

    friend class MemoryLease;

    [[nodiscard]] static MemoryAllocationInfo resolve(
        MemoryAllocationInfo allocation) noexcept;
    [[nodiscard]] static const MemoryBudget& budgetFor(
        const MemoryBudgetProfile& profile,
        MemoryBudgetTarget target,
        MemoryCategory category) noexcept;
    [[nodiscard]] static const MemoryCounter& counterFor(
        const MemorySnapshot& snapshot,
        MemoryBudgetTarget target,
        MemoryCategory category) noexcept;

    static void increment(CounterStorage& counter,
                          std::uint64_t bytes) noexcept;
    static bool decrement(CounterStorage& counter,
                          std::uint64_t bytes) noexcept;
    static MemoryCounter read(const CounterStorage& counter) noexcept;
    static void resetHighWater(CounterStorage& counter) noexcept;

    std::array<CounterStorage, MemoryCategoryCount> categories_{};
    CounterStorage cpu_{};
    CounterStorage vram_{};
    CounterStorage vramEstimated_{};
    CounterStorage vramDriverReported_{};
    CounterStorage transient_{};
    CounterStorage cache_{};
    std::atomic<std::uint64_t> sequence_{0U};
    std::atomic<std::uint64_t> underflowCount_{0U};

    mutable std::mutex profileMutex_;
    MemoryBudgetProfile profile_{};
};

} // namespace genomes::foundation
