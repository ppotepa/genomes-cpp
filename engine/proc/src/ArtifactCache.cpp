#include <genomes/proc/ArtifactCache.hpp>

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

void ArtifactCache::setByteBudget(std::size_t byte_budget) noexcept {
    std::lock_guard lock(mutex_);
    byte_budget_ = byte_budget;
    evictLocked();
}

void ArtifactCache::evictLocked() noexcept {
    while (retained_bytes_ > byte_budget_) {
        auto candidate = entries_.end();
        for (auto iterator = entries_.begin(); iterator != entries_.end(); ++iterator) {
            // A value held by a caller is pinned until that shared pointer is
            // released; eviction never invalidates an in-use artifact.
            if (iterator->second.value.use_count() > 1) {
                continue;
            }
            if (candidate == entries_.end() ||
                iterator->second.last_use < candidate->second.last_use ||
                (iterator->second.last_use == candidate->second.last_use &&
                 ArtifactKeyHash{}(iterator->first) < ArtifactKeyHash{}(candidate->first))) {
                candidate = iterator;
            }
        }
        if (candidate == entries_.end()) {
            break;
        }
        retained_bytes_ -= candidate->second.bytes;
        if (telemetry_ != nullptr) {
            (void)telemetry_->release(foundation::MemoryCategory::ArtifactCache,
                                      candidate->second.bytes);
        }
        entries_.erase(candidate);
        ++evictions_;
    }
}

void ArtifactCache::erase(const ArtifactKey& key) noexcept {
    std::lock_guard lock(mutex_);
    const auto iterator = entries_.find(key);
    if (iterator == entries_.end()) {
        return;
    }
    retained_bytes_ -= iterator->second.bytes;
    if (telemetry_ != nullptr) {
        (void)telemetry_->release(foundation::MemoryCategory::ArtifactCache,
                                  iterator->second.bytes);
    }
    entries_.erase(iterator);
}

void ArtifactCache::clear() noexcept {
    std::lock_guard lock(mutex_);
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
    return {hits_, misses_, evictions_, entries_.size(), retained_bytes_};
}

} // namespace genomes::proc
