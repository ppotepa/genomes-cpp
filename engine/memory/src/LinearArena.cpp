#include <genomes/memory/LinearArena.hpp>

#include <algorithm>
#include <bit>
#include <limits>
#include <new>

namespace genomes::memory {

namespace {

[[nodiscard]] std::size_t normalizedCapacity(std::size_t value) {
    constexpr std::size_t minimum = 256;
    value = std::max(value, minimum);
    if (value > std::numeric_limits<std::size_t>::max() / 2U) {
        return value;
    }
    return std::bit_ceil(value);
}

} // namespace

LinearArena::LinearArena(std::size_t initial_capacity)
    : initial_capacity_(normalizedCapacity(initial_capacity)) {
    addBlock(initial_capacity_);
}

void* LinearArena::allocate(std::size_t size, std::size_t alignment) {
    if (size == 0) {
        size = 1;
    }
    if (!std::has_single_bit(alignment)) {
        throw std::bad_alloc{};
    }

    for (;;) {
        Block& block = blocks_[current_block_];
        void* cursor = block.storage.get() + block.offset;
        std::size_t space = block.capacity - block.offset;
        void* aligned = std::align(alignment, size, cursor, space);
        if (aligned != nullptr) {
            const auto* base = block.storage.get();
            const auto* result = static_cast<std::byte*>(aligned);
            const std::size_t new_offset = static_cast<std::size_t>(result - base) + size;
            bytes_used_ += new_offset - block.offset;
            block.offset = new_offset;
            return aligned;
        }

        ++current_block_;
        if (current_block_ == blocks_.size()) {
            const std::size_t previous = blocks_.back().capacity;
            const std::size_t required = size > std::numeric_limits<std::size_t>::max() - alignment
                                             ? size
                                             : size + alignment;
            addBlock(std::max(required, previous <= std::numeric_limits<std::size_t>::max() / 2U
                                            ? previous * 2U
                                            : previous));
        }
    }
}

void LinearArena::reset() noexcept {
    for (Block& block : blocks_) {
        block.offset = 0;
    }
    current_block_ = 0;
    bytes_used_ = 0;
}

std::size_t LinearArena::capacity() const noexcept {
    std::size_t result = 0;
    for (const Block& block : blocks_) {
        result += block.capacity;
    }
    return result;
}

void LinearArena::addBlock(std::size_t minimum_capacity) {
    const std::size_t capacity = normalizedCapacity(minimum_capacity);
    blocks_.push_back(Block{std::make_unique<std::byte[]>(capacity), capacity, 0});
}

void* LinearArenaResource::do_allocate(std::size_t bytes, std::size_t alignment) {
    return arena_.allocate(bytes, alignment);
}

} // namespace genomes::memory
