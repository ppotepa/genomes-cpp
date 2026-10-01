#pragma once

#include <genomes/runtime/Scene.hpp>
#include <genomes/game_scenes/WorldConfig.hpp>

#include <cstdint>
#include <memory>
#include <utility>

namespace genomes::application {

// Product/application routing is deliberately not part of the neutral
// runtime. The payload is typed here and crosses the lifecycle boundary as a
// SceneCommand owned by the application composition root.
enum class ApplicationCommandKind : std::uint8_t {
    StartScenario,
    OpenWorldConfig,
    OpenUnitLab,
    OpenBuildingLab,
    OpenWorldLab,
    OpenSettings,
    OpenPause,
    ReturnToMainMenu,
    Quit
};

struct ApplicationCommand final : runtime::SceneCommand {
    ApplicationCommand(ApplicationCommandKind command_kind,
                       WorldGenerationConfig config = {})
        : kind(command_kind), world_config(std::move(config)) {}

    ApplicationCommandKind kind{ApplicationCommandKind::OpenSettings};
    WorldGenerationConfig world_config{};
};

[[nodiscard]] inline runtime::SceneCommandPtr makeApplicationCommand(
    ApplicationCommandKind kind,
    WorldGenerationConfig config = {}) {
    return std::make_unique<ApplicationCommand>(kind, std::move(config));
}

inline void enqueueApplicationCommand(runtime::SceneContext& context,
                                      ApplicationCommandKind kind,
                                      WorldGenerationConfig config = {}) {
    context.commands.push(makeApplicationCommand(kind, std::move(config)));
}

} // namespace genomes::application
