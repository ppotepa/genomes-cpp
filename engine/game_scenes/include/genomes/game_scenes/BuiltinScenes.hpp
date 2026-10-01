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

// The application composition root owns the built-in scene catalog.  The
// runtime namespace remains a compatibility name while callers migrate to
// this application-scene header.
void configureBuiltinSceneRouting(SceneDirector&);
void registerBuiltinScenes(SceneDirector&, BuiltinSceneConfig config);
void registerBuiltinScenes(SceneDirector&, bool real_battlefield = true);

} // namespace genomes::runtime
