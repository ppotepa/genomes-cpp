#include <genomes/infantry/AppearanceArtifact.hpp>

#include <genomes/proc/RandomStream.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

namespace genomes::infantry {

namespace {

using foundation::Vec2;
using foundation::Vec3;

[[nodiscard]] Vec3 add(Vec3 left, Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] Vec3 subtract(Vec3 left, Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] Vec3 multiply(Vec3 value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] float dot(Vec3 left, Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] float length(Vec3 value) noexcept { return std::sqrt(dot(value, value)); }

[[nodiscard]] Vec3 normalized(Vec3 value, Vec3 fallback = {0.0F, 1.0F, 0.0F}) noexcept {
    const float magnitude = length(value);
    return magnitude > 1.0e-6F && std::isfinite(magnitude) ? multiply(value, 1.0F / magnitude)
                                                            : fallback;
}

[[nodiscard]] bool finite(Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool finite(foundation::Color value) noexcept {
    return std::isfinite(value.r) && std::isfinite(value.g) && std::isfinite(value.b) &&
           std::isfinite(value.a);
}

[[nodiscard]] foundation::Color shade(foundation::Color color, float factor) noexcept {
    return {std::clamp(color.r * factor, 0.0F, 1.0F),
            std::clamp(color.g * factor, 0.0F, 1.0F),
            std::clamp(color.b * factor, 0.0F, 1.0F), color.a};
}

[[nodiscard]] std::uint16_t indexOf(const SkeletonData& skeleton, BoneId id) noexcept {
    const BoneRecord* bone = skeleton.find(id);
    return bone == nullptr ? kInvalidBoneIndex : static_cast<std::uint16_t>(boneIndex(id));
}

struct MeshBuilder final {
    AppearanceMesh mesh;

    [[nodiscard]] std::uint32_t vertex(Vec3 position,
                                       Vec3 normal,
                                       Vec2 uv,
                                       foundation::Color color,
                                       std::array<SkinInfluence, 4U> influences,
                                       std::uint8_t influence_count,
                                       std::uint16_t material_region) {
        const auto index = static_cast<std::uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({position, normalized(normal), uv, color, influences,
                                 influence_count, material_region});
        if (mesh.vertices.size() == 1U) {
            mesh.minimum = position;
            mesh.maximum = position;
        } else {
            mesh.minimum.x = std::min(mesh.minimum.x, position.x);
            mesh.minimum.y = std::min(mesh.minimum.y, position.y);
            mesh.minimum.z = std::min(mesh.minimum.z, position.z);
            mesh.maximum.x = std::max(mesh.maximum.x, position.x);
            mesh.maximum.y = std::max(mesh.maximum.y, position.y);
            mesh.maximum.z = std::max(mesh.maximum.z, position.z);
        }
        return index;
    }

    void triangle(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        mesh.indices.insert(mesh.indices.end(), {a, b, c});
    }
};

[[nodiscard]] std::array<SkinInfluence, 4U> weights(std::initializer_list<SkinInfluence> values,
                                                     std::uint8_t& count) {
    std::array<SkinInfluence, 4U> result{};
    count = 0U;
    float total = 0.0F;
    for (const SkinInfluence value : values) {
        if (count >= result.size() || value.bone_index == kInvalidBoneIndex ||
            !(value.weight > 0.0F)) {
            continue;
        }
        result[count++] = value;
        total += value.weight;
    }
    if (count == 0U) {
        result[0] = {0U, 1.0F};
        count = 1U;
        return result;
    }
    for (std::size_t index = 0U; index < count; ++index) {
        result[index].weight /= total;
    }
    return result;
}

[[nodiscard]] std::array<SkinInfluence, 4U> torsoWeights(const BodyPhenotype& body,
                                                          const SkeletonData& skeleton,
                                                          float y,
                                                          std::uint8_t& count) {
    const float pelvis = body.pelvis.y;
    const float chest = body.chest.y;
    const float neck = body.chest.y + (body.head.y - body.chest.y) * 0.63F;
    if (y <= pelvis) {
        return weights({{indexOf(skeleton, BoneId::Hips), 1.0F}}, count);
    }
    if (y < chest) {
        const float t = std::clamp((y - pelvis) / std::max(0.001F, chest - pelvis), 0.0F, 1.0F);
        return weights({{indexOf(skeleton, BoneId::SpineLower), (1.0F - t) * 0.65F},
                        {indexOf(skeleton, BoneId::SpineUpper), (1.0F - t) * 0.35F},
                        {indexOf(skeleton, BoneId::Chest), t}},
                       count);
    }
    if (y < neck) {
        const float t = std::clamp((y - chest) / std::max(0.001F, neck - chest), 0.0F, 1.0F);
        return weights({{indexOf(skeleton, BoneId::Chest), 1.0F - t},
                        {indexOf(skeleton, BoneId::Neck), t}},
                       count);
    }
    return weights({{indexOf(skeleton, BoneId::Neck), 1.0F}}, count);
}

void appendRing(MeshBuilder& builder,
                Vec3 center,
                float radius_x,
                float radius_z,
                std::size_t segments,
                foundation::Color color,
                std::uint16_t material_region,
                const std::array<SkinInfluence, 4U>& influences,
                std::uint8_t influence_count,
                std::vector<std::uint32_t>& output) {
    output.clear();
    output.reserve(segments);
    for (std::size_t index = 0U; index < segments; ++index) {
        const float angle = 6.2831853071795864769F * static_cast<float>(index) /
                            static_cast<float>(segments);
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        output.push_back(builder.vertex({center.x + radius_x * cosine, center.y,
                                         center.z + radius_z * sine},
                                        {cosine, 0.0F, sine},
                                        {static_cast<float>(index) / static_cast<float>(segments), 0.0F},
                                        color, influences, influence_count, material_region));
    }
}

void bridge(MeshBuilder& builder,
            const std::vector<std::uint32_t>& first,
            const std::vector<std::uint32_t>& second) {
    const std::size_t count = std::min(first.size(), second.size());
    for (std::size_t index = 0U; index < count; ++index) {
        const std::size_t next = (index + 1U) % count;
        builder.triangle(first[index], second[index], first[next]);
        builder.triangle(first[next], second[index], second[next]);
    }
}

void cap(MeshBuilder& builder,
         const std::vector<std::uint32_t>& loop,
         Vec3 center,
         Vec3 normal,
         foundation::Color color,
         std::uint16_t material_region,
         const std::array<SkinInfluence, 4U>& influences,
         std::uint8_t influence_count) {
    if (loop.size() < 3U) {
        return;
    }
    const std::uint32_t center_index = builder.vertex(
        center, normal, {0.5F, 0.5F}, color, influences, influence_count, material_region);
    for (std::size_t index = 0U; index < loop.size(); ++index) {
        const std::size_t next = (index + 1U) % loop.size();
        builder.triangle(center_index, loop[index], loop[next]);
    }
}

void appendLimb(MeshBuilder& builder,
                Vec3 start,
                Vec3 end,
                float radius_start,
                float radius_end,
                std::size_t segments,
                foundation::Color color,
                std::uint16_t material_region,
                const std::array<SkinInfluence, 4U>& start_weights,
                std::uint8_t start_count,
                const std::array<SkinInfluence, 4U>& end_weights,
                std::uint8_t end_count) {
    const Vec3 direction = normalized(subtract(end, start), {0.0F, 1.0F, 0.0F});
    const Vec3 side = normalized({-direction.z, 0.0F, direction.x}, {1.0F, 0.0F, 0.0F});
    const Vec3 up = normalized({direction.y * side.z - direction.z * side.y,
                                direction.z * side.x - direction.x * side.z,
                                direction.x * side.y - direction.y * side.x},
                               {0.0F, 0.0F, 1.0F});
    std::vector<std::uint32_t> first;
    std::vector<std::uint32_t> second;
    first.reserve(segments);
    second.reserve(segments);
    for (std::size_t index = 0U; index < segments; ++index) {
        const float angle = 6.2831853071795864769F * static_cast<float>(index) /
                            static_cast<float>(segments);
        const Vec3 radial = add(multiply(side, std::cos(angle)), multiply(up, std::sin(angle)));
        first.push_back(builder.vertex(add(start, multiply(radial, radius_start)), radial,
                                       {static_cast<float>(index) / static_cast<float>(segments), 0.0F},
                                       color, start_weights, start_count, material_region));
        second.push_back(builder.vertex(add(end, multiply(radial, radius_end)), radial,
                                        {static_cast<float>(index) / static_cast<float>(segments), 1.0F},
                                        color, end_weights, end_count, material_region));
    }
    bridge(builder, first, second);
}

void appendEllipsoid(MeshBuilder& builder,
                     Vec3 center,
                     Vec3 radii,
                     std::size_t segments,
                     std::size_t rows,
                     foundation::Color color,
                     std::uint16_t material_region,
                     const std::array<SkinInfluence, 4U>& influences,
                     std::uint8_t influence_count) {
    std::vector<std::uint32_t> previous;
    for (std::size_t row = 0U; row <= rows; ++row) {
        const float phi = -1.57079632679489661923F +
                          3.14159265358979323846F * static_cast<float>(row) /
                              static_cast<float>(rows);
        const float cos_phi = std::cos(phi);
        const float sin_phi = std::sin(phi);
        std::vector<std::uint32_t> ring;
        ring.reserve(segments);
        for (std::size_t index = 0U; index < segments; ++index) {
            const float angle = 6.2831853071795864769F * static_cast<float>(index) /
                                static_cast<float>(segments);
            const float cos_angle = std::cos(angle);
            const float sin_angle = std::sin(angle);
            ring.push_back(builder.vertex(
                {center.x + radii.x * cos_phi * cos_angle,
                 center.y + radii.y * sin_phi,
                 center.z + radii.z * cos_phi * sin_angle},
                {cos_phi * cos_angle, sin_phi, cos_phi * sin_angle},
                {static_cast<float>(index) / static_cast<float>(segments),
                 static_cast<float>(row) / static_cast<float>(rows)},
                color, influences, influence_count, material_region));
        }
        if (!previous.empty()) {
            bridge(builder, previous, ring);
        }
        previous = std::move(ring);
    }
}

void appendOpening(MeshBuilder& builder,
                   float center_x,
                   float center_y,
                   float radius_x,
                   float radius_y,
                   float depth,
                   std::size_t segments,
                   foundation::Color color,
                   std::uint16_t material_region,
                   const std::array<SkinInfluence, 4U>& influences,
                   std::uint8_t influence_count) {
    std::vector<std::uint32_t> outer;
    std::vector<std::uint32_t> inner;
    outer.reserve(segments);
    inner.reserve(segments);
    for (std::size_t index = 0U; index < segments; ++index) {
        const float angle = 6.2831853071795864769F * static_cast<float>(index) /
                            static_cast<float>(segments);
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        const Vec3 normal{0.0F, 0.0F, 1.0F};
        const Vec2 uv{0.5F + cosine * 0.5F, 0.5F + sine * 0.5F};
        outer.push_back(builder.vertex({center_x + radius_x * cosine, center_y + radius_y * sine,
                                        depth},
                                       normal, uv, color, influences, influence_count,
                                       material_region));
        inner.push_back(builder.vertex({center_x + radius_x * 0.68F * cosine,
                                        center_y + radius_y * 0.68F * sine, depth + 0.0005F},
                                       normal, uv, shade(color, 0.70F), influences,
                                       influence_count, material_region));
    }
    for (std::size_t index = 0U; index < segments; ++index) {
        const std::size_t next = (index + 1U) % segments;
        builder.triangle(outer[index], inner[index], outer[next]);
        builder.triangle(outer[next], inner[index], inner[next]);
    }
}

[[nodiscard]] std::size_t detailSegments(const AppearanceOptions& options) noexcept {
    return options.detail_level >= 3U ? 24U : options.detail_level == 1U ? 10U : 16U;
}

void initializeMorphs(AppearanceArtifact& artifact) {
    constexpr std::array<std::string_view, 4U> names{
        "eyelidsClose", "eyelidsArc", "neckFlex", "handsRelax"};
    for (std::size_t index = 0U; index < names.size(); ++index) {
        artifact.morphs[index].name = names[index];
        artifact.morphs[index].position_deltas.assign(artifact.body.vertices.size(), {});
        artifact.morphs[index].normal_deltas.assign(artifact.body.vertices.size(), {});
    }
}

void buildMorphs(AppearanceArtifact& artifact, const FacePhenotype& face) {
    MorphTarget& close = artifact.morphs[0];
    MorphTarget& arc = artifact.morphs[1];
    MorphTarget& neck = artifact.morphs[2];
    MorphTarget& hands = artifact.morphs[3];
    for (std::size_t index = 0U; index < artifact.body.vertices.size(); ++index) {
        const Vec3 position = artifact.body.vertices[index].position;
        const float eye_left = std::exp(-((position.x + face.eye_spacing) *
                                          (position.x + face.eye_spacing)) /
                                         std::max(1.0e-5F, face.eye_radius * face.eye_radius));
        const float eye_right = std::exp(-((position.x - face.eye_spacing) *
                                           (position.x - face.eye_spacing)) /
                                          std::max(1.0e-5F, face.eye_radius * face.eye_radius));
        const float eye_y = std::exp(-((position.y - face.eye_y) * (position.y - face.eye_y)) /
                                     std::max(1.0e-5F, face.eye_radius * face.eye_radius));
        const float eyelid = std::clamp((eye_left + eye_right) * eye_y, 0.0F, 1.0F);
        close.position_deltas[index] = {0.0F, -0.006F * eyelid, -0.002F * eyelid};
        arc.position_deltas[index] = {0.0F, 0.003F * eyelid, 0.001F * eyelid};
        const float neck_factor = std::clamp((position.y - face.mouth_y) /
                                                 std::max(0.001F, face.hairline_y - face.mouth_y),
                                             0.0F, 1.0F);
        neck.position_deltas[index] = {0.0F, 0.002F * neck_factor, -0.001F * neck_factor};
        const float hand_factor = std::clamp((position.y - 0.45F) / 0.45F, 0.0F, 1.0F);
        hands.position_deltas[index] = {0.0F, -0.0005F * hand_factor, 0.0F};
    }
}

} // namespace

bool AppearanceOptions::valid() const noexcept {
    return version != 0U && detail_level >= 1U && detail_level <= 3U && finite(skin_color) &&
           finite(cloth_color) && skin_color.a > 0.0F && cloth_color.a > 0.0F;
}

foundation::StableId AppearanceOptions::hash() const noexcept {
    foundation::StableId result = foundation::stable_id("infantry.appearance.options.v1");
    result = foundation::stableHashCombine(result, version);
    result = foundation::stableHashCombine(result, detail_level);
    result = foundation::stableHashCombine(result, seed);
    result = foundation::stableHashCombine(result, static_cast<std::uint64_t>(hair_style));
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(skin_color.r));
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(skin_color.g));
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(skin_color.b));
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(cloth_color.r));
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(cloth_color.g));
    result = foundation::stableHashCombine(result, foundation::stableHashFloat(cloth_color.b));
    return result;
}

