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

} // namespace genomes::runtime

namespace genomes::application {

struct BuiltinSceneConfig final {
    bool real_battlefield{true};
#if GENOMES_HAS_INFANTRY
    std::optional<combat::TacticalAIProfile> tactical_ai_profile;
#endif
};

using BuiltinSceneFactory = std::function<std::unique_ptr<runtime::Scene>()>;

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

    void install(runtime::SceneDirector&) const;

private:
    std::vector<BuiltinSceneEntry> entries_;
    std::shared_ptr<application::WorldGenerationConfig> active_world_config_;
};

// The application composition root owns the built-in scene catalog.  The
// runtime namespace remains a compatibility name while callers migrate to
// this application-scene header.
void configureBuiltinSceneRouting(
    runtime::SceneDirector&,
    std::shared_ptr<WorldGenerationConfig> active_config = {});
void registerBuiltinScenes(runtime::SceneDirector&, BuiltinSceneConfig config);
void registerBuiltinScenes(runtime::SceneDirector&, bool real_battlefield = true);

} // namespace genomes::application

// Compatibility aliases for clients that have not yet moved their include
// and namespace.  Product ownership remains in genomes::application.
namespace genomes::runtime {
using BuiltinSceneConfig = application::BuiltinSceneConfig;
using BuiltinSceneFactory = application::BuiltinSceneFactory;
using BuiltinSceneEntry = application::BuiltinSceneEntry;
using BuiltinSceneCatalog = application::BuiltinSceneCatalog;
using application::configureBuiltinSceneRouting;
using application::registerBuiltinScenes;
} // namespace genomes::runtime
