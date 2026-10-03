#pragma once

#include <genomes/memory/LinearArena.hpp>

#include <cstddef>
#include <memory_resource>

namespace genomes::jobs {

class ScratchContext final {
public:
    explicit ScratchContext(std::size_t capacity = 64U * 1024U)
        : arena_(capacity), resource_(arena_) {}

    [[nodiscard]] void* allocate(std::size_t size,
                                 std::size_t alignment = alignof(std::max_align_t)) {
        return arena_.allocate(size, alignment);
    }
    [[nodiscard]] std::pmr::memory_resource* resource() noexcept { return &resource_; }
    [[nodiscard]] std::size_t bytesUsed() const noexcept { return arena_.bytesUsed(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return arena_.capacity(); }

private:
    friend class JobSystem;
    void reset() noexcept { arena_.reset(); }

    memory::LinearArena arena_;
    memory::LinearArenaResource resource_;
};

} // namespace genomes::jobs
