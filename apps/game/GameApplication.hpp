#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/render/PresentationSnapshot.hpp>
#include <genomes/runtime/SceneDirector.hpp>
#include <genomes/game_scenes/BuiltinScenes.hpp>
#include <genomes/simulation/FixedStepClock.hpp>
#include <genomes/ui/UiRuntime.hpp>
#include <genomes/ui/UiNativePluginManager.hpp>
#if GENOMES_HAS_INFANTRY
#include <genomes/combat/TacticalAI.hpp>
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
                    std::unique_ptr<render::RenderBackend> backend_owner = {}
#if GENOMES_HAS_INFANTRY
                    , combat::TacticalAIProfile tactical_ai_profile = {}
#endif
                    );

    [[nodiscard]] foundation::Result<void, foundation::Error> resize_renderer(
        std::uint32_t width, std::uint32_t height);

    std::unique_ptr<platform::SdlPlatform> platform_;
    // Diligent owns the GPU backend for the lifetime of the renderer. Camera
    // control and input ordering belong to SceneDirector, not the renderer.
    std::unique_ptr<render::RenderBackend> backend_owner_;
    std::unique_ptr<render::IRenderer> renderer_;
    jobs::JobSystem jobs_;
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
    simulation::FixedStepClock clock_;
#if defined(GENOMES_HAS_RMLUI)
    [[nodiscard]] std::string resolve_document(foundation::SceneId scene) const;
    std::unique_ptr<platform::SdlFileDialogService> file_dialog_service_;
    std::unique_ptr<ui::rml::Runtime> rml_ui_;
    std::uint64_t rml_route_revision_{0};
#endif
};

} // namespace genomes::game
