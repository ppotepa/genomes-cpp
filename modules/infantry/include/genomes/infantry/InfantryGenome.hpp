#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/proc/SeedPath.hpp>

#include <cstdint>
#include <array>
#include <optional>

namespace genomes::infantry {

inline constexpr std::uint32_t InfantryGenomeVersion = 1;
inline constexpr std::uint32_t InfantryGeneratorVersion = 1;
inline constexpr std::uint32_t InfantryArtifactVersion = 1;

struct BodyGenes final {
    float frame{0.5F};
    float mass{0.5F};
    float musculature{0.5F};
    float adiposity{0.5F};
    float torso_leg_ratio{0.5F};
    float shoulder_width{0.5F};
    float chest_depth{0.5F};
    float hip_width{0.5F};
    float arm_length{0.5F};
    float leg_length{0.5F};
    float waist_width{0.5F};
    float limb_thickness{0.5F};
    float neck_thickness{0.5F};
    float head_scale{0.5F};
    float hand_scale{0.5F};
    float foot_scale{0.5F};
    float skin_tone{0.5F};

    [[nodiscard]] bool valid() const noexcept;
};

struct FaceGenes final {
    float head_width{0.5F};
    float head_depth{0.5F};
    float head_length{0.5F};
    float forehead_width{0.5F};
    float forehead_slope{0.5F};
    float temple_width{0.5F};
    float brow_ridge{0.5F};
    float eye_spacing{0.5F};
    float eye_size{0.5F};
    float nose_length{0.5F};
    float nose_width{0.5F};
    float mouth_width{0.5F};
    float jaw_width{0.5F};
    float hairline{0.5F};
    float jaw_length{0.5F};
    float jaw_angle{0.5F};
    float chin_width{0.5F};
    float chin_height{0.5F};
    float chin_projection{0.5F};
    float cheekbone_width{0.5F};
    float cheekbone_height{0.5F};
    float cheek_fullness{0.5F};
    float midface_projection{0.5F};
    float eye_width{0.5F};
    float eye_height{0.5F};
    float eye_roundness{0.5F};
    float eye_depth{0.5F};
    float eye_tilt{0.5F};
    float eye_vertical{0.5F};
    float eye_color{0.5F};
    float brow_height{0.5F};
    float brow_thickness{0.5F};
    float brow_tilt{0.5F};
    float brow_spacing{0.5F};
    float nose_projection{0.5F};
    float nose_bridge{0.5F};
    float nose_tip_width{0.5F};
    float nose_tip_rotation{0.5F};
    float nostril_width{0.5F};
    float upper_lip{0.5F};
    float lower_lip{0.5F};
    float mouth_height{0.5F};
    float ear_size{0.5F};
    float ear_angle{0.5F};
    float hair_color{0.5F};
    float hair_brightness{0.5F};
    float hair_style{0.5F};
    float hair_density{0.5F};
    float hair_thickness{0.5F};
    float hair_volume{0.5F};
    float temple_recession{0.5F};
    float widow_peak{0.5F};
    float resting_eye_openness{0.5F};
    float resting_brow{0.5F};
    float resting_mouth{0.5F};
    float eye_height_asymmetry{0.5F};
    float brow_height_asymmetry{0.5F};
    float mouth_corner_asymmetry{0.5F};
    float ear_asymmetry{0.5F};
    float blink_rate{0.5F};
    float blink_speed{0.5F};
    float gaze_restlessness{0.5F};
    float expression_scale{0.5F};
    float eye_expression{0.5F};
    float mouth_expression{0.5F};
    float brow_expression{0.5F};

    [[nodiscard]] bool valid() const noexcept;
};

struct GenomeOverrides final {
    std::optional<float> height;
    std::optional<float> shoulder_width;
    std::optional<float> chest_depth;
    std::optional<float> hip_width;
    std::optional<float> arm_length;
    std::optional<float> leg_length;
    std::optional<float> eye_spacing;
    std::optional<float> eye_size;
    std::optional<float> nose_length;
    std::optional<float> nose_width;
    std::optional<float> mouth_width;
    std::optional<float> jaw_width;
    std::optional<float> hairline;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] foundation::StableId hash() const noexcept;
};

struct InfantryGenome final {
    // The first fields retain the existing spawn-facing contract.  New
    // biological domains are appended so old aggregate initializers remain
    // source-compatible while phenotype generation uses named genes.
    float height{1.75F};
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
    [[nodiscard]] foundation::StableId identityHash() const noexcept;
};

} // namespace genomes::infantry
