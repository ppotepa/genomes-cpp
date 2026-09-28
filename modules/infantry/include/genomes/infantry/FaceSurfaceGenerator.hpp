#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/infantry/AppearanceArtifact.hpp>
#include <genomes/infantry/AppearanceMeshBuilder.hpp>
#include <genomes/infantry/FaceAnatomy.hpp>

namespace genomes::infantry {

class FaceSurfaceGenerator final {
public:
    [[nodiscard]] static foundation::Result<void, foundation::Error> build(
        AppearanceMeshBuilder&, const ResolvedAnatomy&, const FacePhenotype&,
        const SkeletonData&, const AppearanceOptions&, const geometry::Ring& neck_ring);
};

} // namespace genomes::infantry
