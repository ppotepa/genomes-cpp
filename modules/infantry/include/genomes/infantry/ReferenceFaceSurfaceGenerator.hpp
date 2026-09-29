#pragma once

#include <genomes/infantry/EquipmentFit.hpp>
#include <genomes/infantry/ReferenceBodySurfaceGenerator.hpp>

namespace genomes::infantry {

class ReferenceFaceSurfaceGenerator final {
public:
    [[nodiscard]] static ReferenceJacketBuild appendShell(
        ReferenceSurfaceBuilder&, const EquipmentFit&, foundation::Color skin,
        std::uint32_t detail_level);
};

} // namespace genomes::infantry
