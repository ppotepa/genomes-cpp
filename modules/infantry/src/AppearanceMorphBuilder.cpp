#include <genomes/infantry/AppearanceMorphBuilder.hpp>
#include <genomes/infantry/InfantryMaterials.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <vector>

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
        const float nearest_eye_x = position.x < 0.0F ? -face.eye_spacing : face.eye_spacing;
        const float local_x = std::clamp(
            (position.x - nearest_eye_x) / std::max(face.eye_radius, 1.0e-4F), -1.0F, 1.0F);
        // Upper and lower lid converge on the same slightly arced seal. This
        // preserves topology while giving opposite Y deltas on opposite sides.
        const float closure_y = face.eye_y +
            face.eye_radius * 0.035F * (1.0F - local_x * local_x);
        close.position_deltas[index] = {
            0.0F, (closure_y - position.y) * eyelid, 0.00010F * eyelid};
        // Arc is a secondary curvature delta around the half-closed position,
        // not another global upward translation.
        const float lid_side = position.y >= face.eye_y ? 1.0F : -1.0F;
        arc.position_deltas[index] = {
            0.0F, lid_side * face.eye_radius * 0.08F *
                (1.0F - local_x * local_x) * eyelid,
            0.00005F * eyelid};
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

    const auto& indices = artifact.body.indices;
    const auto& vertices = artifact.body.vertices;
    for (MorphTarget& morph : artifact.morphs) {
        std::vector<bool> moved(vertices.size(), false);
        std::vector<bool> affected(vertices.size(), false);
        for (std::size_t index = 0; index < vertices.size(); ++index) {
            const auto& delta = morph.position_deltas[index];
            moved[index] = delta.x != 0.0F || delta.y != 0.0F || delta.z != 0.0F;
        }
        for (std::size_t offset = 0; offset < indices.size(); offset += 3U) {
            const auto a = indices[offset], b = indices[offset + 1U], c = indices[offset + 2U];
            if (moved[a] || moved[b] || moved[c]) affected[a] = affected[b] = affected[c] = true;
        }
        std::vector<foundation::Vec3> accumulated(vertices.size());
        for (std::size_t offset = 0; offset < indices.size(); offset += 3U) {
            const auto a = indices[offset], b = indices[offset + 1U], c = indices[offset + 2U];
            if (!affected[a] && !affected[b] && !affected[c]) continue;
            const auto position = [&](std::uint32_t index) {
                return foundation::Vec3{
                    vertices[index].position.x + morph.position_deltas[index].x,
                    vertices[index].position.y + morph.position_deltas[index].y,
                    vertices[index].position.z + morph.position_deltas[index].z};
            };
            const auto pa = position(a), pb = position(b), pc = position(c);
            const float cbx = pc.x - pb.x, cby = pc.y - pb.y, cbz = pc.z - pb.z;
            const float abx = pa.x - pb.x, aby = pa.y - pb.y, abz = pa.z - pb.z;
            const foundation::Vec3 normal{cby * abz - cbz * aby,
                                          cbz * abx - cbx * abz,
                                          cbx * aby - cby * abx};
            for (const auto index : {a, b, c}) {
                accumulated[index].x += normal.x;
                accumulated[index].y += normal.y;
                accumulated[index].z += normal.z;
            }
        }
        for (std::size_t index = 0; index < vertices.size(); ++index) {
            if (!affected[index]) continue;
            auto normal = accumulated[index];
            const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y +
                                           normal.z * normal.z);
            if (length > 0.0F) {
                normal.x /= length;
                normal.y /= length;
                normal.z /= length;
                morph.normal_deltas[index] = {
                    normal.x - vertices[index].normal.x,
                    normal.y - vertices[index].normal.y,
                    normal.z - vertices[index].normal.z};
            }
        }

        foundation::Vec3 morph_min{};
        foundation::Vec3 morph_max{};
        bool first = true;
        for (const auto& delta : morph.position_deltas) {
            if (first) {
                morph_min = morph_max = delta;
                first = false;
            } else {
                morph_min.x = std::min(morph_min.x, delta.x);
                morph_min.y = std::min(morph_min.y, delta.y);
                morph_min.z = std::min(morph_min.z, delta.z);
                morph_max.x = std::max(morph_max.x, delta.x);
                morph_max.y = std::max(morph_max.y, delta.y);
                morph_max.z = std::max(morph_max.z, delta.z);
            }
        }
        artifact.body.minimum.x += std::min(0.0F, morph_min.x);
        artifact.body.minimum.y += std::min(0.0F, morph_min.y);
        artifact.body.minimum.z += std::min(0.0F, morph_min.z);
        artifact.body.maximum.x += std::max(0.0F, morph_max.x);
        artifact.body.maximum.y += std::max(0.0F, morph_max.y);
        artifact.body.maximum.z += std::max(0.0F, morph_max.z);
    }
    artifact.body.sphere_center={(artifact.body.minimum.x+artifact.body.maximum.x)*.5F,
        (artifact.body.minimum.y+artifact.body.maximum.y)*.5F,
        (artifact.body.minimum.z+artifact.body.maximum.z)*.5F};
    float radius_squared=0;
    const auto include=[&](foundation::Vec3 point){const float x=point.x-artifact.body.sphere_center.x,
        y=point.y-artifact.body.sphere_center.y,z=point.z-artifact.body.sphere_center.z;
        radius_squared=std::max(radius_squared,x*x+y*y+z*z);};
    for(const auto& vertex:artifact.body.vertices)include(vertex.position);
    for(const auto& morph:artifact.morphs)for(std::size_t index=0;index<artifact.body.vertices.size();++index)
        include({artifact.body.vertices[index].position.x+morph.position_deltas[index].x,
                 artifact.body.vertices[index].position.y+morph.position_deltas[index].y,
                 artifact.body.vertices[index].position.z+morph.position_deltas[index].z});
    artifact.body.sphere_radius=std::sqrt(radius_squared);
}

} // namespace genomes::infantry
