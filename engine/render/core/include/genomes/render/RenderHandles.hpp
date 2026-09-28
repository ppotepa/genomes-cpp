#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Handle.hpp>
#include <genomes/foundation/Result.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace genomes::render {

struct BufferTag;
struct TextureTag;
struct SamplerTag;
struct PipelineTag;

using BufferHandle = foundation::Handle<BufferTag>;
using TextureHandle = foundation::Handle<TextureTag>;
using SamplerHandle = foundation::Handle<SamplerTag>;
using PipelineHandle = foundation::Handle<PipelineTag>;

template <class Tag, class Value>
class RenderResourcePool final {
public:
    using Handle = foundation::Handle<Tag>;

    [[nodiscard]] foundation::Result<Handle, foundation::Error> create(Value value) {
        std::uint32_t index = foundation::Handle<Tag>::InvalidIndex;
        if (!free_indices_.empty()) {
            index = free_indices_.back();
            free_indices_.pop_back();
        } else {
            if (slots_.size() >= foundation::Handle<Tag>::InvalidIndex) {
                return foundation::Result<Handle, foundation::Error>::failure(
                    {foundation::ErrorCode::OutOfRange, "render resource pool capacity exhausted"});
            }
            index = static_cast<std::uint32_t>(slots_.size());
            slots_.push_back({});
        }
        Slot& slot = slots_[index];
        slot.value.emplace(std::move(value));
        slot.alive = true;
        return foundation::Result<Handle, foundation::Error>::success(
            {index, slot.generation});
    }

    [[nodiscard]] bool contains(Handle handle) const noexcept {
        return handle.isValid() && handle.index < slots_.size() &&
               slots_[handle.index].alive &&
               slots_[handle.index].generation == handle.generation;
    }

    [[nodiscard]] Value* get(Handle handle) noexcept {
        return contains(handle) ? &*slots_[handle.index].value : nullptr;
    }

    [[nodiscard]] const Value* get(Handle handle) const noexcept {
        return contains(handle) ? &*slots_[handle.index].value : nullptr;
    }

    [[nodiscard]] foundation::Result<Value, foundation::Error> destroy(Handle handle) {
        if (!contains(handle)) {
            return foundation::Result<Value, foundation::Error>::failure(
                {foundation::ErrorCode::NotFound, "stale render resource handle"});
        }
        Slot& slot = slots_[handle.index];
        Value value = std::move(*slot.value);
        slot.value.reset();
        slot.alive = false;
        ++slot.generation;
        if (slot.generation == 0) {
            slot.generation = 1;
        }
        free_indices_.push_back(handle.index);
        return foundation::Result<Value, foundation::Error>::success(std::move(value));
    }

    void clear() noexcept {
        slots_.clear();
        free_indices_.clear();
    }

    [[nodiscard]] std::size_t liveCount() const noexcept {
        std::size_t result = 0;
        for (const Slot& slot : slots_) {
            result += slot.alive ? 1u : 0u;
        }
        return result;
    }

private:
    struct Slot final {
        std::optional<Value> value;
        std::uint32_t generation{1};
        bool alive{false};
    };

    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_indices_;
};

template <class Value>
class DeferredReleaseQueue final {
public:
    using RetireCallback = std::function<void(Value&&)>;

    void enqueue(std::uint64_t fence, Value value) {
        entries_.push_back({fence, std::move(value)});
    }

    [[nodiscard]] std::size_t retire(std::uint64_t completed_fence,
                                     const RetireCallback& callback) {
        std::size_t retired = 0;
        auto iterator = entries_.begin();
        while (iterator != entries_.end()) {
            if (iterator->fence > completed_fence) {
                ++iterator;
                continue;
            }
            callback(std::move(iterator->value));
            iterator = entries_.erase(iterator);
            ++retired;
        }
        return retired;
    }

    [[nodiscard]] std::size_t pending() const noexcept { return entries_.size(); }

    void clear() noexcept { entries_.clear(); }

private:
    struct Entry final {
        std::uint64_t fence{0};
        Value value;
    };

    std::vector<Entry> entries_;
};

} // namespace genomes::render
