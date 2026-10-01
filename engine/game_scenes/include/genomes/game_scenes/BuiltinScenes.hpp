#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/game_scenes/WorldConfig.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#if GENOMES_HAS_INFANTRY
#include <genomes/combat/TacticalAI.hpp>
#endif

namespace genomes::runtime {

class Scene;
class SceneDirector;

struct BuiltinSceneConfig final {
    bool real_battlefield{true};
#if GENOMES_HAS_INFANTRY
    std::optional<combat::TacticalAIProfile> tactical_ai_profile;
#endif
};

using BuiltinSceneFactory = std::function<std::unique_ptr<Scene>()>;

struct BuiltinSceneEntry final {
    foundation::SceneId id;
    BuiltinSceneFactory factory;
};

// Application composition owns this immutable set of product factories. The
// runtime only receives the resulting registrations and keeps scene lifecycle
// (start/change/update) independent from the product catalog.
class BuiltinSceneCatalog final {
public:
    explicit BuiltinSceneCatalog(BuiltinSceneConfig config = {});

    [[nodiscard]] std::span<const BuiltinSceneEntry> entries() const noexcept {
        return entries_;
    }

    void install(SceneDirector&) const;

private:
    std::vector<BuiltinSceneEntry> entries_;
    std::shared_ptr<application::WorldGenerationConfig> active_world_config_;
};

// The application composition root owns the built-in scene catalog.  The
// runtime namespace remains a compatibility name while callers migrate to
// this application-scene header.
void configureBuiltinSceneRouting(
    SceneDirector&, std::shared_ptr<application::WorldGenerationConfig> active_config = {});
void registerBuiltinScenes(SceneDirector&, BuiltinSceneConfig config);
void registerBuiltinScenes(SceneDirector&, bool real_battlefield = true);

} // namespace genomes::runtime
