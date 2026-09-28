#include <genomes/simulation/Archetype.hpp>

#include <algorithm>
#include <utility>

namespace genomes::simulation {

bool ArchetypeKey::valid() const noexcept {
    return std::all_of(components.begin(), components.end(), [](ComponentTypeId id) {
               return id != 0;
           }) &&
           std::adjacent_find(components.begin(), components.end()) == components.end() &&
           std::is_sorted(components.begin(), components.end());
}

bool ArchetypeKey::contains(ComponentTypeId id) const noexcept {
    return std::binary_search(components.begin(), components.end(), id);
}

foundation::Result<Archetype, foundation::Error> Archetype::create(
    ArchetypeKey key,
    std::span<const ComponentTypeInfo> types,
    std::size_t target_payload_bytes) {
    if (!key.valid() || key.components.size() != types.size()) {
        return foundation::Result<Archetype, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid archetype key"});
    }
    for (std::size_t index = 0; index < types.size(); ++index) {
        if (!types[index].valid() || types[index].id != key.components[index]) {
            return foundation::Result<Archetype, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidArgument,
                 "archetype component metadata does not match key"});
        }
    }
    auto layout = ChunkLayout::build(types, target_payload_bytes);
    if (!layout) {
        return foundation::Result<Archetype, foundation::Error>::failure(layout.error());
    }
    return foundation::Result<Archetype, foundation::Error>::success(
        Archetype{std::move(key), std::move(layout.value())});
}

ChunkRow Archetype::append(EntityId entity) noexcept {
    if (chunks_.empty() || chunks_.back().full()) {
        chunks_.emplace_back(layout_);
    }
    const std::uint32_t chunk_index = static_cast<std::uint32_t>(chunks_.size() - 1);
    const std::uint32_t row = chunks_.back().append(entity);
    if (row == foundation::Handle<EntityTag>::InvalidIndex) {
        return {};
    }
    return {chunk_index, row};
}

EntityId Archetype::removeSwap(ChunkRow row) noexcept {
    if (!row.valid() || row.chunk >= chunks_.size()) {
        return {};
    }
    return chunks_[row.chunk].removeSwap(row.row);
}

} // namespace genomes::simulation
