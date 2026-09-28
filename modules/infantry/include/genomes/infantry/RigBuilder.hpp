#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/infantry/BodyPhenotype.hpp>
#include <genomes/infantry/FacePhenotype.hpp>
#include <genomes/infantry/SkeletonData.hpp>

namespace genomes::infantry {

class RigBuilder final {
public:
    [[nodiscard]] static foundation::Result<SkeletonData, foundation::Error> build(
        const BodyPhenotype&, const FacePhenotype&);
};

} // namespace genomes::infantry
