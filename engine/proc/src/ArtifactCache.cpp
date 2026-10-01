#include <genomes/proc/ArtifactCache.hpp>

#include <algorithm>
#include <utility>

namespace genomes::proc {

std::size_t ArtifactKeyHash::operator()(const ArtifactKey& key) const noexcept {
    std::uint64_t hash = foundation::stableHashCombine(key.namespace_id,
                                                       key.generator_version);
    hash = foundation::stableHashCombine(hash, key.seed);
    hash = foundation::stableHashCombine(hash, key.input_hash);
    hash = foundation::stableHashCombine(hash, key.schema_version);
    hash = foundation::stableHashCombine(hash, key.seed_derivation_version);
    hash = foundation::stableHashCombine(hash, key.dependency_hash);
    return static_cast<std::size_t>(hash ^ (hash >> 32U));
}

ArtifactCache::ErasedValue ArtifactCache::findRaw(const ArtifactKey& key,
                                                   std::type_index type) const {
    std::lock_guard lock(mutex_);
    const auto iterator = entries_.find(key);
    if (iterator == entries_.end() || iterator->second.type != type) {
        ++misses_;
        return {};
    }
    ++hits_;
    ++clock_;
    iterator->second.last_use = clock_;
    return iterator->second.value;
}

void ArtifactCache::storeRaw(const ArtifactKey& key,
                             std::type_index type,
                             ErasedValue value,
                             std::size_t bytes) {
    std::lock_guard lock(mutex_);
    const auto existing = entries_.find(key);
    if (existing != entries_.end()) {
        trackExternalPinLocked(existing->second.value, existing->second.bytes);
        if (telemetry_ != nullptr) {
            (void)telemetry_->release(foundation::MemoryCategory::ArtifactCache,
                                      existing->second.bytes);
        }
        retained_bytes_ -= existing->second.bytes;
        entries_.erase(existing);
    }
    if (bytes > byte_budget_) {
        return;
    }
    ++clock_;
    entries_.emplace(key, Entry{type, std::move(value), bytes, clock_});
    retained_bytes_ += bytes;
    if (telemetry_ != nullptr) {
        (void)telemetry_->acquire(foundation::MemoryCategory::ArtifactCache,
                                  bytes);
    }
    evictLocked();
}

void ArtifactCache::setByteBudget(std::size_t byte_budget) {
    std::lock_guard lock(mutex_);
    byte_budget_ = byte_budget;
    evictLocked();
}

void ArtifactCache::evictLocked() {
    collectExpiredPinsLocked();
    while (retained_bytes_ > byte_budget_) {
        auto candidate = entries_.end();
        for (auto iterator = entries_.begin(); iterator != entries_.end(); ++iterator) {
            if (candidate == entries_.end() ||
                iterator->second.last_use < candidate->second.last_use ||
                (iterator->second.last_use == candidate->second.last_use &&
                 ArtifactKeyHash{}(iterator->first) < ArtifactKeyHash{}(candidate->first))) {
                candidate = iterator;
            }
        }
        if (candidate == entries_.end()) break;
        trackExternalPinLocked(candidate->second.value, candidate->second.bytes);
        retained_bytes_ -= candidate->second.bytes;
        if (telemetry_ != nullptr) {
            (void)telemetry_->release(foundation::MemoryCategory::ArtifactCache,
                                      candidate->second.bytes);
        }
        entries_.erase(candidate);
        ++evictions_;
    }
}

void ArtifactCache::trackExternalPinLocked(const ErasedValue& value, std::size_t bytes) {
    if (value && value.use_count() > 1U) {
        evicted_pins_.push_back({value, bytes});
    }
}

void ArtifactCache::collectExpiredPinsLocked() const noexcept {
    std::erase_if(evicted_pins_, [](const EvictedPin& pin) { return pin.value.expired(); });
}

void ArtifactCache::erase(const ArtifactKey& key) {
    std::lock_guard lock(mutex_);
    const auto iterator = entries_.find(key);
    if (iterator == entries_.end()) {
        return;
    }
    trackExternalPinLocked(iterator->second.value, iterator->second.bytes);
    retained_bytes_ -= iterator->second.bytes;
    if (telemetry_ != nullptr) {
        (void)telemetry_->release(foundation::MemoryCategory::ArtifactCache,
                                  iterator->second.bytes);
    }
    entries_.erase(iterator);
}

void ArtifactCache::clear() {
    std::lock_guard lock(mutex_);
    for (const auto& [key, entry] : entries_) {
        (void)key;
        trackExternalPinLocked(entry.value, entry.bytes);
    }
    if (telemetry_ != nullptr) {
        for (const auto& [key, entry] : entries_) {
            (void)key;
            (void)telemetry_->release(foundation::MemoryCategory::ArtifactCache,
                                      entry.bytes);
        }
    }
    entries_.clear();
    retained_bytes_ = 0;
}

ArtifactCacheStats ArtifactCache::stats() const noexcept {
    std::lock_guard lock(mutex_);
    collectExpiredPinsLocked();
    std::size_t shared_bytes = 0U;
    for (const auto& [key, entry] : entries_) {
        (void)key;
        if (entry.value.use_count() > 1U) shared_bytes += entry.bytes;
    }
    std::size_t externally_pinned_bytes = 0U;
    for (const auto& pin : evicted_pins_) externally_pinned_bytes += pin.bytes;
    return {hits_, misses_, evictions_, entries_.size(), retained_bytes_, shared_bytes,
            externally_pinned_bytes};
}

} // namespace genomes::proc
