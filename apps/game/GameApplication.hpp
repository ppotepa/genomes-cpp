#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/jobs/JobSystem.hpp>
#include <genomes/render/PresentationSnapshot.hpp>
#include <genomes/runtime/SceneDirector.hpp>
#include <genomes/simulation/FixedStepClock.hpp>
#include <genomes/ui/UiDocument.hpp>

#include <memory>

namespace genomes::platform {
class SdlPlatform;
}
namespace genomes::render {
class DiligentBackend;
class DiligentSceneRenderer;
}

namespace genomes::game {

class GameApplication final {
public:
    [[nodiscard]] static foundation::Result<std::unique_ptr<GameApplication>, foundation::Error>
    create();
    ~GameApplication();

    int run(int argc, char** argv);

private:
    GameApplication(std::unique_ptr<platform::SdlPlatform> platform,
                    std::unique_ptr<render::DiligentBackend> backend);

    std::unique_ptr<platform::SdlPlatform> platform_;
    std::unique_ptr<render::DiligentBackend> backend_;
    std::unique_ptr<render::DiligentSceneRenderer> renderer_;
    jobs::JobSystem jobs_;
    ui::UiDocument ui_;
    render::PresentationSnapshot presentation_;
    runtime::SceneDirector director_;
    simulation::FixedStepClock clock_;
};

} // namespace genomes::game
