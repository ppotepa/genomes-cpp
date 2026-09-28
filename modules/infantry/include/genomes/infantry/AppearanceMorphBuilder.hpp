#pragma once

#include <genomes/infantry/AppearanceArtifact.hpp>

namespace genomes::infantry {

class AppearanceMorphBuilder final {
public:
    static void initialize(AppearanceArtifact& artifact);
    static void build(AppearanceArtifact& artifact, const FacePhenotype& face);
};

} // namespace genomes::infantry
