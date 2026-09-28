#include <genomes/ballistics/ProjectileStorage.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace genomes::ballistics {

namespace {

[[nodiscard]] foundation::Error error(foundation::ErrorCode code,
                                      std::string_view message) noexcept {
    return {code, message};
}

} // namespace

ProjectileStorage::ProjectileStorage(ProjectileStorageConfig config) : config_{config} {
    if (!config_.valid()) {
        config_ = {};
    }
    slots_.reserve(config_.capacity);
    free_slots_.reserve(config_.capacity);
    dense_handles_.reserve(config_.capacity);
    ids_.reserve(config_.capacity);
}

std::uint32_t ProjectileStorage::allocateSlot() noexcept {
    if (!free_slots_.empty()) {
        const std::uint32_t slot = free_slots_.back();
        free_slots_.pop_back();
        return slot;
    }
    if (slots_.size() >= config_.capacity || slots_.size() >=
                                                static_cast<std::size_t>(
                                                    std::numeric_limits<std::uint32_t>::max())) {
        return std::numeric_limits<std::uint32_t>::max();
    }
    slots_.push_back({});
    return static_cast<std::uint32_t>(slots_.size() - 1U);
}

foundation::Result<ProjectileHandle, foundation::Error> ProjectileStorage::insert(
    ProjectileState state) {
    if (!state.valid()) {
        return foundation::Result<ProjectileHandle, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument, "invalid projectile state"));
    }
    if (ids_.find(state.projectile_id.value()) != ids_.end()) {
        return foundation::Result<ProjectileHandle, foundation::Error>::failure(
            error(foundation::ErrorCode::InvalidArgument, "duplicate projectile semantic ID"));
    }
    const std::uint32_t slot_index = allocateSlot();
    if (slot_index == std::numeric_limits<std::uint32_t>::max()) {
        return foundation::Result<ProjectileHandle, foundation::Error>::failure(
            error(foundation::ErrorCode::OutOfRange, "projectile storage capacity exceeded"));
    }
    Slot& slot = slots_[slot_index];
    slot.state = std::move(state);
    slot.occupied = true;
    slot.dense_index = static_cast<std::uint32_t>(dense_handles_.size());
    const ProjectileHandle handle{slot_index, slot.generation};
    dense_handles_.push_back(handle);
    ids_.emplace(slot.state.projectile_id.value(), handle);
    return foundation::Result<ProjectileHandle, foundation::Error>::success(handle);
}

bool ProjectileStorage::owns(ProjectileHandle handle) const noexcept {
    return handle.valid() && handle.slot < slots_.size() && slots_[handle.slot].occupied &&
           slots_[handle.slot].generation == handle.generation;
}

void ProjectileStorage::releaseSlot(std::uint32_t slot_index) noexcept {
    Slot& slot = slots_[slot_index];
    slot.occupied = false;
    slot.state = {};
    if (slot.generation == std::numeric_limits<std::uint32_t>::max()) {
        slot.generation = 1U;
    } else {
        ++slot.generation;
    }
    free_slots_.push_back(slot_index);
}

bool ProjectileStorage::erase(ProjectileHandle handle) noexcept {
    if (!owns(handle)) {
        return false;
    }
    Slot& slot = slots_[handle.slot];
    ids_.erase(slot.state.projectile_id.value());
    const std::uint32_t dense_index = slot.dense_index;
    const ProjectileHandle moved = dense_handles_.back();
    dense_handles_[dense_index] = moved;
    slots_[moved.slot].dense_index = dense_index;
    dense_handles_.pop_back();
    releaseSlot(handle.slot);
    return true;
}

ProjectileState* ProjectileStorage::get(ProjectileHandle handle) noexcept {
    return owns(handle) ? &slots_[handle.slot].state : nullptr;
}

const ProjectileState* ProjectileStorage::get(ProjectileHandle handle) const noexcept {
    return owns(handle) ? &slots_[handle.slot].state : nullptr;
}

ProjectileHandle ProjectileStorage::find(ProjectileId id) const noexcept {
    const auto iterator = ids_.find(id.value());
    return iterator == ids_.end() ? ProjectileHandle{} : iterator->second;
}

void ProjectileStorage::stableCompact() noexcept {
    std::sort(dense_handles_.begin(), dense_handles_.end(), [this](ProjectileHandle left,
                                                                   ProjectileHandle right) {
        const auto& left_state = slots_[left.slot].state;
        const auto& right_state = slots_[right.slot].state;
        if (left_state.projectile_id != right_state.projectile_id) {
            return left_state.projectile_id < right_state.projectile_id;
        }
        return left.slot < right.slot;
    });
    for (std::size_t index = 0; index < dense_handles_.size(); ++index) {
        slots_[dense_handles_[index].slot].dense_index = static_cast<std::uint32_t>(index);
    }
}

std::vector<ProjectileHandle> ProjectileStorage::stableHandles() const {
    std::vector<ProjectileHandle> result = dense_handles_;
    std::sort(result.begin(), result.end(), [this](ProjectileHandle left,
                                                   ProjectileHandle right) {
        const auto& left_state = slots_[left.slot].state;
        const auto& right_state = slots_[right.slot].state;
        if (left_state.projectile_id != right_state.projectile_id) {
            return left_state.projectile_id < right_state.projectile_id;
        }
        return left.slot < right.slot;
    });
    return result;
}

std::vector<ProjectileState> ProjectileStorage::snapshot() const {
    const std::vector<ProjectileHandle> handles = stableHandles();
    std::vector<ProjectileState> result;
    result.reserve(handles.size());
    for (const ProjectileHandle handle : handles) {
        result.push_back(*get(handle));
    }
    return result;
}

std::vector<ProjectileChunk> ProjectileStorage::activeChunks() const {
    std::vector<ProjectileChunk> result;
    if (dense_handles_.empty()) {
        return result;
    }
    const std::size_t chunk_count =
        (dense_handles_.size() + config_.chunk_size - 1U) / config_.chunk_size;
    result.reserve(chunk_count);
    for (std::size_t index = 0; index < chunk_count; ++index) {
        const std::size_t begin = index * config_.chunk_size;
        const std::size_t end = std::min(dense_handles_.size(), begin + config_.chunk_size);
        result.push_back({index, std::span<const ProjectileHandle>(dense_handles_.data() + begin,
                                                                     end - begin)});
    }
    return result;
}

} // namespace genomes::ballistics
