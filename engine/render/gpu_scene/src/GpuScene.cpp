#include <genomes/render/gpu_scene/GpuScene.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

namespace genomes::render::gpu_scene {

namespace {

[[nodiscard]] bool finite(const foundation::Vec3& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool validInstance(const RenderInstance& instance) noexcept {
    return instance.object_id != 0 && instance.mesh_id != 0 && instance.material_id != 0 &&
           finite(instance.position) && finite(instance.scale) &&
           std::isfinite(instance.rotation_y) && instance.scale.x >= 0.0F &&
           instance.scale.y >= 0.0F && instance.scale.z >= 0.0F;
}

} // namespace

foundation::Error GpuScene::invalid(const char* message) const noexcept {
    return {foundation::ErrorCode::InvalidArgument, message};
}

bool GpuScene::valid(GpuInstanceHandle handle) const noexcept {
    return handle.isValid() && handle.index < slots_.size() &&
           slots_[handle.index].alive && slots_[handle.index].generation == handle.generation;
}

GpuInstanceHandle GpuScene::find(foundation::StableId semantic_id) const noexcept {
    const auto iterator = semantic_to_handle_.find(semantic_id);
    return iterator == semantic_to_handle_.end() ? GpuInstanceHandle{} : iterator->second;
}

const GpuInstanceRecord* GpuScene::record(GpuInstanceHandle handle) const noexcept {
    return valid(handle) ? &slots_[handle.index].record : nullptr;
}

foundation::Result<GpuInstanceHandle, foundation::Error> GpuScene::allocate(
    foundation::StableId semantic_id) {
    if (semantic_id == 0 || semantic_to_handle_.contains(semantic_id)) {
        return foundation::Result<GpuInstanceHandle, foundation::Error>::failure(
            invalid("GPU scene semantic ID is invalid or already allocated"));
    }

    std::uint32_t index = foundation::Handle<GpuInstanceTag>::InvalidIndex;
    if (!free_indices_.empty()) {
        index = free_indices_.back();
        free_indices_.pop_back();
    } else {
        if (slots_.size() >= foundation::Handle<GpuInstanceTag>::InvalidIndex) {
            return foundation::Result<GpuInstanceHandle, foundation::Error>::failure(
                {foundation::ErrorCode::OutOfRange, "GPU scene handle capacity exhausted"});
        }
        index = static_cast<std::uint32_t>(slots_.size());
        growStorage();
        slots_.push_back({});
        dirty_.push_back(0);
        present_.push_back(0);
    }

    Slot& slot = slots_[index];
    slot.alive = true;
    slot.record = {};
    slot.record.semantic_id = semantic_id;
    const GpuInstanceHandle handle{index, slot.generation};
    semantic_to_handle_.emplace(semantic_id, handle);
    ++live_count_;
    markDirty(index);
    return foundation::Result<GpuInstanceHandle, foundation::Error>::success(handle);
}

void GpuScene::markDirty(std::uint32_t index) noexcept {
    if (index < dirty_.size()) {
        dirty_[index] = 1;
    }
}

void GpuScene::removeSlot(GpuInstanceHandle handle, GpuUploadBatch& batch) noexcept {
    if (!valid(handle)) {
        return;
    }
    Slot& slot = slots_[handle.index];
    semantic_to_handle_.erase(slot.record.semantic_id);
    batch.removed.push_back(handle);
    slot.alive = false;
    slot.record = {};
    ++slot.generation;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
    free_indices_.push_back(handle.index);
    markDirty(handle.index);
    --live_count_;
}

void GpuScene::growStorage() {
    if (slots_.size() < slots_.capacity()) {
        return;
    }
    constexpr std::size_t max_slots =
        static_cast<std::size_t>(foundation::Handle<GpuInstanceTag>::InvalidIndex);
    const std::size_t old_capacity = slots_.capacity();
    const std::size_t next_capacity =
        old_capacity == 0 ? std::min<std::size_t>(64, max_slots)
                          : std::min(max_slots, old_capacity > max_slots / 2
                                                  ? max_slots
                                                  : old_capacity * 2);
    slots_.reserve(next_capacity);
    dirty_.reserve(next_capacity);
    present_.reserve(next_capacity);
}

void GpuScene::appendDirtyUploads(GpuUploadBatch& batch) {
    std::uint32_t index = 0;
    while (index < dirty_.size()) {
        if (dirty_[index] == 0) {
            ++index;
            continue;
        }
        const std::uint32_t first = index;
        while (index < dirty_.size() && dirty_[index] != 0) {
            ++index;
        }
        const std::uint32_t count = index - first;
        const std::size_t payload_offset = batch.payload.size();
        batch.payload.reserve(batch.payload.size() + count);
        for (std::uint32_t slot_index = first; slot_index < index; ++slot_index) {
            batch.payload.push_back(slots_[slot_index].record);
            dirty_[slot_index] = 0;
        }
        batch.ranges.push_back({first, count, payload_offset});
    }
}

foundation::Result<GpuUploadBatch, foundation::Error> GpuScene::apply(
    std::span<const RenderChange> changes) {
    std::unordered_set<foundation::StableId> seen;
    seen.reserve(changes.size());
    for (const RenderChange& change : changes) {
        if (change.semantic_id == 0 || !seen.insert(change.semantic_id).second) {
            return foundation::Result<GpuUploadBatch, foundation::Error>::failure(
                invalid("GPU scene received duplicate or invalid render delta"));
        }
        if (change.kind != RenderChangeKind::Removed &&
            (change.instance.object_id != change.semantic_id ||
             !validInstance(change.instance))) {
            return foundation::Result<GpuUploadBatch, foundation::Error>::failure(
                invalid("GPU scene received an invalid render delta payload"));
        }
        const GpuInstanceHandle handle = find(change.semantic_id);
        if (change.kind == RenderChangeKind::Added && handle.isValid()) {
            return foundation::Result<GpuUploadBatch, foundation::Error>::failure(
                invalid("GPU scene delta adds an already allocated semantic ID"));
        }
        if (change.kind != RenderChangeKind::Added && !valid(handle)) {
            return foundation::Result<GpuUploadBatch, foundation::Error>::failure(
                invalid("GPU scene delta references a missing semantic ID"));
        }
    }

    GpuUploadBatch batch{};
    for (const RenderChange& change : changes) {
        if (change.kind == RenderChangeKind::Added) {
            const auto allocated = allocate(change.semantic_id);
            if (!allocated) {
                return foundation::Result<GpuUploadBatch, foundation::Error>::failure(
                    allocated.error());
            }
            Slot& slot = slots_[allocated.value().index];
            slot.record = GpuInstanceRecord::fromRender(change.instance);
            markDirty(allocated.value().index);
        } else if (change.kind == RenderChangeKind::Updated) {
            const GpuInstanceHandle handle = find(change.semantic_id);
            Slot& slot = slots_[handle.index];
            const GpuInstanceRecord next = GpuInstanceRecord::fromRender(change.instance);
            if (!(slot.record == next)) {
                slot.record = next;
                markDirty(handle.index);
            }
        } else {
            removeSlot(find(change.semantic_id), batch);
        }
    }
    appendDirtyUploads(batch);
    if (!batch.empty()) {
        ++revision_;
    }
    return foundation::Result<GpuUploadBatch, foundation::Error>::success(std::move(batch));
}

GpuUploadBatch GpuScene::fullUpload() const {
    GpuUploadBatch batch{};
    if (slots_.empty()) {
        return batch;
    }
    batch.payload.reserve(slots_.size());
    for (const Slot& slot : slots_) {
        batch.payload.push_back(slot.alive ? slot.record : GpuInstanceRecord{});
    }
    batch.ranges.push_back({0, static_cast<std::uint32_t>(slots_.size()), 0});
    return batch;
}

foundation::Result<GpuUploadBatch, foundation::Error> GpuScene::sync(
    std::span<const RenderInstance> instances) {
    if (present_.size() < slots_.size()) {
        present_.resize(slots_.size(), std::uint8_t{0});
    }
    std::fill(present_.begin(), present_.end(), std::uint8_t{0});

    // Validate semantic uniqueness before mutating slots. Existing handles
    // use the reusable present marker; IDs that are new in this snapshot are
    // temporarily represented by an invalid map value and removed again after
    // validation. This avoids constructing a hash-set for every unchanged
    // frame while still keeping invalid snapshots transactional.
    std::vector<foundation::StableId> staged_ids;
    const auto rollback_validation = [&]() noexcept {
        for (const foundation::StableId id : staged_ids) {
            semantic_to_handle_.erase(id);
        }
        std::fill(present_.begin(), present_.end(), std::uint8_t{0});
    };
    for (const RenderInstance& instance : instances) {
        if (!validInstance(instance)) {
            rollback_validation();
            return foundation::Result<GpuUploadBatch, foundation::Error>::failure(
                invalid("GPU scene received an invalid render instance"));
        }
        const auto iterator = semantic_to_handle_.find(instance.object_id);
        if (iterator == semantic_to_handle_.end()) {
            semantic_to_handle_.emplace(instance.object_id, GpuInstanceHandle{});
            staged_ids.push_back(instance.object_id);
            continue;
        }
        if (!iterator->second.isValid() || present_[iterator->second.index] != 0) {
            rollback_validation();
            return foundation::Result<GpuUploadBatch, foundation::Error>::failure(
                invalid("GPU scene received duplicate semantic instance ID"));
        }
        present_[iterator->second.index] = 1;
    }
    for (const foundation::StableId id : staged_ids) {
        semantic_to_handle_.erase(id);
    }
    std::fill(present_.begin(), present_.end(), std::uint8_t{0});

    GpuUploadBatch batch{};
    for (const RenderInstance& instance : instances) {
        GpuInstanceHandle handle = find(instance.object_id);
        if (!handle.isValid()) {
            const auto allocated = allocate(instance.object_id);
            if (!allocated) {
                return foundation::Result<GpuUploadBatch, foundation::Error>::failure(
                    allocated.error());
            }
            handle = allocated.value();
            if (present_.size() < slots_.size()) {
                present_.resize(slots_.size(), 0);
            }
        }
        Slot& slot = slots_[handle.index];
        if (present_[handle.index] != 0) {
            return foundation::Result<GpuUploadBatch, foundation::Error>::failure(
                invalid("GPU scene received duplicate semantic instance ID"));
        }
        present_[handle.index] = 1;
        const GpuInstanceRecord next = GpuInstanceRecord::fromRender(instance);
        if (!(slot.record == next)) {
            slot.record = next;
            markDirty(handle.index);
        }
    }

    for (std::uint32_t index = 0; index < slots_.size(); ++index) {
        const GpuInstanceHandle handle{index, slots_[index].generation};
        if (slots_[index].alive && (index >= present_.size() || present_[index] == 0)) {
            removeSlot(handle, batch);
        }
    }

    appendDirtyUploads(batch);

    if (!batch.empty()) {
        ++revision_;
    }
    return foundation::Result<GpuUploadBatch, foundation::Error>::success(std::move(batch));
}

foundation::Result<GpuUploadBatch, foundation::Error> GpuScene::release(
    std::span<const GpuInstanceHandle> handles) {
    std::vector<GpuInstanceHandle> sorted(handles.begin(), handles.end());
    std::sort(sorted.begin(), sorted.end());
    for (std::size_t index = 0; index < sorted.size(); ++index) {
        if (!valid(sorted[index]) || (index != 0 && sorted[index] == sorted[index - 1])) {
            return foundation::Result<GpuUploadBatch, foundation::Error>::failure(
                invalid("GPU scene release contains an invalid or duplicate handle"));
        }
    }

    GpuUploadBatch batch{};
    for (const GpuInstanceHandle handle : sorted) {
        removeSlot(handle, batch);
    }
    appendDirtyUploads(batch);
    if (!batch.empty()) {
        ++revision_;
    }
    return foundation::Result<GpuUploadBatch, foundation::Error>::success(std::move(batch));
}

std::size_t GpuScene::memoryBytes() const noexcept {
    return slots_.capacity() * sizeof(Slot) + dirty_.capacity() * sizeof(std::uint8_t) +
           present_.capacity() * sizeof(std::uint8_t) +
           free_indices_.capacity() * sizeof(std::uint32_t) +
           semantic_to_handle_.size() * (sizeof(foundation::StableId) +
                                         sizeof(GpuInstanceHandle));
}

void GpuScene::clear() noexcept {
    semantic_to_handle_.clear();
    slots_.clear();
    free_indices_.clear();
    dirty_.clear();
    present_.clear();
    live_count_ = 0;
    ++revision_;
}

} // namespace genomes::render::gpu_scene
