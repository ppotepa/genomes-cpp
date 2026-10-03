#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/buildings/BuildingModel.hpp>
#include <genomes/buildings/BuildingProfile.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/proc/GeneratorRegistry.hpp>

namespace genomes::buildings {

struct BuildingSiteGenerationRequest final {
    world::BuildingSiteRequest site{};
    BuildingSiteGenerationProfile profile{};
};

[[nodiscard]] foundation::Result<void, foundation::Error> registerBuildingGenerator(
    proc::GeneratorRegistry::Builder& builder);

} // namespace genomes::buildings
