#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/simulation/ComponentType.hpp>
#include <genomes/simulation/Entity.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace genomes::simulation {

struct ChunkColumn final {
    ComponentTypeInfo type{};
    std::size_t offset{0};
};

struct ChunkLayout final {
    std::size_t capacity{0};
    std::size_t payload_bytes{0};
    std::vector<ChunkColumn> columns;

    [[nodiscard]] const ChunkColumn* find(ComponentTypeId id) const noexcept;

    [[nodiscard]] static foundation::Result<ChunkLayout, foundation::Error> build(
        std::span<const ComponentTypeInfo> types,
        std::size_t target_payload_bytes = 32u * 1024u);
};

// One archetype chunk owns an entity column and one contiguous aligned column
// per component. Structural mutation is expected at commit boundaries.
class ChunkStorage final {
public:
    explicit ChunkStorage(ChunkLayout layout);
    ~ChunkStorage();

    ChunkStorage(const ChunkStorage&) = delete;
    ChunkStorage& operator=(const ChunkStorage&) = delete;
    ChunkStorage(ChunkStorage&&) noexcept;
    ChunkStorage& operator=(ChunkStorage&&) noexcept;

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return layout_.capacity; }
    [[nodiscard]] bool full() const noexcept { return size_ >= capacity(); }
    [[nodiscard]] const ChunkLayout& layout() const noexcept { return layout_; }
    [[nodiscard]] std::span<const EntityId> entities() const noexcept {
        return {entities_.data(), size_};
    }
    [[nodiscard]] std::span<EntityId> entities() noexcept {
        return {entities_.data(), size_};
    }

    [[nodiscard]] std::uint32_t append(EntityId entity) noexcept;
    [[nodiscard]] EntityId removeSwap(std::uint32_t row) noexcept;

    [[nodiscard]] void* component(ComponentTypeId id, std::uint32_t row) noexcept;
    [[nodiscard]] const void* component(ComponentTypeId id, std::uint32_t row) const noexcept;
    [[nodiscard]] void* columnData(ComponentTypeId id) noexcept;
    [[nodiscard]] const void* columnData(ComponentTypeId id) const noexcept;
    [[nodiscard]] std::size_t componentStride(ComponentTypeId id) const noexcept;

    // Component lifecycle helpers used by archetype structural moves.
    void defaultConstruct(ComponentTypeId id, std::uint32_t row) noexcept;
    void copyConstructFrom(const ChunkStorage& source,
                           ComponentTypeId id,
                           std::uint32_t destination_row,
                           std::uint32_t source_row) noexcept;
    void moveConstructFrom(ChunkStorage& source,
                           ComponentTypeId id,
                           std::uint32_t destination_row,
                           std::uint32_t source_row) noexcept;
    void copyAssignFrom(const ChunkStorage& source,
                        ComponentTypeId id,
                        std::uint32_t destination_row,
                        std::uint32_t source_row) noexcept;
    void moveAssignFrom(ChunkStorage& source,
                        ComponentTypeId id,
                        std::uint32_t destination_row,
                        std::uint32_t source_row) noexcept;
    void copyAssignRaw(ComponentTypeId id,
                       std::uint32_t destination_row,
                       const void* source) noexcept;
    void destroy(ComponentTypeId id, std::uint32_t row) noexcept;

private:
    struct AlignedDeleter final {
        std::size_t alignment{alignof(std::max_align_t)};
        void operator()(std::byte* value) const noexcept;
    };

    [[nodiscard]] std::byte* columnAddress(const ChunkColumn& column,
                                            std::uint32_t row) noexcept;
    [[nodiscard]] const std::byte* columnAddress(const ChunkColumn& column,
                                                  std::uint32_t row) const noexcept;

    ChunkLayout layout_;
    std::vector<EntityId> entities_;
    std::unique_ptr<std::byte[], AlignedDeleter> payload_{nullptr};
    std::size_t size_{0};
};

} // namespace genomes::simulation
