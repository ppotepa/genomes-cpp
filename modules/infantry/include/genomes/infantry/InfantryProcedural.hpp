#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/infantry/InfantryModelCompiler.hpp>
#include <genomes/proc/GeneratorRegistry.hpp>

namespace genomes::infantry {

[[nodiscard]] foundation::Result<void, foundation::Error> registerInfantryGenerator(
    proc::GeneratorRegistry::Builder& builder);

} // namespace genomes::infantry
