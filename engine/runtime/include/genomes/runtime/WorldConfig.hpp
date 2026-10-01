#pragma once

#include <genomes/game_scenes/WorldConfig.hpp>

namespace genomes::runtime {

// Transitional aliases for clients that still include the former runtime
// path. New product code includes genomes/game_scenes/WorldConfig.hpp.
using WorldGenerationConfig = application::WorldGenerationConfig;
using WorldSeedMode = application::WorldSeedMode;
using WorldSeedInput = application::WorldSeedInput;

} // namespace genomes::runtime
