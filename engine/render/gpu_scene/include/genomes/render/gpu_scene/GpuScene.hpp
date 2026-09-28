#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/render/RenderExtraction.hpp>
#include <genomes/render/RenderTypes.hpp>
#include <genomes/render/gpu_scene/GpuSceneRecords.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace genomes::render::gpu_scene {

class GpuScene final {
public:
    [[nodiscard]] foundation::Result<GpuUploadBatch, foundation::Error> sync(
        std::span<const RenderInstance> instances);

    [[nodiscard]] foundation::Result<GpuUploadBatch, foundation::Error> apply(
        std::span<const RenderChange> changes);

    // Rebuilds a complete slot-table upload, used only when a backend grows
    // its persistent GPU buffer. Normal frames should consume sync/apply
    // deltas instead.
    [[nodiscard]] GpuUploadBatch fullUpload() const;

    // Releases a validated batch of handles, e.g. all instances owned by an
    // unloaded world region. The returned batch contains cleared slots and
    // the coalesced dirty ranges needed by the backend.
    [[nodiscard]] foundation::Result<GpuUploadBatch, foundation::Error> release(
        std::span<const GpuInstanceHandle> handles);

    void clear() noexcept;

    [[nodiscard]] GpuInstanceHandle find(foundation::StableId semantic_id) const noexcept;
    [[nodiscard]] const GpuInstanceRecord* record(GpuInstanceHandle) const noexcept;
    [[nodiscard]] std::size_t liveCount() const noexcept { return live_count_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return slots_.size(); }
    [[nodiscard]] std::size_t slotCount() const noexcept { return slots_.size(); }
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] std::size_t memoryBytes() const noexcept;

private:
    struct Slot final {
        GpuInstanceRecord record{};
        std::uint32_t generation{1};
        bool alive{false};
    };

    [[nodiscard]] foundation::Result<GpuInstanceHandle, foundation::Error> allocate(
        foundation::StableId semantic_id);
    void growStorage();
    void removeSlot(GpuInstanceHandle handle, GpuUploadBatch& batch) noexcept;
    void appendDirtyUploads(GpuUploadBatch& batch);
    void markDirty(std::uint32_t index) noexcept;
    [[nodiscard]] bool valid(GpuInstanceHandle) const noexcept;
    [[nodiscard]] foundation::Error invalid(const char* message) const noexcept;

    std::unordered_map<foundation::StableId, GpuInstanceHandle> semantic_to_handle_;
    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_indices_;
    std::vector<std::uint8_t> dirty_;
    std::vector<std::uint8_t> present_;
    std::size_t live_count_{0};
    std::uint64_t revision_{0};
};

} // namespace genomes::render::gpu_scene
