#pragma once

namespace genomes::runtime {

class SceneDirector;

// Registers the application-level scene catalog in one place.  Headless tools
// can request a boundary placeholder for the heavy battlefield while the game
// registers the real simulation scene; menu and game therefore share the same
// scene IDs and command flow.
void registerBuiltinScenes(SceneDirector&, bool real_battlefield = true);

} // namespace genomes::runtime