bool AppearanceArtifact::valid(const SkeletonData& skeleton) const noexcept {
    if (version == 0U || cache_key == 0 || !has_eye_openings || !has_mouth_opening ||
        body.vertices.empty() || body.indices.empty() || !finite(minimum) || !finite(maximum)) {
        return false;
    }
    const auto validMesh = [&skeleton](const AppearanceMesh& mesh) {
        for (const AppearanceVertex& vertex : mesh.vertices) {
            if (!finite(vertex.position) || !finite(vertex.normal) || !finite(vertex.color) ||
                vertex.influence_count == 0U || vertex.influence_count > 4U) {
                return false;
            }
            float sum = 0.0F;
            for (std::size_t index = 0U; index < vertex.influence_count; ++index) {
                const SkinInfluence influence = vertex.influences[index];
                if (influence.bone_index >= skeleton.bones().size() ||
                    !std::isfinite(influence.weight) || influence.weight <= 0.0F) {
                    return false;
                }
                sum += influence.weight;
            }
            if (std::abs(sum - 1.0F) > 1.0e-4F) {
                return false;
            }
        }
        for (const std::uint32_t index : mesh.indices) {
            if (index >= mesh.vertices.size()) {
                return false;
            }
        }
        return true;
    };
    if (!validMesh(body) || (!hair.vertices.empty() && !validMesh(hair))) {
        return false;
    }
    for (const MorphTarget& morph : morphs) {
        if (morph.name.empty() || morph.position_deltas.size() != body.vertices.size() ||
            morph.normal_deltas.size() != body.vertices.size()) {
            return false;
        }
        for (std::size_t index = 0U; index < body.vertices.size(); ++index) {
            if (!finite(morph.position_deltas[index]) || !finite(morph.normal_deltas[index])) {
                return false;
            }
        }
    }
    return minimum.x <= maximum.x && minimum.y <= maximum.y && minimum.z <= maximum.z;
}

