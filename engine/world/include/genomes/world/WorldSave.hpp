#pragma once

#include <genomes/world/WorldPosition.hpp>
#include <genomes/world_core/WorldSave.hpp>

namespace genomes::world {

// Transitional compatibility surface. New neutral consumers include
// <genomes/world_core/WorldSave.hpp> and use genomes::world_core.
using world_core::WorldSaveCodec;
using world_core::WorldSaveEntity;
using world_core::WorldSaveHeaderBytes;
using world_core::WorldSaveLimits;
using world_core::WorldSaveMagic;
using world_core::WorldSaveMaximumBytes;
using world_core::WorldSaveMetadata;
using world_core::WorldSaveModel;
using world_core::WorldSaveRegion;
using world_core::WorldSaveSchemaVersion;

} // namespace genomes::world
