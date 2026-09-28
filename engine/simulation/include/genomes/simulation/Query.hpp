#pragma once

#include <genomes/simulation/Archetype.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace genomes::simulation {

struct QueryDescription final {
    std::vector<ComponentTypeId> required;
    std::vector<ComponentTypeId> optional;
    std::vector<ComponentTypeId> excluded;
};

struct QueryPlan final {
    std::uint64_t archetype_revision{0};
    std::vector<std::size_t> archetypes;
};

struct QueryChunkView final {
    const ChunkStorage* storage{nullptr};

    [[nodiscard]] std::size_t size() const noexcept {
        return storage == nullptr ? 0 : storage->size();
    }
    [[nodiscard]] std::span<const EntityId> entities() const noexcept {
        return storage == nullptr ? std::span<const EntityId>{} : storage->entities();
    }
    [[nodiscard]] const void* column(ComponentTypeId id) const noexcept {
        return storage == nullptr ? nullptr : storage->columnData(id);
    }
    [[nodiscard]] std::size_t stride(ComponentTypeId id) const noexcept {
        return storage == nullptr ? 0 : storage->componentStride(id);
    }
};

} // namespace genomes::simulation
