#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/buildings/BuildingProfile.hpp>
#include <genomes/camera/Camera.hpp>
#include <genomes/game_scenes/WorldConfig.hpp>
#include <genomes/world/WorldGenerationProfile.hpp>

#include <functional>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#if GENOMES_HAS_INFANTRY
#include <genomes/combat/TacticalAI.hpp>
#include <genomes/infantry/AppearanceCatalog.hpp>
#endif

namespace genomes::runtime {

class Scene;
class SceneDirector;

} // namespace genomes::runtime

namespace genomes::application {

struct BuiltinSceneConfig final {
    bool real_battlefield{true};
    std::shared_ptr<const world::FrozenWorldGenerationProfile> world_generation_profile;
    std::shared_ptr<const buildings::FrozenBuildingProfile> building_profile;
#if GENOMES_HAS_INFANTRY
    std::optional<combat::TacticalAIProfile> tactical_ai_profile;
    std::shared_ptr<const infantry::FrozenAppearanceCatalog> appearance_catalog;
#endif
    std::size_t live_animation_budget{96U};
    camera::RtsCameraSettings rts_controls{};
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
    explicit BuiltinSceneCatalog(BuiltinSceneConfig config);

    [[nodiscard]] std::span<const BuiltinSceneEntry> entries() const noexcept {
        return entries_;
    }

    void install(runtime::SceneDirector&) const;

private:
    std::vector<BuiltinSceneEntry> entries_;
    std::shared_ptr<application::WorldGenerationConfig> active_world_config_;
};

// The application composition root owns the built-in scene catalog.  The
// neutral runtime namespace is used only for lifecycle types referenced by
// this boundary; catalog ownership remains in genomes::application.
void configureBuiltinSceneRouting(
    runtime::SceneDirector&,
    std::shared_ptr<WorldGenerationConfig> active_config);
void registerBuiltinScenes(runtime::SceneDirector&, BuiltinSceneConfig config);

} // namespace genomes::application