foundation::StableId AppearanceCompiler::cacheKey(const PhenotypeArtifact& phenotype,
                                                  const SkeletonData& skeleton,
                                                  const AppearanceOptions& options) noexcept {
    foundation::StableId result = foundation::stable_id("infantry.appearance.v1");
    result = foundation::stableHashCombine(result, phenotype.cache_key);
    result = foundation::stableHashCombine(result, skeleton.cacheKey());
    result = foundation::stableHashCombine(result, options.hash());
    return result;
}

foundation::Result<AppearanceArtifact, foundation::Error> AppearanceCompiler::build(
    const PhenotypeArtifact& phenotype,
    const SkeletonData& skeleton,
    const AppearanceOptions& options) {
    if (!phenotype.valid() || !skeleton.valid() || !options.valid()) {
        return foundation::Result<AppearanceArtifact, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid infantry appearance input"});
    }

    const BodyPhenotype& body = phenotype.body;
    const FacePhenotype& face = phenotype.face;
    const std::size_t segments = detailSegments(options);
    const foundation::Color cloth = options.cloth_color;
    const foundation::Color skin = options.skin_color;
    MeshBuilder builder{};
    std::vector<std::uint32_t> previous;
    const std::array<float, 12U> torso_t{
        0.0F, 0.08F, 0.18F, 0.28F, 0.38F, 0.48F,
        0.58F, 0.68F, 0.78F, 0.86F, 0.94F, 1.0F};
    for (const float t : torso_t) {
        const float y = body.pelvis.y + (body.chest.y - body.pelvis.y) * t;
        const float hip_radius = body.hip_width * 0.50F;
        const float waist_radius = body.waist_width * 0.50F;
        const float shoulder_radius = body.shoulder_width * 0.50F;
        const float upper_t = std::clamp((t - 0.42F) / 0.58F, 0.0F, 1.0F);
        const float lower_t = std::clamp(t / 0.42F, 0.0F, 1.0F);
        const float lower_smooth = lower_t * lower_t * (3.0F - 2.0F * lower_t);
        const float upper_smooth = upper_t * upper_t * (3.0F - 2.0F * upper_t);
        const float width = lower_t < 1.0F
                                ? hip_radius + (waist_radius - hip_radius) * lower_smooth
                                : waist_radius +
                                      (shoulder_radius - waist_radius) * upper_smooth;
        const float depth = body.chest_depth * (0.72F + 0.28F * t);
        std::uint8_t count = 0U;
        const auto influence = torsoWeights(body, skeleton, y, count);
        std::vector<std::uint32_t> ring;
        appendRing(builder, {0.0F, y, body.pelvis.z}, width, depth * 0.5F, segments, cloth, 0U,
                    influence, count, ring);
        if (!previous.empty()) {
            bridge(builder, previous, ring);
        }
        previous = std::move(ring);
    }
    std::uint8_t torso_count = 0U;
    const auto torso_influence = torsoWeights(body, skeleton, body.chest.y, torso_count);
    cap(builder, previous, {0.0F, body.chest.y, body.chest.z}, {0.0F, 1.0F, 0.0F}, cloth,
        0U, torso_influence, torso_count);

    const std::array<BoneId, 4U> limb_ids{BoneId::UpperArmL, BoneId::ForeArmL, BoneId::ThighL,
                                          BoneId::ShinL};
    const std::array<BoneId, 4U> limb_end_ids{BoneId::ForeArmL, BoneId::HandL, BoneId::ShinL,
                                              BoneId::FootL};
    for (std::size_t index = 0U; index < limb_ids.size(); ++index) {
        const BoneRecord* start = skeleton.find(limb_ids[index]);
        const BoneRecord* end = skeleton.find(limb_end_ids[index]);
        if (start == nullptr || end == nullptr) {
            return foundation::Result<AppearanceArtifact, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState, "infantry rig is missing limb attachment"});
        }
        std::uint8_t start_count = 0U;
        std::uint8_t end_count = 0U;
        const auto start_weight = weights({{indexOf(skeleton, limb_ids[index]), 1.0F}}, start_count);
        const auto end_weight = weights({{indexOf(skeleton, limb_end_ids[index]), 1.0F}}, end_count);
        const foundation::Color trouser = shade(cloth, 0.88F);
        const foundation::Color boot = shade(cloth, 0.42F);
        const foundation::Color limb_color = index < 2U ? cloth : trouser;
        appendLimb(builder, start->world_bind.translation, end->world_bind.translation,
                   body.height * (index < 2U ? 0.045F : 0.065F),
                   body.height * (index < 2U ? 0.038F : 0.052F), segments / 2U + 4U,
                   limb_color, index < 2U ? 0U : 11U,
                   start_weight, start_count, end_weight, end_count);
        const BoneId right_start = static_cast<BoneId>(boneIndex(limb_ids[index]) + 8U);
        const BoneId right_end = static_cast<BoneId>(boneIndex(limb_end_ids[index]) + 8U);
        const BoneRecord* right_start_bone = skeleton.find(right_start);
        const BoneRecord* right_end_bone = skeleton.find(right_end);
        if (right_start_bone == nullptr || right_end_bone == nullptr) {
            return foundation::Result<AppearanceArtifact, foundation::Error>::failure(
                {foundation::ErrorCode::InvalidState, "infantry rig is missing right limb attachment"});
        }
        const auto right_start_weight = weights({{indexOf(skeleton, right_start), 1.0F}}, start_count);
        const auto right_end_weight = weights({{indexOf(skeleton, right_end), 1.0F}}, end_count);
            appendLimb(builder, right_start_bone->world_bind.translation,
                       right_end_bone->world_bind.translation,
                       body.height * (index < 2U ? 0.045F : 0.065F),
                       body.height * (index < 2U ? 0.038F : 0.052F), segments / 2U + 4U,
                       limb_color, index < 2U ? 0U : 11U,
                       right_start_weight, start_count, right_end_weight, end_count);

        // The reference surface separates hands and boots from the jacket /
        // trouser shells.  Keep those pieces on the same rig so they deform
        // with the existing animation palette instead of becoming static
        // decorative meshes.
        if (index == 1U) {
            const BoneRecord* hand = skeleton.find(BoneId::HandL);
            const BoneRecord* right_hand = skeleton.find(BoneId::HandR);
            if (hand == nullptr || right_hand == nullptr) {
                return foundation::Result<AppearanceArtifact, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState,
                     "infantry rig is missing hand attachment"});
            }
            std::uint8_t hand_count = 0U;
            const auto hand_weight =
                weights({{indexOf(skeleton, BoneId::HandL), 1.0F}}, hand_count);
            appendEllipsoid(builder, hand->world_bind.translation,
                            {body.height * 0.028F, body.height * 0.040F,
                             body.height * 0.024F},
                            std::max<std::size_t>(8U, segments / 2U), 4U,
                            shade(skin, 0.98F), 12U, hand_weight, hand_count);
            const auto right_hand_weight =
                weights({{indexOf(skeleton, BoneId::HandR), 1.0F}}, hand_count);
            appendEllipsoid(builder, right_hand->world_bind.translation,
                            {body.height * 0.028F, body.height * 0.040F,
                             body.height * 0.024F},
                            std::max<std::size_t>(8U, segments / 2U), 4U,
                            shade(skin, 0.98F), 12U, right_hand_weight, hand_count);
        } else if (index == 3U) {
            const BoneRecord* foot = skeleton.find(BoneId::FootL);
            const BoneRecord* right_foot = skeleton.find(BoneId::FootR);
            if (foot == nullptr || right_foot == nullptr) {
                return foundation::Result<AppearanceArtifact, foundation::Error>::failure(
                    {foundation::ErrorCode::InvalidState,
                     "infantry rig is missing foot attachment"});
            }
            std::uint8_t foot_count = 0U;
            const auto foot_weight =
                weights({{indexOf(skeleton, BoneId::FootL), 1.0F}}, foot_count);
            appendEllipsoid(builder, foot->world_bind.translation,
                            {body.height * 0.060F, body.height * 0.045F,
                             body.height * 0.105F},
                            std::max<std::size_t>(10U, segments / 2U), 5U,
                            boot, 13U, foot_weight, foot_count);
            const auto right_foot_weight =
                weights({{indexOf(skeleton, BoneId::FootR), 1.0F}}, foot_count);
            appendEllipsoid(builder, right_foot->world_bind.translation,
                            {body.height * 0.060F, body.height * 0.045F,
                             body.height * 0.105F},
                            std::max<std::size_t>(10U, segments / 2U), 5U,
                            boot, 13U, right_foot_weight, foot_count);
        }
    }

    const std::array<float, 16U> head_levels{
        face.mouth_y - 0.035F,
        face.mouth_y - 0.012F,
        face.mouth_y,
        face.mouth_y + 0.012F,
        face.nose_y - 0.018F,
        face.nose_y,
        face.nose_y + 0.018F,
        face.eye_y - face.eye_radius * 1.35F,
        face.eye_y,
        face.eye_y + face.eye_radius * 1.35F,
        face.brow_y,
        face.brow_y + (face.hairline_y - face.brow_y) * 0.30F,
        face.brow_y + (face.hairline_y - face.brow_y) * 0.60F,
        face.brow_y + (face.hairline_y - face.brow_y) * 0.84F,
        face.hairline_y,
        face.hairline_y + 0.018F};
    previous.clear();
    for (const float y : head_levels) {
        const FaceSection section = face.section(y);
        std::uint8_t count = 0U;
        const auto influence = weights({{indexOf(skeleton, BoneId::Head), 1.0F}}, count);
        std::vector<std::uint32_t> ring;
        appendRing(builder, {0.0F, y, section.center_z}, section.radius_x, section.radius_z,
                    segments, skin, 1U, influence, count, ring);
        if (!previous.empty()) {
            for (std::size_t index = 0U; index < previous.size(); ++index) {
                const std::size_t next = (index + 1U) % previous.size();
                const float center_x = (builder.mesh.vertices[previous[index]].position.x +
                                        builder.mesh.vertices[ring[index]].position.x) * 0.5F;
                const float center_y = (builder.mesh.vertices[previous[index]].position.y +
                                        builder.mesh.vertices[ring[index]].position.y) * 0.5F;
                const float center_z = (builder.mesh.vertices[previous[index]].position.z +
                                        builder.mesh.vertices[ring[index]].position.z) * 0.5F;
                const FaceSection center_section = face.section(center_y);
                const bool front_side = center_z > center_section.center_z +
                                        center_section.radius_z * 0.08F;
                const bool eye_hole = front_side &&
                                      std::abs(center_y - face.eye_y) < face.eye_radius * 1.35F &&
                                      std::abs(std::abs(center_x) - face.eye_spacing) <
                                          face.eye_radius * 1.8F;
                const bool mouth_hole = front_side &&
                                        std::abs(center_y - face.mouth_y) < 0.018F &&
                                        std::abs(center_x) < face.mouth_width * 0.62F;
                if (!eye_hole && !mouth_hole) {
                    builder.triangle(previous[index], ring[index], previous[next]);
                    builder.triangle(previous[next], ring[index], ring[next]);
                }
            }
        }
        previous = std::move(ring);
    }

    std::uint8_t head_count = 0U;
    const auto head_weights = weights({{indexOf(skeleton, BoneId::Head), 1.0F}}, head_count);
    appendOpening(builder, -face.eye_spacing, face.eye_y, face.eye_radius * 1.45F,
                  face.eye_radius * 0.90F, face.frontZ(face.eye_y) + 0.001F, segments / 2U + 4U,
                  shade(skin, 0.96F), 2U, head_weights, head_count);
    appendOpening(builder, face.eye_spacing, face.eye_y, face.eye_radius * 1.45F,
                  face.eye_radius * 0.90F, face.frontZ(face.eye_y) + 0.001F, segments / 2U + 4U,
                  shade(skin, 0.96F), 2U, head_weights, head_count);
    appendOpening(builder, 0.0F, face.mouth_y, face.mouth_width * 0.50F, 0.010F,
                  face.frontZ(face.mouth_y) + 0.001F, segments / 2U + 4U, {0.12F, 0.035F, 0.025F, 1.0F},
                  3U, head_weights, head_count);
    const std::size_t feature_segments = std::max<std::size_t>(8U, segments / 2U);
    const std::size_t feature_rows = options.detail_level >= 3U ? 8U : 5U;
    const foundation::Color eye_white{0.86F, 0.85F, 0.78F, 1.0F};
    const foundation::Color iris_color{0.18F, 0.25F, 0.20F, 1.0F};
    const foundation::Color pupil_color{0.015F, 0.012F, 0.010F, 1.0F};
    for (const float side : {-1.0F, 1.0F}) {
        const float eye_front = face.frontZ(face.eye_y);
        appendEllipsoid(builder,
                        {side * face.eye_spacing, face.eye_y, eye_front - face.eye_radius * 0.42F},
                        {face.eye_radius * 1.18F, face.eye_radius * 0.84F,
                         face.eye_radius * 0.66F},
                        feature_segments, feature_rows, eye_white, 5U, head_weights, head_count);
        appendEllipsoid(builder,
                        {side * face.eye_spacing, face.eye_y, eye_front + face.eye_radius * 0.28F},
                        {face.eye_radius * 0.46F, face.eye_radius * 0.46F,
                         face.eye_radius * 0.12F},
                        feature_segments, 3U, iris_color, 6U, head_weights, head_count);
        appendEllipsoid(builder,
                        {side * face.eye_spacing, face.eye_y, eye_front + face.eye_radius * 0.39F},
                        {face.eye_radius * 0.17F, face.eye_radius * 0.17F,
                         face.eye_radius * 0.06F},
                        feature_segments, 2U, pupil_color, 7U, head_weights, head_count);
        const float ear_x = side * (face.section(face.eye_y).radius_x * 0.96F);
        appendEllipsoid(builder, {ear_x, face.eye_y - face.eye_radius * 0.25F,
                                  face.section(face.eye_y).center_z},
                        {face.eye_radius * 0.46F, face.eye_radius * 1.25F,
                         face.eye_radius * 0.30F},
                        feature_segments, 4U, shade(skin, 0.86F), 8U, head_weights, head_count);
    }
    const float nose_front = face.frontZ(face.nose_y);
    appendLimb(builder, {0.0F, face.eye_y - face.eye_radius, nose_front},
               {0.0F, face.nose_y, nose_front + face.nose_length * 0.34F},
               face.nose_width * 0.28F, face.nose_width * 0.52F, feature_segments,
               shade(skin, 1.04F), 9U, head_weights, head_count, head_weights, head_count);
    appendEllipsoid(builder, {0.0F, face.mouth_y + face.mouth_width * 0.02F,
                              face.frontZ(face.mouth_y) + 0.0018F},
                    {face.mouth_width * 0.48F, 0.0028F, 0.0014F},
                    feature_segments, 2U, shade(skin, 1.08F), 10U, head_weights, head_count);
    appendEllipsoid(builder, {0.0F, face.mouth_y - 0.0028F,
                              face.frontZ(face.mouth_y) + 0.0019F},
                    {face.mouth_width * 0.43F, 0.0026F, 0.0013F},
                    feature_segments, 2U, shade(skin, 0.92F), 10U, head_weights, head_count);
    cap(builder, previous, {0.0F, face.hairline_y + 0.018F, face.section(face.hairline_y).center_z},
        {0.0F, 1.0F, 0.0F}, skin, 1U, head_weights, head_count);

    AppearanceArtifact result{};
    result.version = options.version;
    result.cache_key = cacheKey(phenotype, skeleton, options);
    result.body = std::move(builder.mesh);
    result.has_eye_openings = true;
    result.has_mouth_opening = true;

    if (options.hair_style != HairStyle::Bald) {
        MeshBuilder hair_builder{};
        proc::RandomStream random(options.seed, foundation::stable_id("infantry.appearance.hair"));
        const float style_scale = [&options] {
            switch (options.hair_style) {
            case HairStyle::Bald:
                return 0.0F;
            case HairStyle::Short:
                return 1.0F;
            case HairStyle::Long:
                return 1.18F;
            case HairStyle::Braids:
                return 1.08F;
            case HairStyle::Bun:
                return 1.26F;
            case HairStyle::Mohawk:
                return 1.35F;
            case HairStyle::Curly:
                return 1.12F;
            }
            return 1.0F;
        }();
        const float style_width = options.hair_style == HairStyle::Mohawk
                                      ? 0.72F
                                      : options.hair_style == HairStyle::Bun ? 1.06F : 1.0F;
        const std::size_t hair_segments = options.detail_level >= 3U
                                              ? std::max<std::size_t>(24U, segments * 2U)
                                              : options.detail_level == 1U
                                                    ? std::max<std::size_t>(12U, segments)
                                                    : std::max<std::size_t>(20U, segments + 4U);
        const std::uint8_t hair_count = head_count;
        foundation::Color hair_color = options.hair_style == HairStyle::Curly
                                           ? foundation::Color{0.12F, 0.07F, 0.035F, 1.0F}
                                           : foundation::Color{0.06F, 0.045F, 0.035F, 1.0F};
        hair_color = shade(hair_color, 0.78F + static_cast<float>(random.uniform01()) * 0.32F);
        const std::size_t hair_rows = options.detail_level >= 3U ? 12U : 8U;
        std::vector<std::uint32_t> hair_previous;
        std::vector<std::uint32_t> hair_first;
        for (std::size_t row = 0U; row <= hair_rows; ++row) {
            const float t = static_cast<float>(row) / static_cast<float>(hair_rows);
            const float y = face.hairline_y + 0.060F * style_scale * t;
            const FaceSection section = face.section(y);
            const float taper = 1.0F - 0.20F * t;
            std::vector<std::uint32_t> hair_ring;
            appendRing(hair_builder, {0.0F, y, section.center_z},
                       section.radius_x * (0.98F + 0.09F * style_width) * taper,
                       section.radius_z * (1.00F + 0.04F * style_width) * taper,
                       hair_segments, shade(hair_color, 0.90F + 0.08F * t), 4U,
                       head_weights, hair_count, hair_ring);
            if (!hair_previous.empty()) {
                bridge(hair_builder, hair_previous, hair_ring);
            } else {
                hair_first = hair_ring;
            }
            hair_previous = std::move(hair_ring);
        }
        if (!hair_previous.empty()) {
            cap(hair_builder, hair_previous,
                {0.0F, face.hairline_y + 0.060F * style_scale, face.section(face.hairline_y).center_z},
                {0.0F, 1.0F, 0.0F}, hair_color, 4U, head_weights, hair_count);
        }
        // A narrow front skirt anchors the hair to the scalp and avoids the
        // detached floating ring produced by the former two-ring generator.
        if (!hair_first.empty()) {
            std::vector<std::uint32_t> hair_skirt;
            appendRing(hair_builder, {0.0F, face.hairline_y - 0.002F,
                                      face.section(face.hairline_y).center_z},
                       face.section(face.hairline_y).radius_x * 1.005F,
                       face.section(face.hairline_y).radius_z * 1.005F,
                       hair_segments, shade(hair_color, 0.84F), 4U,
                       head_weights, hair_count, hair_skirt);
            bridge(hair_builder, hair_skirt, hair_first);
        }
        result.hair = std::move(hair_builder.mesh);
    }

    initializeMorphs(result);
    buildMorphs(result, face);
    result.minimum = result.body.minimum;
    result.maximum = result.body.maximum;
    if (!result.hair.vertices.empty()) {
        result.minimum.x = std::min(result.minimum.x, result.hair.minimum.x);
        result.minimum.y = std::min(result.minimum.y, result.hair.minimum.y);
        result.minimum.z = std::min(result.minimum.z, result.hair.minimum.z);
        result.maximum.x = std::max(result.maximum.x, result.hair.maximum.x);
        result.maximum.y = std::max(result.maximum.y, result.hair.maximum.y);
        result.maximum.z = std::max(result.maximum.z, result.hair.maximum.z);
    }
    return result.valid(skeleton)
               ? foundation::Result<AppearanceArtifact, foundation::Error>::success(std::move(result))
               : foundation::Result<AppearanceArtifact, foundation::Error>::failure(
                     {foundation::ErrorCode::InvalidState,
                      "infantry appearance mesh or skin contract is invalid"});
}

