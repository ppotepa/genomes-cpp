#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/simulation/ChunkStorage.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace genomes::simulation {

struct ArchetypeKey final {
    std::vector<ComponentTypeId> components;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool contains(ComponentTypeId id) const noexcept;

    friend bool operator==(const ArchetypeKey&, const ArchetypeKey&) noexcept = default;
    friend bool operator<(const ArchetypeKey& left, const ArchetypeKey& right) noexcept {
        return left.components < right.components;
    }
};

struct ChunkRow final {
    std::uint32_t chunk{foundation::Handle<EntityTag>::InvalidIndex};
    std::uint32_t row{foundation::Handle<EntityTag>::InvalidIndex};

    [[nodiscard]] bool valid() const noexcept {
        return chunk != foundation::Handle<EntityTag>::InvalidIndex &&
               row != foundation::Handle<EntityTag>::InvalidIndex;
    }
};

class Archetype final {
public:
    [[nodiscard]] static foundation::Result<Archetype, foundation::Error> create(
        ArchetypeKey key,
        std::span<const ComponentTypeInfo> types,
        std::size_t target_payload_bytes = 32u * 1024u);

    Archetype(Archetype&&) noexcept = default;
    Archetype& operator=(Archetype&&) noexcept = default;
    Archetype(const Archetype&) = delete;
    Archetype& operator=(const Archetype&) = delete;

    [[nodiscard]] const ArchetypeKey& key() const noexcept { return key_; }
    [[nodiscard]] std::size_t chunkCount() const noexcept { return chunks_.size(); }
    [[nodiscard]] const ChunkStorage& chunk(std::size_t index) const noexcept {
        return chunks_[index];
    }
    [[nodiscard]] ChunkStorage& chunk(std::size_t index) noexcept { return chunks_[index]; }

    [[nodiscard]] ChunkRow append(EntityId entity) noexcept;
    [[nodiscard]] EntityId removeSwap(ChunkRow row) noexcept;

private:
    Archetype(ArchetypeKey key, ChunkLayout layout) : key_{std::move(key)}, layout_{std::move(layout)} {}

    ArchetypeKey key_;
    ChunkLayout layout_;
    std::vector<ChunkStorage> chunks_;
};

} // namespace genomes::simulation
