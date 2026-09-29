#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/proc/SeedPath.hpp>

#include <cstdint>
#include <array>
#include <optional>
#include <string_view>

namespace genomes::infantry {

inline constexpr std::uint32_t InfantryGenomeVersion = 1;
inline constexpr std::uint32_t InfantryGeneratorVersion = 1;
inline constexpr std::uint32_t InfantryArtifactVersion = 1;

// Exact JavaScript traversal order: root genes, body insertion order, then
// face insertion order. The numeric values are part of the fixture schema.
enum class GenomeGene : std::uint16_t {
    Height,
    Speed,
    BodyFrame,
    BodyMass,
    BodyMusculature,
    BodyAdiposity,
    BodyShoulderBreadth,
    BodyHipBreadth,
    BodyTorsoLegRatio,
    BodyArmLength,
    BodyLegLength,
    BodyChestDepth,
    BodyWaistWidth,
    BodyLimbThickness,
    BodyNeckThickness,
    BodyHeadScale,
    BodyHandScale,
    BodyFootScale,
    BodySkinTone,
    FaceHeadWidth,
    FaceHeadDepth,
    FaceHeadLength,
    FaceForeheadWidth,
    FaceForeheadSlope,
    FaceTempleWidth,
    FaceBrowRidge,
    FaceJawWidth,
    FaceJawLength,
    FaceJawAngle,
    FaceChinWidth,
    FaceChinHeight,
    FaceChinProjection,
    FaceCheekboneWidth,
    FaceCheekboneHeight,
    FaceCheekFullness,
    FaceMidfaceProjection,
    FaceEyeSpacing,
    FaceEyeWidth,
    FaceEyeHeight,
    FaceEyeRoundness,
    FaceEyeDepth,
    FaceEyeTilt,
    FaceEyeVertical,
    FaceEyeColor,
    FaceBrowHeight,
    FaceBrowThickness,
    FaceBrowTilt,
    FaceBrowSpacing,
    FaceNoseWidth,
    FaceNoseLength,
    FaceNoseProjection,
    FaceNoseBridge,
    FaceNoseTipWidth,
    FaceNoseTipRotation,
    FaceNostrilWidth,
    FaceMouthWidth,
    FaceUpperLip,
    FaceLowerLip,
    FaceMouthHeight,
    FaceEarSize,
    FaceEarAngle,
    FaceHairColor,
    FaceHairBrightness,
    FaceHairStyle,
    FaceHairDensity,
    FaceHairThickness,
    FaceHairVolume,
    FaceHairline,
    FaceTempleRecession,
    FaceWidowPeak,
    FaceRestingEyeOpenness,
    FaceRestingBrow,
    FaceRestingMouth,
    FaceEyeHeightAsymmetry,
    FaceBrowHeightAsymmetry,
    FaceMouthCornerAsymmetry,
    FaceEarAsymmetry,
    FaceBlinkRate,
    FaceBlinkSpeed,
    FaceGazeRestlessness,
    FaceExpressionScale,
    FaceEyeExpression,
    FaceMouthExpression,
    FaceBrowExpression,
    Count,
};

inline constexpr std::size_t GenomeGeneCount = static_cast<std::size_t>(GenomeGene::Count);

[[nodiscard]] std::string_view genomeGeneName(GenomeGene gene) noexcept;
[[nodiscard]] std::optional<GenomeGene> genomeGeneFromName(std::string_view name) noexcept;

struct BodyGenes final {
    double frame{0.5};
    double mass{0.5};
    double musculature{0.5};
    double adiposity{0.5};
    double torso_leg_ratio{0.5};
    double shoulder_width{0.5};
    double chest_depth{0.5};
    double hip_width{0.5};
    double arm_length{0.5};
    double leg_length{0.5};
    double waist_width{0.5};
    double limb_thickness{0.5};
    double neck_thickness{0.5};
    double head_scale{0.5};
    double hand_scale{0.5};
    double foot_scale{0.5};
    double skin_tone{0.5};

