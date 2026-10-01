#pragma once

#include <optional>

#if GENOMES_HAS_INFANTRY
#include <genomes/combat/TacticalAI.hpp>
#endif

namespace genomes::runtime {

class SceneDirector;

struct BuiltinSceneConfig final {
    bool real_battlefield{true};
#if GENOMES_HAS_INFANTRY
    std::optional<combat::TacticalAIProfile> tactical_ai_profile;
#endif
};

// Registers the application-level scene catalog in one place.  Headless tools
// can request a boundary placeholder for the heavy battlefield while the game
// registers the real simulation scene; menu and game therefore share the same
// scene IDs and command flow.
void registerBuiltinScenes(SceneDirector&, BuiltinSceneConfig config);
void registerBuiltinScenes(SceneDirector&, bool real_battlefield = true);

} // namespace genomes::runtime
