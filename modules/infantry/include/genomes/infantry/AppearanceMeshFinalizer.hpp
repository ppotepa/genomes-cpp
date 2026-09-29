#pragma once
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/infantry/AppearanceArtifact.hpp>

namespace genomes::infantry {

[[nodiscard]] foundation::Result<void, foundation::Error>
finalizeAppearanceMesh(AppearanceMesh& mesh);

} // namespace genomes::infantry