AppearanceCache::Artifact AppearanceCache::find(foundation::StableId key) const {
    std::scoped_lock lock(mutex_);
    const auto iterator = entries_.find(key);
    if (iterator == entries_.end()) {
        ++misses_;
        return {};
    }
    ++hits_;
    return iterator->second;
}

bool AppearanceCache::insert(Artifact artifact) {
    if (!artifact || artifact->cache_key == 0) {
        return false;
    }
    std::scoped_lock lock(mutex_);
    return entries_.emplace(artifact->cache_key, std::move(artifact)).second;
}

void AppearanceCache::clear() {
    std::scoped_lock lock(mutex_);
    entries_.clear();
}

std::size_t AppearanceCache::size() const noexcept {
    std::scoped_lock lock(mutex_);
    return entries_.size();
}

std::size_t AppearanceCache::hits() const noexcept {
    std::scoped_lock lock(mutex_);
    return hits_;
}

std::size_t AppearanceCache::misses() const noexcept {
    std::scoped_lock lock(mutex_);
    return misses_;
}

AppearanceCache::Artifact AppearanceCache::acquire(const PhenotypeArtifact& phenotype,
                                                   const SkeletonData& skeleton,
                                                   const AppearanceOptions& options) {
    const foundation::StableId key = AppearanceCompiler::cacheKey(phenotype, skeleton, options);
    if (Artifact cached = find(key)) {
        return cached;
    }
    const auto built = AppearanceCompiler::build(phenotype, skeleton, options);
    if (!built) {
        return {};
    }
    Artifact artifact = std::make_shared<AppearanceArtifact>(built.value());
    if (insert(artifact)) {
        return artifact;
    }
    return find(key);
}

} // namespace genomes::infantry
