#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/proc/GeneratorRegistry.hpp>

namespace genomes::hydrology {

[[nodiscard]] foundation::Result<void, foundation::Error> registerHydrologyGenerator(
    proc::GeneratorRegistry::Builder& builder);

} // namespace genomes::hydrology
