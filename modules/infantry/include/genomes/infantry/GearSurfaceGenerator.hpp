#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/infantry/AppearanceArtifact.hpp>
#include <genomes/infantry/AppearanceMeshBuilder.hpp>
#include <genomes/infantry/EquipmentFit.hpp>

namespace genomes::infantry {

class GearSurfaceGenerator final {
public:
    [[nodiscard]] static foundation::Result<AppearanceMesh, foundation::Error> build(
        const GearArtifact&);
};

} // namespace genomes::infantry
