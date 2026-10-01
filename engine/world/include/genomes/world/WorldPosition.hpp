#pragma once

#include <genomes/world_core/WorldPosition.hpp>

namespace genomes::world {

// Transitional compatibility surface. New neutral consumers include
// <genomes/world_core/WorldPosition.hpp> and use genomes::world_core.
using world_core::RegionCoord;
using world_core::RegionId;
using world_core::RegionLocalPosition;
using world_core::WorldCoordinateConfig;
using world_core::WorldId;
using world_core::WorldPosition;
using world_core::regionCoordFor;
using world_core::regionId;
using world_core::regionOrigin;
using world_core::toGlobal;
using world_core::toLocal;

} // namespace genomes::world
