#pragma once

#include <genomes/world_core/WorldQuerySnapshot.hpp>

namespace genomes::world {

// Transitional compatibility surface. New neutral consumers include
// <genomes/world_core/WorldQuerySnapshot.hpp> and use genomes::world_core.
using world_core::QueryAabbResult;
using world_core::QueryCandidate;
using world_core::QueryCompleteness;
using world_core::QueryRegion;
using world_core::QuerySegmentHit;
using world_core::QuerySegmentResult;
using world_core::QuerySourceKind;
using world_core::TerrainSampleResult;
using world_core::WorldQuerySnapshot;
using world_core::WorldQuerySnapshotVersion;

} // namespace genomes::world
