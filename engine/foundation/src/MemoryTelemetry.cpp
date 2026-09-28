#include <genomes/foundation/MemoryTelemetry.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace genomes::foundation {

namespace {

constexpr MemoryLocation categoryLocation(MemoryCategory category) noexcept {
    switch (category) {
    case MemoryCategory::GpuBuffers:
    case MemoryCategory::GpuTextures:
        return MemoryLocation::Vram;
    default:
        return MemoryLocation::Cpu;
    }
}

constexpr MemoryLifetime categoryLifetime(MemoryCategory category) noexcept {
    switch (category) {
    case MemoryCategory::ArtifactCache:
    case MemoryCategory::ReferenceDebug:
        return MemoryLifetime::Cache;
    case MemoryCategory::GenerationScratch:
        return MemoryLifetime::Transient;
    default:
        return MemoryLifetime::Persistent;
    }
}

constexpr MemoryBudgetTarget aggregateTarget(std::size_t index) noexcept {
    switch (index) {
    case MemoryCategoryCount:
        return MemoryBudgetTarget::Cpu;
    case MemoryCategoryCount + 1U:
        return MemoryBudgetTarget::Vram;
    case MemoryCategoryCount + 2U:
        return MemoryBudgetTarget::Transient;
    default:
        return MemoryBudgetTarget::Cache;
    }
}

constexpr std::uint64_t saturatingDelta(std::uint64_t before,
                                        std::uint64_t after) noexcept {
    return after > before ? after - before : 0U;
}

bool exceeds(const MemoryCounter& observed, const MemoryBudget& budget,
             bool highWater) noexcept {
    const std::uint64_t value =
        highWater ? observed.highWaterBytes : observed.currentBytes;
    return budget.hardLimitBytes != 0U && value > budget.hardLimitBytes;
}

bool warns(const MemoryCounter& observed, const MemoryBudget& budget) noexcept {
    return budget.warningHighWaterBytes != 0U &&
           observed.highWaterBytes > budget.warningHighWaterBytes;
}

} // namespace

std::string_view toString(MemoryCategory category) noexcept {
    switch (category) {
    case MemoryCategory::PersistentSemantic:
        return "persistent_semantic";
    case MemoryCategory::EcsRuntime:
        return "ecs_runtime";
    case MemoryCategory::ArtifactCache:
        return "artifact_cache";
    case MemoryCategory::GenerationScratch:
        return "generation_scratch";
    case MemoryCategory::RenderCpu:
        return "render_cpu";
    case MemoryCategory::GpuBuffers:
        return "gpu_buffers";
    case MemoryCategory::GpuTextures:
        return "gpu_textures";
    case MemoryCategory::Physics:
        return "physics";
    case MemoryCategory::Navigation:
        return "navigation";
    case MemoryCategory::ReferenceDebug:
        return "reference_debug";
    case MemoryCategory::Unknown:
        return "unknown";
    }
    return "unknown";
}

std::string_view toString(MemoryLocation location) noexcept {
    switch (location) {
    case MemoryLocation::Automatic:
        return "automatic";
    case MemoryLocation::Cpu:
        return "cpu";
    case MemoryLocation::Vram:
        return "vram";
    }
    return "automatic";
}

std::string_view toString(MemoryLifetime lifetime) noexcept {
    switch (lifetime) {
    case MemoryLifetime::Automatic:
        return "automatic";
    case MemoryLifetime::Persistent:
        return "persistent";
    case MemoryLifetime::Transient:
        return "transient";
    case MemoryLifetime::Cache:
        return "cache";
    }
    return "automatic";
}

std::string_view toString(MemorySource source) noexcept {
    switch (source) {
    case MemorySource::Automatic:
        return "automatic";
    case MemorySource::NotApplicable:
        return "not_applicable";
    case MemorySource::Estimated:
        return "estimated";
    case MemorySource::DriverReported:
        return "driver_reported";
    }
    return "automatic";
}

MemoryBudgetProfile MemoryBudgetProfile::defaults() {
    // The library default deliberately does not invent a hardware tier. A
    // target or test should load a tier-specific profile before enforcing
    // limits; telemetry remains useful in the unbounded/default profile.
    return {};
}

const MemoryBudget& MemoryBudgetProfile::budget(MemoryCategory category) const
    noexcept {
    if (!isMemoryCategory(category)) {
        return categoryBudgets[memoryCategoryIndex(MemoryCategory::Unknown)];
    }
    return categoryBudgets[memoryCategoryIndex(category)];
}

