#pragma once

#include <genomes/world/WorldPosition.hpp>
#include <genomes/world_core/WorldQuery.hpp>

namespace genomes::world {

// Transitional compatibility surface. New neutral consumers include
// <genomes/world_core/WorldQuery.hpp> and use genomes::world_core.
using world_core::QuerySegmentRequest;
using world_core::WorldQueryService;
using world_core::querySegments;

} // namespace genomes::world
