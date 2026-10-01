#pragma once

#include <genomes/world_core/BuildingSite.hpp>

namespace genomes::world {

// Transitional compatibility surface. New neutral consumers include
// <genomes/world_core/BuildingSite.hpp> and use genomes::world_core.
using world_core::BuildingSiteRequest;
using world_core::BuildingSiteResolution;
using world_core::SiteAccessClass;
using world_core::SiteAccessSurface;

} // namespace genomes::world