void MemoryBudgetProfile::setBudget(MemoryCategory category,
                                    MemoryBudget value) noexcept {
    if (isMemoryCategory(category)) {
        categoryBudgets[memoryCategoryIndex(category)] = value;
    }
}

const MemoryCounter& MemorySnapshot::category(MemoryCategory value) const
    noexcept {
    if (!isMemoryCategory(value)) {
        return categories[memoryCategoryIndex(MemoryCategory::Unknown)];
    }
    return categories[memoryCategoryIndex(value)];
}

MemoryLease::~MemoryLease() {
    reset();
}

MemoryLease::MemoryLease(MemoryLease&& other) noexcept
    : owner_(other.owner_), info_(other.info_) {
    other.owner_ = nullptr;
}

MemoryLease& MemoryLease::operator=(MemoryLease&& other) noexcept {
    if (this != &other) {
        reset();
        owner_ = other.owner_;
        info_ = other.info_;
        other.owner_ = nullptr;
    }
    return *this;
}

void MemoryLease::reset() noexcept {
    if (owner_ != nullptr) {
        (void)owner_->release(info_);
        owner_ = nullptr;
    }
}

MemoryTelemetry::MemoryTelemetry(MemoryBudgetProfile profile)
    : profile_(std::move(profile)) {}

MemoryAllocationInfo MemoryTelemetry::resolve(
    MemoryAllocationInfo allocation) noexcept {
    if (allocation.location == MemoryLocation::Automatic) {
        allocation.location = categoryLocation(allocation.category);
    }
    if (allocation.lifetime == MemoryLifetime::Automatic) {
        allocation.lifetime = categoryLifetime(allocation.category);
    }
    if (allocation.location == MemoryLocation::Cpu) {
        allocation.source = MemorySource::NotApplicable;
    } else if (allocation.source == MemorySource::Automatic) {
        // A GPU resource without a driver query is an estimate. It is never
        // silently counted as CPU memory or presented as an exact value.
        allocation.source = MemorySource::Estimated;
    }
    return allocation;
}

bool MemoryTelemetry::acquire(MemoryAllocationInfo allocation) noexcept {
    if (!isMemoryCategory(allocation.category)) {
        return false;
    }
    if (allocation.bytes == 0U) {
        return true;
    }

    allocation = resolve(allocation);
    CounterStorage& category = categories_[memoryCategoryIndex(allocation.category)];
    increment(category, allocation.bytes);

    CounterStorage* locationCounter =
        allocation.location == MemoryLocation::Vram ? &vram_ : &cpu_;
    increment(*locationCounter, allocation.bytes);

    if (allocation.location == MemoryLocation::Vram) {
        if (allocation.source == MemorySource::DriverReported) {
            increment(vramDriverReported_, allocation.bytes);
        } else {
            increment(vramEstimated_, allocation.bytes);
        }
    }
    if (allocation.lifetime == MemoryLifetime::Transient) {
        increment(transient_, allocation.bytes);
    } else if (allocation.lifetime == MemoryLifetime::Cache) {
        increment(cache_, allocation.bytes);
    }
    sequence_.fetch_add(1U, std::memory_order_relaxed);
    return true;
}

bool MemoryTelemetry::acquire(MemoryCategory category,
                              std::uint64_t bytes) noexcept {
    return acquire({category, bytes});
}

bool MemoryTelemetry::acquire(MemoryCategory category,
                              std::uint64_t bytes,
                              MemoryLocation location,
                              MemoryLifetime lifetime,
                              MemorySource source) noexcept {
    return acquire({category, bytes, location, lifetime, source});
}

bool MemoryTelemetry::release(MemoryAllocationInfo allocation) noexcept {
    if (!isMemoryCategory(allocation.category)) {
        underflowCount_.fetch_add(1U, std::memory_order_relaxed);
        return false;
    }
    if (allocation.bytes == 0U) {
        return true;
    }

    allocation = resolve(allocation);
    CounterStorage& category = categories_[memoryCategoryIndex(allocation.category)];
    if (!decrement(category, allocation.bytes)) {
        underflowCount_.fetch_add(1U, std::memory_order_relaxed);
        return false;
    }

    CounterStorage* locationCounter =
        allocation.location == MemoryLocation::Vram ? &vram_ : &cpu_;
    const bool locationReleased = decrement(*locationCounter, allocation.bytes);
    bool sourceReleased = true;
    if (allocation.location == MemoryLocation::Vram) {
        CounterStorage& sourceCounter =
            allocation.source == MemorySource::DriverReported
                ? vramDriverReported_
                : vramEstimated_;
        sourceReleased = decrement(sourceCounter, allocation.bytes);
    }
    bool lifetimeReleased = true;
    if (allocation.lifetime == MemoryLifetime::Transient) {
        lifetimeReleased = decrement(transient_, allocation.bytes);
    } else if (allocation.lifetime == MemoryLifetime::Cache) {
        lifetimeReleased = decrement(cache_, allocation.bytes);
    }

    if (!locationReleased || !sourceReleased || !lifetimeReleased) {
        underflowCount_.fetch_add(1U, std::memory_order_relaxed);
        return false;
    }
    sequence_.fetch_add(1U, std::memory_order_relaxed);
    return true;
}

