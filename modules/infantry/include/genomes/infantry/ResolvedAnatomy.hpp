#pragma once

#include <genomes/foundation/Types.hpp>

#include <cstdint>
#include <vector>

namespace genomes::infantry {

struct BodyCrossSection final {
    float y{0.0F};
    float half_width{0.0F};
    float half_depth{0.0F};
    float front_offset{0.0F};
    double reference_y{0.0};
    double reference_half_width{0.0};
    double reference_half_depth{0.0};
};

struct HeadCrossSection final {
    float y{0.0F};
    float half_width{0.0F};
    float half_depth{0.0F};
    float center_z{0.0F};
};

struct FaceLandmarks final {
    foundation::Vec3 left_eye{};
    foundation::Vec3 right_eye{};
    foundation::Vec3 nose_bridge{};
    foundation::Vec3 nose_tip{};
    foundation::Vec3 left_mouth_corner{};
    foundation::Vec3 right_mouth_corner{};
    foundation::Vec3 chin{};
    foundation::Vec3 left_ear{};
    foundation::Vec3 right_ear{};
};

struct ScalpProfile final {
    float base_y{0.0F};
    float hairline_y{0.0F};
    float crown_y{0.0F};
    float temple_recession{0.0F};
    float widow_peak{0.0F};
};

struct ResolvedAnatomy final {
    std::uint32_t version{1};
    bool reference_profile{false};
    float height{0.0F};
    std::vector<BodyCrossSection> torso_sections;
    std::vector<HeadCrossSection> head_sections;
    FaceLandmarks face{};
    ScalpProfile scalp{};
};

} // namespace genomes::infantry
