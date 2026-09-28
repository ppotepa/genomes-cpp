#pragma once

#include <genomes/infantry/BodyPhenotype.hpp>
#include <genomes/infantry/FacePhenotype.hpp>
#include <genomes/infantry/InfantryGenome.hpp>

#include <cstdint>

namespace genomes::infantry {

struct PhenotypeDiagnostics final {
    bool height_clamped{false};
    bool shoulder_clamped{false};
    bool face_spacing_adjusted{false};
    bool landmark_order_adjusted{false};
};

struct PhenotypeArtifact final {
    std::uint32_t version{1};
    foundation::StableId cache_key{0};
    InfantryGenome requested{};
    BodyPhenotype body{};
    FacePhenotype face{};
    PhenotypeDiagnostics diagnostics{};

    [[nodiscard]] bool valid() const noexcept;
};

class PhenotypeResolver final {
public:
    [[nodiscard]] static foundation::Result<PhenotypeArtifact, foundation::Error> resolve(
        const InfantryGenome&, const GenomeOverrides& = {});
};

} // namespace genomes::infantry
