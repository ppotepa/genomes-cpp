#pragma once

#include <genomes/ballistics/ProjectileState.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace genomes::ballistics {

struct ProjectileHandle final {
    std::uint32_t slot{0};
    std::uint32_t generation{0};

    [[nodiscard]] bool valid() const noexcept { return generation != 0U; }
    friend constexpr bool operator==(ProjectileHandle, ProjectileHandle) noexcept = default;
};

struct ProjectileStorageConfig final {
    std::uint32_t capacity{2048};
    std::uint32_t chunk_size{64};

    [[nodiscard]] bool valid() const noexcept {
        return capacity > 0U && chunk_size > 0U;
    }
};

struct ProjectileChunk final {
    std::size_t batch_index{0};
    std::span<const ProjectileHandle> handles{};
};

class ProjectileStorage final {
public:
    explicit ProjectileStorage(ProjectileStorageConfig config = {});

    [[nodiscard]] foundation::Result<ProjectileHandle, foundation::Error> insert(
        ProjectileState state);
    [[nodiscard]] bool erase(ProjectileHandle handle) noexcept;
    [[nodiscard]] ProjectileState* get(ProjectileHandle handle) noexcept;
    [[nodiscard]] const ProjectileState* get(ProjectileHandle handle) const noexcept;
    [[nodiscard]] ProjectileHandle find(ProjectileId id) const noexcept;

    // Rebuilds dense iteration order by semantic projectile ID.  Handles do
    // not change; only the physical iteration order is compacted.
    void stableCompact() noexcept;

    [[nodiscard]] std::vector<ProjectileHandle> stableHandles() const;
    [[nodiscard]] std::vector<ProjectileState> snapshot() const;
    [[nodiscard]] std::vector<ProjectileChunk> activeChunks() const;

    [[nodiscard]] std::size_t activeCount() const noexcept { return dense_handles_.size(); }
    [[nodiscard]] std::uint32_t capacity() const noexcept { return config_.capacity; }
    [[nodiscard]] std::uint32_t chunkSize() const noexcept { return config_.chunk_size; }

private:
    struct Slot final {
        ProjectileState state{};
        std::uint32_t generation{1};
        std::uint32_t dense_index{0};
        bool occupied{false};
    };

    [[nodiscard]] bool owns(ProjectileHandle handle) const noexcept;
    [[nodiscard]] std::uint32_t allocateSlot() noexcept;
    void releaseSlot(std::uint32_t slot) noexcept;

    ProjectileStorageConfig config_{};
    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_slots_;
    std::vector<ProjectileHandle> dense_handles_;
    std::unordered_map<foundation::StableId, ProjectileHandle> ids_;
};

} // namespace genomes::ballistics
