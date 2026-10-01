#pragma once

#include <genomes/world/WorldPosition.hpp>
#include <genomes/world_core/DirtyBounds.hpp>

namespace genomes::world {

// Transitional compatibility surface. New neutral consumers include
// <genomes/world_core/DirtyBounds.hpp> and use genomes::world_core.
using world_core::Aabb3;
using world_core::DirtyBounds;
using world_core::DirtyReason;
using world_core::DirtyReasonMask;
using world_core::hasReason;
using world_core::operator|;

} // namespace genomes::world
