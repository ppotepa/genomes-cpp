#pragma once

#include <genomes/infantry/BodyPhenotype.hpp>
#include <genomes/infantry/FacePhenotype.hpp>
#include <genomes/infantry/InfantryGenome.hpp>

#include <cstdint>
#include <vector>

namespace genomes::infantry {

enum class AnatomyParameter : std::uint8_t {
    Height,
    ShoulderWidth,
    EyeSpacing,
    LandmarkOrder,
};

enum class AnatomyAdjustmentReason : std::uint8_t {
    ClampedToDomain,
    ConstrainedToFaceWidth,
    RestoredLandmarkOrdering,
};

struct AnatomyAdjustment final {
    AnatomyParameter parameter{AnatomyParameter::Height};
    float requested{0.0F};
    float resolved{0.0F};
    AnatomyAdjustmentReason reason{AnatomyAdjustmentReason::ClampedToDomain};
};

struct PhenotypeDiagnostics final {
    bool height_clamped{false};
    bool shoulder_clamped{false};
    bool face_spacing_adjusted{false};
    bool landmark_order_adjusted{false};
    std::vector<AnatomyAdjustment> adjustments;
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