bool MemoryTelemetry::release(MemoryCategory category,
                              std::uint64_t bytes) noexcept {
    return release({category, bytes});
}

bool MemoryTelemetry::release(MemoryCategory category,
                              std::uint64_t bytes,
                              MemoryLocation location,
                              MemoryLifetime lifetime,
                              MemorySource source) noexcept {
    return release({category, bytes, location, lifetime, source});
}

MemoryLease MemoryTelemetry::lease(MemoryAllocationInfo allocation) noexcept {
    if (!acquire(allocation)) {
        return {};
    }
    return {this, resolve(allocation)};
}

MemoryLease MemoryTelemetry::lease(MemoryCategory category,
                                   std::uint64_t bytes) noexcept {
    return lease({category, bytes});
}

MemoryCounter MemoryTelemetry::read(const CounterStorage& counter) noexcept {
    return {counter.currentBytes.load(std::memory_order_relaxed),
            counter.highWaterBytes.load(std::memory_order_relaxed),
            counter.allocationCount.load(std::memory_order_relaxed),
            counter.releaseCount.load(std::memory_order_relaxed),
            counter.liveAllocations.load(std::memory_order_relaxed)};
}

void MemoryTelemetry::increment(CounterStorage& counter,
                                std::uint64_t bytes) noexcept {
    const std::uint64_t current =
        counter.currentBytes.fetch_add(bytes, std::memory_order_relaxed) + bytes;
    counter.allocationCount.fetch_add(1U, std::memory_order_relaxed);
    counter.liveAllocations.fetch_add(1U, std::memory_order_relaxed);

    std::uint64_t highWater = counter.highWaterBytes.load(std::memory_order_relaxed);
    while (highWater < current &&
           !counter.highWaterBytes.compare_exchange_weak(
               highWater, current, std::memory_order_relaxed,
               std::memory_order_relaxed)) {
    }
}

bool MemoryTelemetry::decrement(CounterStorage& counter,
                                std::uint64_t bytes) noexcept {
    std::uint64_t current = counter.currentBytes.load(std::memory_order_relaxed);
    while (current >= bytes) {
        if (counter.currentBytes.compare_exchange_weak(
                current, current - bytes, std::memory_order_relaxed,
                std::memory_order_relaxed)) {
            counter.releaseCount.fetch_add(1U, std::memory_order_relaxed);
            counter.liveAllocations.fetch_sub(1U, std::memory_order_relaxed);
            return true;
        }
    }
    return false;
}

MemorySnapshot MemoryTelemetry::snapshot() const noexcept {
    MemorySnapshot result{};
    result.sequence = sequence_.load(std::memory_order_relaxed);
    for (std::size_t index = 0U; index < MemoryCategoryCount; ++index) {
        result.categories[index] = read(categories_[index]);
    }
    result.cpu = read(cpu_);
    result.vram = read(vram_);
    result.vramEstimated = read(vramEstimated_);
    result.vramDriverReported = read(vramDriverReported_);
    result.transient = read(transient_);
    result.cache = read(cache_);
    result.underflowCount = underflowCount_.load(std::memory_order_relaxed);
    return result;
}

MemoryBudgetProfile MemoryTelemetry::profile() const {
    std::scoped_lock lock(profileMutex_);
    return profile_;
}

void MemoryTelemetry::setProfile(MemoryBudgetProfile profile) {
    std::scoped_lock lock(profileMutex_);
    profile_ = std::move(profile);
}

