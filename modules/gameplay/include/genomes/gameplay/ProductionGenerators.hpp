#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/proc/GeneratorRegistry.hpp>

namespace genomes::gameplay {

// Frozen registry shared by product scenes and tooling.  Domain generators
// remain owned by their modules; this composition point only defines the
// production set and its stable registration order.
[[nodiscard]] foundation::Result<proc::GeneratorRegistry, foundation::Error>
makeProductionGeneratorRegistry();

} // namespace genomes::gameplay
