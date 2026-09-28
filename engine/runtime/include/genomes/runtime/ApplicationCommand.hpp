#pragma once

#include <genomes/runtime/WorldConfig.hpp>

#include <cstdint>

namespace genomes::runtime {

enum class ApplicationCommandKind {
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

struct ApplicationCommand {
    ApplicationCommandKind kind{ApplicationCommandKind::OpenSettings};
    WorldGenerationConfig world_config{};
};

} // namespace genomes::runtime