const MemoryBudget& MemoryTelemetry::budgetFor(const MemoryBudgetProfile& profile,
                                              MemoryBudgetTarget target,
                                              MemoryCategory category) noexcept {
    switch (target) {
    case MemoryBudgetTarget::Category:
        return profile.budget(category);
    case MemoryBudgetTarget::Cpu:
        return profile.cpu;
    case MemoryBudgetTarget::Vram:
        return profile.vram;
    case MemoryBudgetTarget::Transient:
        return profile.transient;
    case MemoryBudgetTarget::Cache:
        return profile.cache;
    }
    return profile.cache;
}

const MemoryCounter& MemoryTelemetry::counterFor(const MemorySnapshot& snapshot,
                                                MemoryBudgetTarget target,
                                                MemoryCategory category) noexcept {
    switch (target) {
    case MemoryBudgetTarget::Category:
        return snapshot.category(category);
    case MemoryBudgetTarget::Cpu:
        return snapshot.cpu;
    case MemoryBudgetTarget::Vram:
        return snapshot.vram;
    case MemoryBudgetTarget::Transient:
        return snapshot.transient;
    case MemoryBudgetTarget::Cache:
        return snapshot.cache;
    }
    return snapshot.cache;
}

MemoryBudgetReport MemoryTelemetry::checkBudgets() const {
    return checkBudgets(snapshot());
}

MemoryBudgetReport MemoryTelemetry::checkBudgets(
    const MemorySnapshot& snapshotValue) const {
    const MemoryBudgetProfile currentProfile = profile();
    MemoryBudgetReport result{};
    const std::size_t targetCount = MemoryCategoryCount + 4U;
    for (std::size_t index = 0U; index < targetCount; ++index) {
        const MemoryBudgetTarget target =
            index < MemoryCategoryCount ? MemoryBudgetTarget::Category
                                         : aggregateTarget(index);
        const MemoryCategory category =
            index < MemoryCategoryCount
                ? static_cast<MemoryCategory>(index)
                : MemoryCategory::Unknown;
        const MemoryBudget& budget = budgetFor(currentProfile, target, category);
        if (!budget.configured()) {
            continue;
        }

        const MemoryCounter& observed = counterFor(snapshotValue, target, category);
        const bool warning = warns(observed, budget);
        const bool hard = exceeds(observed, budget, true) ||
                          exceeds(observed, budget, false);
        if (warning) {
            result.warningExceeded = true;
        }
        if (hard) {
            result.hardLimitExceeded = true;
        }
        if ((warning || hard) && result.violationCount < result.violations.size()) {
            result.violations[result.violationCount++] = {
                target, category, observed, budget, warning, hard};
        }
    }
    return result;
}

MemoryRegressionReport MemoryTelemetry::checkRegression(
    const MemorySnapshot& baseline,
    const MemorySnapshot& final,
    MemoryRegressionPolicy policy) const noexcept {
    MemoryRegressionReport result{};
    result.transientDeltaBytes =
        saturatingDelta(baseline.transient.currentBytes,
                        final.transient.currentBytes);
    result.cacheDeltaBytes = saturatingDelta(baseline.cache.currentBytes,
                                             final.cache.currentBytes);

    const std::uint64_t baselinePersistent =
        baseline.totalCurrentBytes() - baseline.transient.currentBytes -
        baseline.cache.currentBytes;
    const std::uint64_t finalPersistent =
        final.totalCurrentBytes() - final.transient.currentBytes -
        final.cache.currentBytes;
    result.persistentDeltaBytes =
        saturatingDelta(baselinePersistent, finalPersistent);
    result.transientReclaimed =
        final.transient.currentBytes <=
        baseline.transient.currentBytes + policy.transientToleranceBytes;
    result.persistentWithinAllowance =
        result.persistentDeltaBytes <= policy.allowedPersistentGrowthBytes;
    result.cacheWithinAllowance =
        result.cacheDeltaBytes <= policy.allowedCacheGrowthBytes;
    result.passed = result.transientReclaimed &&
                    result.persistentWithinAllowance &&
                    result.cacheWithinAllowance;
    return result;
}

void MemoryTelemetry::resetHighWater(CounterStorage& counter) noexcept {
    counter.highWaterBytes.store(counter.currentBytes.load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
}

void MemoryTelemetry::resetHighWater() noexcept {
    for (CounterStorage& counter : categories_) {
        resetHighWater(counter);
    }
    resetHighWater(cpu_);
    resetHighWater(vram_);
    resetHighWater(vramEstimated_);
    resetHighWater(vramDriverReported_);
    resetHighWater(transient_);
    resetHighWater(cache_);
}

} // namespace genomes::foundation