    [[nodiscard]] bool valid() const noexcept;
};

struct FaceGenes final {
    float head_width{0.5};
    float head_depth{0.5};
    float head_length{0.5};
    float forehead_width{0.5};
    float forehead_slope{0.5};
    float temple_width{0.5};
    float brow_ridge{0.5};
    float eye_spacing{0.5};
    float eye_size{0.5};
    float nose_length{0.5};
    float nose_width{0.5};
    float mouth_width{0.5};
    float jaw_width{0.5};
    float hairline{0.5};
    float jaw_length{0.5};
    float jaw_angle{0.5};
    float chin_width{0.5};
    float chin_height{0.5};
    float chin_projection{0.5};
    float cheekbone_width{0.5};
    float cheekbone_height{0.5};
    float cheek_fullness{0.5};
    float midface_projection{0.5};
    float eye_width{0.5};
    float eye_height{0.5};
    float eye_roundness{0.5};
    float eye_depth{0.5};
    float eye_tilt{0.5};
    float eye_vertical{0.5};
    float eye_color{0.5};
    float brow_height{0.5};
    float brow_thickness{0.5};
    float brow_tilt{0.5};
    float brow_spacing{0.5};
    float nose_projection{0.5};
    float nose_bridge{0.5};
    float nose_tip_width{0.5};
    float nose_tip_rotation{0.5};
    float nostril_width{0.5};
    float upper_lip{0.5};
    float lower_lip{0.5};
    float mouth_height{0.5};
    float ear_size{0.5};
    float ear_angle{0.5};
    float hair_color{0.5};
    float hair_brightness{0.5};
    float hair_style{0.5};
    float hair_density{0.5};
    float hair_thickness{0.5};
    float hair_volume{0.5};
    float temple_recession{0.5};
    float widow_peak{0.5};
    float resting_eye_openness{0.5};
    float resting_brow{0.5};
    float resting_mouth{0.5};
    float eye_height_asymmetry{0.5};
    float brow_height_asymmetry{0.5};
    float mouth_corner_asymmetry{0.5};
    float ear_asymmetry{0.5};
    float blink_rate{0.5};
    float blink_speed{0.5};
    float gaze_restlessness{0.5};
    float expression_scale{0.5};
    float eye_expression{0.5};
    float mouth_expression{0.5};
    float brow_expression{0.5};

    [[nodiscard]] bool valid() const noexcept;
};

struct GenomeOverrides final {
    std::array<std::optional<double>, GenomeGeneCount> genes{};

    [[nodiscard]] bool set(GenomeGene gene, double value) noexcept;
    [[nodiscard]] bool set(std::string_view name, double value) noexcept;
    [[nodiscard]] std::optional<double> get(GenomeGene gene) const noexcept;
    [[nodiscard]] std::optional<double> get(std::string_view name) const noexcept;

    // Compatibility aliases for the pre-parity API. They represent expressed
    // dimensions where documented, while `genes` always stores normalized JS
    // genes. Remove after downstream callers migrate to typed genes.
    std::optional<double> height;
    std::optional<double> shoulder_width;
    std::optional<double> chest_depth;
    std::optional<double> hip_width;
    std::optional<double> arm_length;
    std::optional<double> leg_length;
    std::optional<double> eye_spacing;
    std::optional<double> eye_size;
    std::optional<double> nose_length;
    std::optional<double> nose_width;
    std::optional<double> mouth_width;
    std::optional<double> jaw_width;
    std::optional<double> hairline;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] foundation::StableId hash() const noexcept;
};

struct InfantryGenome final {
    // The first fields retain the existing spawn-facing contract.  New
    // biological domains are appended so old aggregate initializers remain
    // source-compatible while phenotype generation uses named genes.
    float height{1.75F};
    double reference_height{1.75};
    float move_speed{3.0F};
    float perception_radius{60.0F};
    float attack_range{35.0F};
    float max_health{100.0F};
    std::uint32_t appearance_variant{0};
    std::uint32_t version{InfantryGenomeVersion};
    proc::Seed seed{0};
    float diversity_scale{1.0F};
    BodyGenes body{};
    FaceGenes face{};

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] static foundation::Result<InfantryGenome, foundation::Error> generate(
        proc::Seed seed, float diversity_scale = 1.0F);
    [[nodiscard]] foundation::Result<InfantryGenome, foundation::Error> applyVariation(
        float scale) const;
    [[nodiscard]] foundation::Result<InfantryGenome, foundation::Error> withOverrides(
        const GenomeOverrides&) const;
    [[nodiscard]] double geneValue(GenomeGene gene) const noexcept;
    [[nodiscard]] foundation::StableId identityHash() const noexcept;
};

} // namespace genomes::infantry
