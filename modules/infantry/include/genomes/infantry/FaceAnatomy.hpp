#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/infantry/PhenotypeResolver.hpp>
#include <genomes/infantry/ResolvedAnatomy.hpp>

namespace genomes::infantry {

class FaceAnatomyEvaluator final {
public:
    [[nodiscard]] static foundation::Result<ResolvedAnatomy, foundation::Error> resolve(
        const PhenotypeArtifact& phenotype);

    [[nodiscard]] static HeadCrossSection sectionAt(const ResolvedAnatomy& anatomy,
                                                    float y) noexcept;

    [[nodiscard]] static float hairlineY(const ResolvedAnatomy& anatomy,
                                         float azimuth) noexcept;

    [[nodiscard]] static foundation::Vec3 scalpPoint(const ResolvedAnatomy& anatomy,
                                                     float normalized_height,
                                                     float azimuth) noexcept;
};

} // namespace genomes::infantry
