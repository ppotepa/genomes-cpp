#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/buildings/BuildingProfile.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/render/PresentationSnapshot.hpp>
#include <genomes/runtime/FrameCoordinator.hpp>
#include <genomes/runtime/SceneDirector.hpp>
#include <genomes/game_scenes/BuiltinScenes.hpp>
#include <genomes/simulation/SessionSimulationClock.hpp>
#include <genomes/ui/UiRuntime.hpp>
#include <genomes/ui/UiNativePluginManager.hpp>
#include <genomes/world/WorldGenerationProfile.hpp>
#if GENOMES_HAS_INFANTRY
#include <genomes/combat/TacticalAI.hpp>
#include <genomes/infantry/AppearanceCatalog.hpp>
#endif

#if defined(GENOMES_HAS_RMLUI)
#include <RmlUiRuntime.hpp>
#endif

#include <memory>
#include <string>

namespace genomes::platform { class SdlPlatform; }
namespace genomes::platform { class SdlFileDialogService; }
namespace genomes::render {
class IRenderer;
class RenderBackend;
}

namespace genomes::game {

class GameApplication final
#if defined(GENOMES_HAS_RMLUI)
    : public ui::IUiActionRouter
#endif
{
public:
    [[nodiscard]] static foundation::Result<std::unique_ptr<GameApplication>, foundation::Error>
    create();
    ~GameApplication();

    int run(int argc, char** argv);

#if defined(GENOMES_HAS_RMLUI)
    [[nodiscard]] ui::UiActionResult dispatch(ui::UiActionId action,
                                              const ui::UiActionArguments& arguments) override;
#endif

private:
    GameApplication(std::unique_ptr<platform::SdlPlatform> platform,
                    std::unique_ptr<render::IRenderer> renderer,
                    std::unique_ptr<render::RenderBackend> backend_owner,
                    std::shared_ptr<const world::FrozenWorldGenerationProfile> world_profile,
                    std::shared_ptr<const buildings::FrozenBuildingProfile> building_profile
#if GENOMES_HAS_INFANTRY
                    , combat::TacticalAIProfile tactical_ai_profile
                    , std::shared_ptr<const infantry::FrozenAppearanceCatalog> appearance_catalog
#endif
                    );

    [[nodiscard]] foundation::Result<void, foundation::Error> resize_renderer(
        std::uint32_t width, std::uint32_t height);

    std::unique_ptr<platform::SdlPlatform> platform_;
    // Diligent owns the GPU backend for the lifetime of the renderer. Camera
    // control and input ordering belong to SceneDirector, not the renderer.
    std::unique_ptr<render::RenderBackend> backend_owner_;
    std::unique_ptr<render::IRenderer> renderer_;
    // The process scheduler is the single CPU execution authority for the
    // application; this reference keeps the composition root explicit
    // without owning a second worker pool.
    jobs::JobSystem& jobs_;
    ui::UiRuntime ui_;
    ui::UiContentRegistry content_;
    ui::UiNativePluginManager plugins_;
    render::PresentationSnapshot presentation_;
    // SceneDirector owns CameraController; keeping it after presentation_ makes
    // camera/snapshot state outlive the renderer during orderly destruction.
    runtime::SceneDirector director_;
    // The composition root owns the product catalog for the whole session;
    // SceneDirector only owns the registered lifecycle callbacks.
    std::unique_ptr<application::BuiltinSceneCatalog> scene_catalog_;
    // The application owns the single session clock. Scenes receive the
    // resulting TickContext through the director; frame time remains a
    // presentation-only input.
    simulation::SessionSimulationClock clock_;
    runtime::FrameCoordinator frame_coordinator_;
#if defined(GENOMES_HAS_RMLUI)
    [[nodiscard]] std::string resolve_document(foundation::SceneId scene) const;
    std::unique_ptr<platform::SdlFileDialogService> file_dialog_service_;
    std::unique_ptr<ui::rml::Runtime> rml_ui_;
    std::uint64_t rml_route_revision_{0};
#endif
};

} // namespace genomes::game
