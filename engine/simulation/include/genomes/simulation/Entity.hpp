#pragma once

#include <genomes/foundation/Handle.hpp>

namespace genomes::simulation {

// Runtime identity is deliberately separate from persistent/save identifiers.
// Generation changes on reuse so stale references cannot address a new entity.
struct EntityTag;
using EntityId = foundation::Handle<EntityTag>;

struct EntityLocation final {
    std::uint32_t generation{0};
    std::uint32_t archetype{foundation::Handle<EntityTag>::InvalidIndex};
    std::uint32_t chunk{foundation::Handle<EntityTag>::InvalidIndex};
    std::uint32_t row{foundation::Handle<EntityTag>::InvalidIndex};
    bool alive{false};
};

} // namespace genomes::simulation
