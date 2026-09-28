#include <genomes/infantry/BodySurfaceGenerator.hpp>
#include <genomes/infantry/InfantryMaterials.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>

namespace genomes::infantry {

namespace {

[[nodiscard]] std::uint16_t indexOf(const SkeletonData& skeleton, BoneId id) noexcept {
    return skeleton.find(id) == nullptr ? kInvalidBoneIndex
                                        : static_cast<std::uint16_t>(boneIndex(id));
}

[[nodiscard]] NormalizedInfluences blendWeights(
    std::initializer_list<SkinInfluence> candidates) noexcept {
    return normalizeTopFour(std::span<const SkinInfluence>(candidates.begin(), candidates.size()));
}

[[nodiscard]] foundation::Vec3 add(foundation::Vec3 left, foundation::Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] foundation::Vec3 subtract(foundation::Vec3 left,
                                         foundation::Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] foundation::Vec3 multiply(foundation::Vec3 value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] float dot(foundation::Vec3 left, foundation::Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] foundation::Vec3 cross(foundation::Vec3 left,
                                     foundation::Vec3 right) noexcept {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

[[nodiscard]] float angleFor(std::size_t index, std::size_t segments) noexcept {
    return 6.2831853071795864769F * static_cast<float>(index) /
           static_cast<float>(segments);
}

[[nodiscard]] foundation::Vec3 normalized(
    foundation::Vec3 value, foundation::Vec3 fallback = {0.0F, 1.0F, 0.0F}) noexcept {
    const float magnitude = std::sqrt(dot(value, value));
    return magnitude > 1.0e-6F ? multiply(value, 1.0F / magnitude) : fallback;
}

[[nodiscard]] foundation::Color shade(foundation::Color color, float factor) noexcept {
    return {std::clamp(color.r * factor, 0.0F, 1.0F),
            std::clamp(color.g * factor, 0.0F, 1.0F),
            std::clamp(color.b * factor, 0.0F, 1.0F), color.a};
}

void bridge(AppearanceMeshBuilder& builder, const geometry::Ring& first,
            const geometry::Ring& second) {
    const std::size_t count = std::min(first.indices.size(), second.indices.size());
    for (std::size_t index = 0U; index < count; ++index) {
        const std::size_t next = (index + 1U) % count;
        builder.triangle(first.indices[index], second.indices[index], first.indices[next]);
        builder.triangle(first.indices[next], second.indices[index], second.indices[next]);
    }
}

void appendProfileRing(AppearanceMeshBuilder& builder, foundation::Vec3 center,
                       float radius_x, float radius_z, std::size_t segments,
                       foundation::Color color, std::uint16_t material_region,
                       const NormalizedInfluences& influences, float uv_v,
                       geometry::Ring& ring) {
    std::vector<AppearanceVertexSpec> specs;
    specs.reserve(segments);
    for (std::size_t index = 0U; index < segments; ++index) {
        const float angle = 6.2831853071795864769F * static_cast<float>(index) /
                            static_cast<float>(segments);
        const foundation::Vec3 normal{std::cos(angle), 0.0F, std::sin(angle)};
        specs.push_back({
            {center.x + radius_x * normal.x, center.y,
             center.z + radius_z * normal.z},
            normal,
            {static_cast<float>(index) / static_cast<float>(segments), uv_v},
            color,
            material_region,
            std::span<const SkinInfluence>(influences.values.data(), influences.count)});
    }
    ring = builder.appendRing(specs);
}

} // namespace

bool BodySurfaceGenerator::buildTorso(
    AppearanceMeshBuilder& builder, const BodyPhenotype& body, const SkeletonData& skeleton,
    std::size_t segments, foundation::Color color, geometry::Ring& neck_ring) {
    if (segments < 3U || body.chest.y <= body.pelvis.y) {
        return false;
    }
    const std::array<float, 12U> torso_t{
        0.0F, 0.08F, 0.18F, 0.28F, 0.38F, 0.48F,
        0.58F, 0.68F, 0.78F, 0.86F, 0.94F, 1.0F};
    geometry::Ring previous;
    const auto torsoWeights = [&body, &skeleton](float y) {
        const float pelvis = body.pelvis.y;
        const float chest = body.chest.y;
        const float neck = body.chest.y + (body.head.y - body.chest.y) * 0.63F;
        if (y <= pelvis) {
            return blendWeights({{indexOf(skeleton, BoneId::Hips), 1.0F}});
        }
        if (y < chest) {
            const float t = std::clamp((y - pelvis) / std::max(0.001F, chest - pelvis),
                                       0.0F, 1.0F);
            return blendWeights({{indexOf(skeleton, BoneId::SpineLower), (1.0F - t) * 0.65F},
                                 {indexOf(skeleton, BoneId::SpineUpper), (1.0F - t) * 0.35F},
                                 {indexOf(skeleton, BoneId::Chest), t}});
        }
        if (y < neck) {
            const float t = std::clamp((y - chest) / std::max(0.001F, neck - chest),
                                       0.0F, 1.0F);
            return blendWeights({{indexOf(skeleton, BoneId::Chest), 1.0F - t},
                                 {indexOf(skeleton, BoneId::Neck), t}});
        }
        return blendWeights({{indexOf(skeleton, BoneId::Neck), 1.0F}});
    };
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
            : waist_radius + (shoulder_radius - waist_radius) * upper_smooth;
        const float depth = body.chest_depth * (0.72F + 0.28F * t);
        const geometry::Ring current = [&] {
            geometry::Ring ring;
            appendProfileRing(builder, {0.0F, y, body.pelvis.z}, width, depth * 0.5F,
                              segments, color,
                              static_cast<std::uint16_t>(AppearanceMaterialRegion::UniformCloth),
                              torsoWeights(y), t, ring);
            return ring;
        }();
        if (!previous.indices.empty()) {
            bridge(builder, previous, current);
        }
        previous = current;
    }
    const float neck_y = body.chest.y + (body.head.y - body.chest.y) * 0.63F;
    const auto neck_weights = torsoWeights(neck_y);
    appendProfileRing(builder, {0.0F, neck_y, body.head.z},
                      body.height * 0.0285F * body.neck_scale,
                      body.height * 0.024F * body.neck_scale, segments, color,
                      static_cast<std::uint16_t>(AppearanceMaterialRegion::UniformCloth),
                      neck_weights, 1.0F, neck_ring);
    bridge(builder, previous, neck_ring);
    return true;
}

bool BodySurfaceGenerator::buildLimbs(
    AppearanceMeshBuilder& builder, const BodyPhenotype& body, const SkeletonData& skeleton,
    std::size_t segments, foundation::Color cloth, foundation::Color skin) {
    if (segments < 3U || !body.valid() || !skeleton.valid()) {
        return false;
    }
    const std::array<BoneId, 4U> limb_ids{BoneId::UpperArmL, BoneId::ForeArmL,
                                          BoneId::ThighL, BoneId::ShinL};
    const std::array<BoneId, 4U> limb_end_ids{BoneId::ForeArmL, BoneId::HandL,
                                              BoneId::ShinL, BoneId::FootL};
    const auto emit_finger_set = [&builder, &skeleton, &body, &skin, segments](bool left) {
        const std::array<BoneId, 5U> roots = left
            ? std::array<BoneId, 5U>{BoneId::FingerLLittle0, BoneId::FingerLRing0,
                                     BoneId::FingerLMiddle0, BoneId::FingerLIndex0,
                                     BoneId::FingerLThumb0}
            : std::array<BoneId, 5U>{BoneId::FingerRIndex0, BoneId::FingerRMiddle0,
                                     BoneId::FingerRRing0, BoneId::FingerRLittle0,
                                     BoneId::FingerRThumb0};
        for (std::size_t finger = 0U; finger < roots.size(); ++finger) {
            const BoneId root = roots[finger];
            for (std::size_t segment = 0U; segment < 3U; ++segment) {
                const BoneId start_id = static_cast<BoneId>(boneIndex(root) + segment);
                const BoneRecord* start = skeleton.find(start_id);
                if (start == nullptr) {
                    continue;
                }
                const BoneRecord* end = segment < 2U
                    ? skeleton.find(static_cast<BoneId>(boneIndex(root) + segment + 1U))
                    : nullptr;
                const foundation::Vec3 direction = end == nullptr
                    ? normalized(subtract(
                          start->world_bind.translation,
                          skeleton.find(static_cast<BoneId>(boneIndex(root) +
                                                           (segment == 0U ? 0U : segment - 1U)))
                              ->world_bind.translation),
                          left ? foundation::Vec3{-1.0F, 0.0F, 0.0F}
                               : foundation::Vec3{1.0F, 0.0F, 0.0F})
                    : normalized(subtract(end->world_bind.translation,
                                          start->world_bind.translation));
                const foundation::Vec3 end_position = end == nullptr
                    ? add(start->world_bind.translation,
                          multiply(direction, body.height * 0.018F))
                    : end->world_bind.translation;
                const std::uint8_t next_index = static_cast<std::uint8_t>(
                    std::min<std::size_t>(segment + 1U, 2U));
                const auto start_weight = blendWeights({
                    {indexOf(skeleton, start_id), 0.82F},
                    {indexOf(skeleton, static_cast<BoneId>(boneIndex(root) + next_index)), 0.18F}});
                const BoneId end_id = end == nullptr
                    ? start_id
                    : static_cast<BoneId>(boneIndex(root) + segment + 1U);
                const auto end_weight = blendWeights({
                    {indexOf(skeleton, start_id), 0.18F},
                    {indexOf(skeleton, end_id), 0.82F}});
                const float proximal = body.height * (finger == 4U ? 0.010F : 0.008F);
                appendProfiledLimb(builder, start->world_bind.translation, end_position,
                                   proximal, proximal * 0.72F,
                                   std::max<std::size_t>(5U, segments / 3U), skin, 12U,
                                   start_weight.values, start_weight.count,
                                   end_weight.values, end_weight.count);
            }
        }
    };

    for (std::size_t index = 0U; index < limb_ids.size(); ++index) {
        const BoneRecord* start = skeleton.find(limb_ids[index]);
        const BoneRecord* end = skeleton.find(limb_end_ids[index]);
        const BoneId right_start_id = static_cast<BoneId>(boneIndex(limb_ids[index]) + 8U);
        const BoneId right_end_id = static_cast<BoneId>(boneIndex(limb_end_ids[index]) + 8U);
        const BoneRecord* right_start = skeleton.find(right_start_id);
        const BoneRecord* right_end = skeleton.find(right_end_id);
        if (start == nullptr || end == nullptr || right_start == nullptr || right_end == nullptr) {
            return false;
        }
        const auto start_weight = index == 0U
            ? blendWeights({{indexOf(skeleton, BoneId::ClavicleL), 0.28F},
                            {indexOf(skeleton, BoneId::UpperArmL), 0.72F}})
            : index == 1U
                ? blendWeights({{indexOf(skeleton, BoneId::UpperArmL), 0.18F},
                                {indexOf(skeleton, BoneId::ForeArmL), 0.82F}})
                : index == 2U
                    ? blendWeights({{indexOf(skeleton, BoneId::Hips), 0.18F},
                                    {indexOf(skeleton, BoneId::ThighL), 0.82F}})
                    : blendWeights({{indexOf(skeleton, limb_ids[index]), 1.0F}});
        const auto end_weight = index == 0U
            ? blendWeights({{indexOf(skeleton, BoneId::UpperArmL), 1.0F}})
            : index == 1U
                ? blendWeights({{indexOf(skeleton, BoneId::ForeArmL), 0.72F},
                                {indexOf(skeleton, BoneId::HandL), 0.28F}})
                : index == 2U
                    ? blendWeights({{indexOf(skeleton, BoneId::ThighL), 0.72F},
                                    {indexOf(skeleton, BoneId::ShinL), 0.28F}})
                    : blendWeights({{indexOf(skeleton, limb_end_ids[index]), 1.0F}});
        const auto right_start_weight = index == 0U
            ? blendWeights({{indexOf(skeleton, BoneId::ClavicleR), 0.28F},
                            {indexOf(skeleton, BoneId::UpperArmR), 0.72F}})
            : index == 1U
                ? blendWeights({{indexOf(skeleton, BoneId::UpperArmR), 0.18F},
                                {indexOf(skeleton, BoneId::ForeArmR), 0.82F}})
                : index == 2U
                    ? blendWeights({{indexOf(skeleton, BoneId::Hips), 0.18F},
                                    {indexOf(skeleton, BoneId::ThighR), 0.82F}})
                    : blendWeights({{indexOf(skeleton, right_start_id), 1.0F}});
        const auto right_end_weight = index == 0U
            ? blendWeights({{indexOf(skeleton, BoneId::UpperArmR), 1.0F}})
            : index == 1U
                ? blendWeights({{indexOf(skeleton, BoneId::ForeArmR), 0.72F},
                                {indexOf(skeleton, BoneId::HandR), 0.28F}})
                : index == 2U
                    ? blendWeights({{indexOf(skeleton, BoneId::ThighR), 0.72F},
                                    {indexOf(skeleton, BoneId::ShinR), 0.28F}})
                    : blendWeights({{indexOf(skeleton, right_end_id), 1.0F}});
        const foundation::Color trouser = shade(cloth, 0.88F);
        const foundation::Color boot = shade(cloth, 0.42F);
        const foundation::Color limb_color = index < 2U ? cloth : trouser;
        const float arm_radius = body.limb_thickness * 0.42F;
        const float leg_radius = body.limb_thickness * 0.62F;
        appendProfiledLimb(builder, start->world_bind.translation, end->world_bind.translation,
                           index < 2U ? arm_radius : leg_radius,
                           index < 2U ? arm_radius * 0.84F : leg_radius * 0.80F,
                           segments / 2U + 4U,
                           limb_color, index < 2U ? 0U : 11U,
                           start_weight.values, start_weight.count,
                           end_weight.values, end_weight.count);
        appendProfiledLimb(builder, right_start->world_bind.translation,
                           right_end->world_bind.translation,
                           index < 2U ? arm_radius : leg_radius,
                           index < 2U ? arm_radius * 0.84F : leg_radius * 0.80F,
                           segments / 2U + 4U,
                           limb_color, index < 2U ? 0U : 11U,
                           right_start_weight.values, right_start_weight.count,
                           right_end_weight.values, right_end_weight.count);
        if (index == 1U) {
            const BoneRecord* hand = skeleton.find(BoneId::HandL);
            const BoneRecord* right_hand = skeleton.find(BoneId::HandR);
            if (hand == nullptr || right_hand == nullptr) {
                return false;
            }
            const auto hand_weight = blendWeights({{indexOf(skeleton, BoneId::HandL), 1.0F}});
            const auto right_hand_weight = blendWeights({{indexOf(skeleton, BoneId::HandR), 1.0F}});
            appendEllipsoid(builder, hand->world_bind.translation,
                            {body.height * 0.028F * body.hand_scale,
                             body.height * 0.040F * body.hand_scale,
                             body.height * 0.024F * body.hand_scale},
                            std::max<std::size_t>(8U, segments / 2U),
                            4U, shade(skin, 0.98F), 12U,
                            hand_weight.values, hand_weight.count);
            appendEllipsoid(builder, right_hand->world_bind.translation,
                            {body.height * 0.028F * body.hand_scale,
                             body.height * 0.040F * body.hand_scale,
                             body.height * 0.024F * body.hand_scale},
                            std::max<std::size_t>(8U, segments / 2U),
                            4U, shade(skin, 0.98F), 12U,
                            right_hand_weight.values, right_hand_weight.count);
            emit_finger_set(true);
            emit_finger_set(false);
        } else if (index == 3U) {
            const BoneRecord* foot = skeleton.find(BoneId::FootL);
            const BoneRecord* right_foot = skeleton.find(BoneId::FootR);
            if (foot == nullptr || right_foot == nullptr) {
                return false;
            }
            const auto foot_weight = blendWeights({{indexOf(skeleton, BoneId::FootL), 1.0F}});
            const auto right_foot_weight = blendWeights({{indexOf(skeleton, BoneId::FootR), 1.0F}});
            appendEllipsoid(builder, foot->world_bind.translation,
                            {body.height * 0.060F * body.foot_scale,
                             body.height * 0.045F * body.foot_scale,
                             body.height * 0.105F * body.foot_scale},
                            std::max<std::size_t>(10U, segments / 2U),
                            5U, boot, 13U, foot_weight.values, foot_weight.count);
            appendEllipsoid(builder, right_foot->world_bind.translation,
                            {body.height * 0.060F * body.foot_scale,
                             body.height * 0.045F * body.foot_scale,
                             body.height * 0.105F * body.foot_scale},
                            std::max<std::size_t>(10U, segments / 2U),
                            5U, boot, 13U, right_foot_weight.values, right_foot_weight.count);
        }
    }
    return true;
}

void BodySurfaceGenerator::appendEllipsoid(
    AppearanceMeshBuilder& builder, foundation::Vec3 center, foundation::Vec3 radii,
    std::size_t segments, std::size_t rows, foundation::Color color,
    std::uint16_t material_region, const std::array<SkinInfluence, 4U>& influences,
    std::uint8_t influence_count) {
    if (segments < 3U || rows < 2U) {
        return;
    }
    const auto append_oriented = [&builder](std::uint32_t a, std::uint32_t b,
                                             std::uint32_t c, foundation::Vec3 expected) {
        const foundation::Vec3 ab = subtract(builder.mesh().vertices[b].position,
                                              builder.mesh().vertices[a].position);
        const foundation::Vec3 ac = subtract(builder.mesh().vertices[c].position,
                                              builder.mesh().vertices[a].position);
        if (dot(cross(ab, ac), expected) < 0.0F) {
            std::swap(b, c);
        }
        builder.triangle(a, b, c);
    };
    const std::uint32_t south = builder.vertex(
        {center.x, center.y - radii.y, center.z}, {0.0F, -1.0F, 0.0F},
        {0.5F, 0.0F}, color, influences, influence_count, material_region);
    std::vector<std::uint32_t> previous;
    for (std::size_t row = 1U; row < rows; ++row) {
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
        if (previous.empty()) {
            for (std::size_t index = 0U; index < segments; ++index) {
                const std::size_t next = (index + 1U) % segments;
                const float angle = 6.2831853071795864769F * static_cast<float>(index) /
                                    static_cast<float>(segments);
                append_oriented(south, ring[next], ring[index],
                                {cos_phi * std::cos(angle), sin_phi,
                                 cos_phi * std::sin(angle)});
            }
        } else {
            for (std::size_t index = 0U; index < segments; ++index) {
                const std::size_t next = (index + 1U) % segments;
                append_oriented(previous[index], ring[index], previous[next],
                                {cos_phi * std::cos(angleFor(index, segments)),
                                 sin_phi, cos_phi * std::sin(angleFor(index, segments))});
                append_oriented(previous[next], ring[index], ring[next],
                                {cos_phi * std::cos(angleFor(next, segments)),
                                 sin_phi, cos_phi * std::sin(angleFor(next, segments))});
            }
        }
        previous = std::move(ring);
    }
    const std::uint32_t north = builder.vertex(
        {center.x, center.y + radii.y, center.z}, {0.0F, 1.0F, 0.0F},
        {0.5F, 1.0F}, color, influences, influence_count, material_region);
    for (std::size_t index = 0U; index < previous.size(); ++index) {
        const std::size_t next = (index + 1U) % previous.size();
        append_oriented(north, previous[index], previous[next], {0.0F, 1.0F, 0.0F});
    }
}

void BodySurfaceGenerator::appendProfiledLimb(
    AppearanceMeshBuilder& builder, foundation::Vec3 start, foundation::Vec3 end,
    float radius_start, float radius_end, std::size_t segments,
    foundation::Color color, std::uint16_t material_region,
    const std::array<SkinInfluence, 4U>& start_weights, std::uint8_t start_count,
    const std::array<SkinInfluence, 4U>& end_weights, std::uint8_t end_count) {
    const foundation::Vec3 direction = normalized(subtract(end, start));
    const foundation::Vec3 side = normalized({-direction.z, 0.0F, direction.x});
    const foundation::Vec3 up = normalized({direction.y * side.z - direction.z * side.y,
                                             direction.z * side.x - direction.x * side.z,
                                             direction.x * side.y - direction.y * side.x});
    std::vector<std::uint32_t> previous;
    constexpr std::size_t kProfileSections = 5U;
    for (std::size_t section = 0U; section < kProfileSections; ++section) {
        const float t = static_cast<float>(section) /
                        static_cast<float>(kProfileSections - 1U);
        const foundation::Vec3 center = add(start, multiply(subtract(end, start), t));
        const float radius = radius_start + (radius_end - radius_start) * t;
        std::array<SkinInfluence, 8U> candidates{};
        std::size_t candidate_count = 0U;
        for (std::size_t index = 0U; index < start_count; ++index) {
            candidates[candidate_count++] = {
                start_weights[index].bone_index, start_weights[index].weight * (1.0F - t)};
        }
        for (std::size_t index = 0U; index < end_count; ++index) {
            candidates[candidate_count++] = {
                end_weights[index].bone_index, end_weights[index].weight * t};
        }
        const NormalizedInfluences normalized_weights = normalizeTopFour(
            std::span<const SkinInfluence>(candidates.data(), candidate_count));
        std::vector<AppearanceVertexSpec> specs;
        specs.reserve(segments);
        for (std::size_t index = 0U; index < segments; ++index) {
            const float angle = 6.2831853071795864769F * static_cast<float>(index) /
                                static_cast<float>(segments);
            const foundation::Vec3 radial = add(multiply(side, std::cos(angle)),
                                                 multiply(up, std::sin(angle)));
            specs.push_back({add(center, multiply(radial, radius)), radial,
                             {static_cast<float>(index) / static_cast<float>(segments), t},
                             color, material_region,
                             std::span<const SkinInfluence>(normalized_weights.values.data(),
                                                             normalized_weights.count)});
        }
        const geometry::Ring current = builder.appendRing(specs);
        if (!previous.empty()) {
            bridge(builder, {previous}, current);
        }
        previous = current.indices;
    }
}

} // namespace genomes::infantry
