#pragma once

#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/MemoryTelemetry.hpp>
#include <genomes/foundation/Types.hpp>
#include <genomes/proc/Seed.hpp>

#include <cstdint>
#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>

namespace genomes::proc {

struct ArtifactKey final {
    foundation::StableId namespace_id{0};
    std::uint32_t generator_version{0};
    Seed seed{0};
    foundation::StableId input_hash{0};
    std::uint32_t schema_version{1};
    std::uint32_t seed_derivation_version{SeedDerivationVersion};
    foundation::StableId dependency_hash{0};

    [[nodiscard]] bool operator==(const ArtifactKey&) const noexcept = default;
};

struct ArtifactKeyHash final {
    [[nodiscard]] std::size_t operator()(const ArtifactKey&) const noexcept;
};

struct ArtifactCacheStats final {
    std::uint64_t hits{0};
    std::uint64_t misses{0};
    std::uint64_t evictions{0};
    std::size_t entries{0};
    std::size_t retained_bytes{0};
};

struct ArtifactCacheOptions final {
    std::size_t byte_budget{64U * 1024U * 1024U};
    foundation::MemoryTelemetry* telemetry{nullptr};
};

// Thread-safe immutable-artifact cache. The cache stores shared const values;
// callers can read an artifact concurrently, but no cache entry can be mutated
// through the returned pointer.
class ArtifactCache final {
public:
    explicit ArtifactCache(ArtifactCacheOptions options = {}) noexcept
        : byte_budget_{options.byte_budget}, telemetry_{options.telemetry} {}

    ArtifactCache(const ArtifactCache&) = delete;
    ArtifactCache& operator=(const ArtifactCache&) = delete;

    template <class T>
    [[nodiscard]] std::shared_ptr<const T> find(const ArtifactKey& key) const {
        const auto value = findRaw(key, std::type_index(typeid(T)));
        return value ? std::static_pointer_cast<const T>(value) : nullptr;
    }

    template <class T>
    void store(const ArtifactKey& key,
               std::shared_ptr<const T> value,
               std::size_t estimated_bytes = sizeof(T)) {
        if (!value) {
            return;
        }
        storeRaw(key, std::type_index(typeid(T)), std::move(value), estimated_bytes);
    }

    void setByteBudget(std::size_t byte_budget) noexcept;
    void erase(const ArtifactKey& key) noexcept;
    void clear() noexcept;
    [[nodiscard]] ArtifactCacheStats stats() const noexcept;

private:
    using ErasedValue = std::shared_ptr<const void>;

    [[nodiscard]] ErasedValue findRaw(const ArtifactKey&, std::type_index) const;
    void storeRaw(const ArtifactKey&, std::type_index, ErasedValue, std::size_t bytes);
    void evictLocked() noexcept;

    struct Entry final {
        std::type_index type{typeid(void)};
        ErasedValue value;
        std::size_t bytes{0};
        std::uint64_t last_use{0};
    };

    mutable std::mutex mutex_;
    mutable std::unordered_map<ArtifactKey, Entry, ArtifactKeyHash> entries_;
    std::size_t byte_budget_{0};
    std::size_t retained_bytes_{0};
    foundation::MemoryTelemetry* telemetry_{nullptr};
    mutable std::uint64_t clock_{0};
    mutable std::uint64_t hits_{0};
    mutable std::uint64_t misses_{0};
    std::uint64_t evictions_{0};
};

} // namespace genomes::proc
