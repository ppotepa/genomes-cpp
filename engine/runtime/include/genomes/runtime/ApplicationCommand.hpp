#pragma once

// Transitional compatibility include. ApplicationCommand is owned by the
// product/application scene layer; runtime transports only SceneCommand.
#include <genomes/game_scenes/ApplicationCommand.hpp>

namespace genomes::runtime {
using application::ApplicationCommand;
using application::ApplicationCommandKind;
using application::enqueueApplicationCommand;
using application::makeApplicationCommand;
} // namespace genomes::runtime
