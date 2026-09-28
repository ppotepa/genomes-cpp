#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/render/PresentationSnapshot.hpp>
#include <genomes/runtime/SceneDirector.hpp>
#include <genomes/simulation/FixedStepClock.hpp>
#include <genomes/ui/UiRuntime.hpp>
#include <genomes/ui/UiNativePluginManager.hpp>

#if defined(GENOMES_HAS_RMLUI)
#include <RmlUiRuntime.hpp>
#endif

#include <memory>
#include <string>

namespace genomes::platform {
class SdlPlatform;
}
namespace genomes::render {
class DiligentBackend;
class DiligentSceneRenderer;
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
                    std::unique_ptr<render::DiligentBackend> backend);

    std::unique_ptr<platform::SdlPlatform> platform_;
    std::unique_ptr<render::DiligentBackend> backend_;
    std::unique_ptr<render::DiligentSceneRenderer> renderer_;
    jobs::JobSystem jobs_;
    ui::UiRuntime ui_;
    ui::UiContentRegistry content_;
    ui::UiNativePluginManager plugins_;
    render::PresentationSnapshot presentation_;
    runtime::SceneDirector director_;
    simulation::FixedStepClock clock_;
#if defined(GENOMES_HAS_RMLUI)
    [[nodiscard]] std::string document_for_scene(foundation::SceneId scene) const;
    std::unique_ptr<ui::rml::Runtime> rml_ui_;
    std::uint64_t rml_route_revision_{0};
#endif
};

} // namespace genomes::game
