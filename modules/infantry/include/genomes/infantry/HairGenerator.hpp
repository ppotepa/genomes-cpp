#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/infantry/AppearanceArtifact.hpp>
#include <genomes/infantry/AppearanceMeshBuilder.hpp>
#include <genomes/infantry/FaceAnatomy.hpp>

namespace genomes::infantry {

class HairGenerator final {
public:
    [[nodiscard]] static foundation::Result<AppearanceMesh, foundation::Error> build(
        const ResolvedAnatomy&, const FacePhenotype&, const SkeletonData&,
        const AppearanceOptions&);
};

} // namespace genomes::infantry
