#include <genomes/infantry/AppearanceMorphBuilder.hpp>
#include <genomes/infantry/InfantryMaterials.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace genomes::infantry {

void AppearanceMorphBuilder::initialize(AppearanceArtifact& artifact) {
    constexpr std::array<std::string_view, 4U> names{
        "eyelidsClose", "eyelidsArc", "neckFlex", "handsRelax"};
    for (std::size_t index = 0U; index < names.size(); ++index) {
        artifact.morphs[index].name = names[index];
        artifact.morphs[index].position_deltas.assign(artifact.body.vertices.size(), {});
        artifact.morphs[index].normal_deltas.assign(artifact.body.vertices.size(), {});
    }
}

void AppearanceMorphBuilder::build(AppearanceArtifact& artifact, const FacePhenotype& face) {
    MorphTarget& close = artifact.morphs[0];
    MorphTarget& arc = artifact.morphs[1];
    MorphTarget& neck = artifact.morphs[2];
    MorphTarget& hands = artifact.morphs[3];
    for (std::size_t index = 0U; index < artifact.body.vertices.size(); ++index) {
        const AppearanceVertex& vertex = artifact.body.vertices[index];
        const foundation::Vec3 position = vertex.position;
        const float radius2 = std::max(1.0e-5F, face.eye_radius * face.eye_radius);
        const float eye_left = std::exp(-((position.x + face.eye_spacing) *
                                          (position.x + face.eye_spacing)) / radius2);
        const float eye_right = std::exp(-((position.x - face.eye_spacing) *
                                           (position.x - face.eye_spacing)) / radius2);
        const float eye_y = std::exp(-((position.y - face.eye_y) *
                                       (position.y - face.eye_y)) / radius2);
        const bool eyelid_surface = vertex.material_region == static_cast<std::uint16_t>(
            AppearanceMaterialRegion::Eyelid);
        const float eyelid = eyelid_surface
            ? std::clamp((eye_left + eye_right) * eye_y, 0.0F, 1.0F)
            : 0.0F;
        close.position_deltas[index] = {0.0F, -0.006F * eyelid, -0.002F * eyelid};
        arc.position_deltas[index] = {0.0F, 0.003F * eyelid, 0.001F * eyelid};
        const float neck_factor = vertex.material_region == static_cast<std::uint16_t>(
            AppearanceMaterialRegion::Skin)
            ? std::clamp((face.mouth_y - position.y) / 0.14F, 0.0F, 1.0F)
            : 0.0F;
        neck.position_deltas[index] = {0.0F, 0.002F * neck_factor,
                                       -0.001F * neck_factor};
        const float hand_factor = vertex.material_region == static_cast<std::uint16_t>(
            AppearanceMaterialRegion::SkinHand) ? 1.0F : 0.0F;
        hands.position_deltas[index] = {0.0F, -0.0005F * hand_factor, 0.0F};
    }
}

} // namespace genomes::infantry
