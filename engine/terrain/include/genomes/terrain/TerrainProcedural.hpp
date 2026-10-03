#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/proc/GeneratorRegistry.hpp>

namespace genomes::terrain {

[[nodiscard]] foundation::Result<void, foundation::Error> registerTerrainGenerator(
    proc::GeneratorRegistry::Builder& builder);

} // namespace genomes::terrain
