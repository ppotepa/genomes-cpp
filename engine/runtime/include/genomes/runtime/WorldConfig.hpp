#pragma once

#include <genomes/world/WorldPlan.hpp>

namespace genomes::runtime {

// Runtime screens expose the same request object consumed by the world
// generator. Keeping one contract prevents UI defaults from drifting away
// from the procedural pipeline.
using WorldGenerationConfig = world::WorldGenerationRequest;

} // namespace genomes::runtime
