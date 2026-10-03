#pragma once

#include <cstddef>
#include <memory>
#include <memory_resource>
#include <vector>

namespace genomes::memory {

class LinearArena final {
public:
    explicit LinearArena(std::size_t initial_capacity = 64U * 1024U);

    LinearArena(const LinearArena&) = delete;
    LinearArena& operator=(const LinearArena&) = delete;
    LinearArena(LinearArena&&) noexcept = default;
    LinearArena& operator=(LinearArena&&) noexcept = default;

    [[nodiscard]] void* allocate(std::size_t size,
                                 std::size_t alignment = alignof(std::max_align_t));
    void reset() noexcept;

    [[nodiscard]] std::size_t bytesUsed() const noexcept { return bytes_used_; }
    [[nodiscard]] std::size_t capacity() const noexcept;

private:
    struct Block final {
        std::unique_ptr<std::byte[]> storage;
        std::size_t capacity{0};
        std::size_t offset{0};
    };

    void addBlock(std::size_t minimum_capacity);

    std::vector<Block> blocks_;
    std::size_t initial_capacity_{0};
    std::size_t current_block_{0};
    std::size_t bytes_used_{0};
};

class LinearArenaResource final : public std::pmr::memory_resource {
public:
    explicit LinearArenaResource(LinearArena& arena) noexcept : arena_(arena) {}

private:
    [[nodiscard]] void* do_allocate(std::size_t bytes, std::size_t alignment) override;
    void do_deallocate(void*, std::size_t, std::size_t) noexcept override {}
    [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }

    LinearArena& arena_;
};

} // namespace genomes::memory
