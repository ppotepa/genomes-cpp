#include <genomes/infantry/GearSurfaceGenerator.hpp>

#include <genomes/infantry/EquipmentCatalog.hpp>
#include <genomes/infantry/FaceAnatomy.hpp>
#include <genomes/infantry/InfantryMaterials.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace genomes::infantry {

namespace {

using foundation::Vec2;
using foundation::Vec3;

[[nodiscard]] float mix(float a, float b, float t) noexcept { return a + (b - a) * t; }
[[nodiscard]] std::uint32_t tubeSegments(std::uint32_t detail_level) noexcept {
    return detail_level == 3U ? 8U : detail_level == 1U ? 4U : 6U;
}

struct Quaternion final { float x{0.0F}, y{0.0F}, z{0.0F}, w{1.0F}; };

[[nodiscard]] Vec3 rotate(Vec3 value, Quaternion quaternion) noexcept {
    const Vec3 q{quaternion.x, quaternion.y, quaternion.z};
    const Vec3 twice_cross{
        2.0F * (q.y * value.z - q.z * value.y),
        2.0F * (q.z * value.x - q.x * value.z),
        2.0F * (q.x * value.y - q.y * value.x)};
    return {value.x + quaternion.w * twice_cross.x +
                (q.y * twice_cross.z - q.z * twice_cross.y),
            value.y + quaternion.w * twice_cross.y +
                (q.z * twice_cross.x - q.x * twice_cross.z),
            value.z + quaternion.w * twice_cross.z +
                (q.x * twice_cross.y - q.y * twice_cross.x)};
}

[[nodiscard]] Quaternion fromZ(Vec3 normal) noexcept {
    const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    normal = {normal.x / length, normal.y / length, normal.z / length};
    if (normal.z < -0.999999F) return {0.0F, 1.0F, 0.0F, 0.0F};
    Quaternion result{-normal.y, normal.x, 0.0F, 1.0F + normal.z};
    const float q_length = std::sqrt(result.x * result.x + result.y * result.y +
                                     result.z * result.z + result.w * result.w);
    result.x /= q_length; result.y /= q_length; result.z /= q_length; result.w /= q_length;
    return result;
}

[[nodiscard]] foundation::Color tint(foundation::Color color, float factor) noexcept {
    return {std::clamp(color.r * factor, 0.0F, 1.0F),
            std::clamp(color.g * factor, 0.0F, 1.0F),
            std::clamp(color.b * factor, 0.0F, 1.0F), color.a};
}

[[nodiscard]] float srgbToLinear(float value) noexcept {
    return value <= .04045F ? value / 12.92F
                            : std::pow((value + .055F) / 1.055F, 2.4F);
}

[[nodiscard]] foundation::Color rgb(std::uint32_t hex) noexcept {
    return {srgbToLinear(static_cast<float>((hex >> 16U) & 0xffU) / 255.0F),
            srgbToLinear(static_cast<float>((hex >> 8U) & 0xffU) / 255.0F),
            srgbToLinear(static_cast<float>(hex & 0xffU) / 255.0F), 1.0F};
}

[[nodiscard]] foundation::Color clothColor(const GearArtifact& artifact,
                                            float variant_shade,
                                            float factor = 1.0F) noexcept {
    foundation::Color result{
        std::min(1.0F, artifact.palette.r * variant_shade * factor),
        std::min(1.0F, artifact.palette.g * variant_shade * factor),
        std::min(1.0F, artifact.palette.b * variant_shade * factor), 1.0F};
    const float fade = .018F * artifact.wear;
    result.r += (1.0F - result.r) * fade;
    result.g += (1.0F - result.g) * fade;
    result.b += (1.0F - result.b) * fade;
    return result;
}

[[nodiscard]] std::array<SkinInfluence, 4U> boneWeight(const GearPiece& piece,
                                                         std::uint8_t& count) noexcept {
    count = 1U;
    return {SkinInfluence{static_cast<std::uint16_t>(boneIndex(piece.bone)), 1.0F},
            {}, {}, {}};
}

void appendBox(AppearanceMeshBuilder& builder, const GearPiece& piece, Vec3 center,
               Vec3 dimensions, foundation::Color color, std::uint32_t region,
               std::uint32_t detail_level, const Quaternion* rotation = nullptr,
               std::span<const SkinInfluence> override_weights = {},
               const std::function<NormalizedInfluences(Vec3)>* weight_function = nullptr) {
    constexpr std::array<std::array<int, 2U>, 6U> faces{{
        {{0, 1}}, {{0, -1}}, {{1, 1}}, {{1, -1}}, {{2, 1}}, {{2, -1}}}};
    const Vec3 half{dimensions.x * 0.5F, dimensions.y * 0.5F, dimensions.z * 0.5F};
    const float radius = std::min({half.x, half.y, half.z}) * .4F;
    const Vec3 core{half.x - radius, half.y - radius, half.z - radius};
    const std::uint32_t segments = detail_level == 3U ? 4U : detail_level == 1U ? 1U : 2U;
    std::uint8_t count = 0U;
    const auto influences = boneWeight(piece, count);
    const std::span<const SkinInfluence> weights = override_weights.empty()
        ? std::span<const SkinInfluence>(influences.data(), count) : override_weights;
    const auto component = [](Vec3& value, int axis) -> float& {
        return axis == 0 ? value.x : axis == 1 ? value.y : value.z;
    };
    const auto read = [](Vec3 value, int axis) {
        return axis == 0 ? value.x : axis == 1 ? value.y : value.z;
    };
    for (const auto face : faces) {
        const int axis = face[0], sign = face[1];
        const int u = (axis + 1) % 3, v = (axis + 2) % 3;
        std::vector<std::vector<std::uint32_t>> loops(segments + 1U,
                                                      std::vector<std::uint32_t>(segments + 1U));
        for (std::uint32_t row = 0U; row <= segments; ++row) {
            for (std::uint32_t column = 0U; column <= segments; ++column) {
                Vec3 raw{};
                component(raw, axis) = read(half, axis) * static_cast<float>(sign);
                component(raw, u) = (static_cast<float>(column) / static_cast<float>(segments) * 2.0F - 1.0F) * read(half, u);
                component(raw, v) = (static_cast<float>(row) / static_cast<float>(segments) * 2.0F - 1.0F) * read(half, v);
                Vec3 point{std::clamp(raw.x, -core.x, core.x),
                           std::clamp(raw.y, -core.y, core.y),
                           std::clamp(raw.z, -core.z, core.z)};
                Vec3 normal{raw.x - point.x, raw.y - point.y, raw.z - point.z};
                const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
                normal = {normal.x / length, normal.y / length, normal.z / length};
                point = {point.x + normal.x * radius,
                         point.y + normal.y * radius,
                         point.z + normal.z * radius};
                if (rotation != nullptr) {
                    point = rotate(point, *rotation);
                    normal = rotate(normal, *rotation);
                }
                point = {point.x + center.x, point.y + center.y, point.z + center.z};
                NormalizedInfluences dynamic_weights{};
                std::span<const SkinInfluence> vertex_weights = weights;
                if (weight_function != nullptr) {
                    dynamic_weights = (*weight_function)(point);
                    vertex_weights = std::span<const SkinInfluence>(dynamic_weights.values.data(),
                                                                    dynamic_weights.count);
                }
                loops[row][column] = builder.appendVertex({point, normal,
                    {static_cast<float>(column) / static_cast<float>(segments),
                     static_cast<float>(row) / static_cast<float>(segments)},
                    color, static_cast<std::uint16_t>(region), vertex_weights});
            }
        }
        for (std::uint32_t row = 0U; row < segments; ++row) {
            for (std::uint32_t column = 0U; column < segments; ++column) {
                builder.triangle(loops[row][column], loops[row + 1U][column],
                                 loops[row][column + 1U]);
                builder.triangle(loops[row][column + 1U], loops[row + 1U][column],
                                 loops[row + 1U][column + 1U]);
            }
        }
    }
}

void bridge(AppearanceMeshBuilder& builder, std::span<const std::uint32_t> first,
            std::span<const std::uint32_t> second);

void appendEllipsoid(AppearanceMeshBuilder& builder, const GearPiece& piece, Vec3 center,
                     Vec3 radii, foundation::Color color, std::uint32_t region,
                     std::uint32_t segments = 20U, std::uint32_t rows = 12U,
                     const Quaternion* rotation = nullptr) {
    std::uint8_t count = 0U;
    const auto influences = boneWeight(piece, count);
    const std::span<const SkinInfluence> weights(influences.data(), count);
    const auto vertex = [&builder, color, region, weights](Vec3 position, Vec3 normal,
                                                            Vec2 uv) {
        return builder.appendVertex({position, normal, uv,
                                     color, static_cast<std::uint16_t>(region), weights});
    };
    std::vector<std::vector<std::uint32_t>> rings;
    rings.reserve(rows + 1U);
    for (std::uint32_t row = 0U; row <= rows; ++row) {
        const float phi = -1.57079632679F +
            3.14159265359F * static_cast<float>(row) / static_cast<float>(rows);
        const float cp = std::cos(phi);
        const float sp = std::sin(phi);
        std::vector<std::uint32_t> current;
        current.reserve(segments);
        for (std::uint32_t segment = 0U; segment < segments; ++segment) {
            const float angle = 6.28318530718F * static_cast<float>(segment) /
                                static_cast<float>(segments);
            const float ca = std::cos(angle);
            const float sa = std::sin(angle);
            Vec3 point{radii.x * cp * ca, radii.y * sp, radii.z * cp * sa};
            Vec3 normal{cp * ca / radii.x, sp / radii.y, cp * sa / radii.z};
            const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            normal = {normal.x / length, normal.y / length, normal.z / length};
            if (rotation != nullptr) { point = rotate(point, *rotation); normal = rotate(normal, *rotation); }
            current.push_back(vertex({center.x + point.x, center.y + point.y,
                                      center.z + point.z}, normal,
                                     {static_cast<float>(segment) / static_cast<float>(segments),
                                      static_cast<float>(row) / static_cast<float>(rows)}));
        }
        if (!rings.empty()) bridge(builder, rings.back(), current);
        rings.push_back(std::move(current));
    }
}

void appendRibbon(AppearanceMeshBuilder& builder, const GearPiece& piece,
                  std::span<const Vec3> points, float width,
                  foundation::Color color, std::uint32_t region,
                  Vec3 axis = {1.0F, 0.0F, 0.0F},
                  const std::function<NormalizedInfluences(Vec3)>* weight_function = nullptr) {
    std::uint8_t count = 0U;
    const auto influences = boneWeight(piece, count);
    const std::span<const SkinInfluence> weights(influences.data(), count);
    std::array<std::uint32_t, 2U> previous{};
    bool has_previous = false;
    for (std::size_t index = 0U; index < points.size(); ++index) {
        const Vec3 before = points[index == 0U ? 0U : index - 1U];
        const Vec3 after = points[std::min(index + 1U, points.size() - 1U)];
        Vec3 tangent{after.x - before.x, after.y - before.y, after.z - before.z};
        const float tangent_length = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
        if (tangent_length > 0.0F) tangent = {tangent.x / tangent_length, tangent.y / tangent_length, tangent.z / tangent_length};
        Vec3 normal{axis.y * tangent.z - axis.z * tangent.y,
                    axis.z * tangent.x - axis.x * tangent.z,
                    axis.x * tangent.y - axis.y * tangent.x};
        const float normal_length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (normal_length > 0.0F) normal = {normal.x / normal_length, normal.y / normal_length, normal.z / normal_length};
        NormalizedInfluences dynamic_weights{};
        std::span<const SkinInfluence> vertex_weights = weights;
        if (weight_function != nullptr) {
            dynamic_weights = (*weight_function)(points[index]);
            vertex_weights = std::span<const SkinInfluence>(dynamic_weights.values.data(),
                                                             dynamic_weights.count);
        }
        const std::array<std::uint32_t, 2U> edge{
            builder.appendVertex({{points[index].x - axis.x * width * .5F,
                                   points[index].y - axis.y * width * .5F,
                                   points[index].z - axis.z * width * .5F},
                                  normal, {0.0F, static_cast<float>(index)}, color,
                                  static_cast<std::uint16_t>(region), vertex_weights}),
            builder.appendVertex({{points[index].x + axis.x * width * .5F,
                                   points[index].y + axis.y * width * .5F,
                                   points[index].z + axis.z * width * .5F},
                                  normal, {1.0F, static_cast<float>(index)}, color,
                                  static_cast<std::uint16_t>(region), vertex_weights})};
        if (has_previous) {
            builder.triangle(previous[0], edge[0], previous[1]);
            builder.triangle(previous[1], edge[0], edge[1]);
        }
        previous = edge;
        has_previous = true;
    }
}

[[nodiscard]] NormalizedInfluences neckWeights(const EquipmentFit& fit, float y);

[[nodiscard]] NormalizedInfluences torsoWeights(const EquipmentFit& fit, Vec3 point) {
    const float spine_lower = fit.bind_points[boneIndex(BoneId::SpineLower)].y;
    const float spine_upper = fit.bind_points[boneIndex(BoneId::SpineUpper)].y;
    const float chest = fit.bind_points[boneIndex(BoneId::Chest)].y;
    std::array<SkinInfluence, 2U> values{};
    if (point.y < spine_lower) {
        const float t = std::clamp((point.y - fit.hip_y) /
            std::max(.001F, spine_lower - fit.hip_y), 0.0F, 1.0F);
        const float eased = t * t * (3.0F - 2.0F * t);
        values = {{{static_cast<std::uint16_t>(boneIndex(BoneId::Hips)), 1.0F - eased},
                   {static_cast<std::uint16_t>(boneIndex(BoneId::SpineLower)), eased}}};
    } else if (point.y < spine_upper) {
        const float t = std::clamp((point.y - spine_lower) /
            std::max(.001F, spine_upper - spine_lower), 0.0F, 1.0F);
        const float eased = t * t * (3.0F - 2.0F * t);
        values = {{{static_cast<std::uint16_t>(boneIndex(BoneId::SpineLower)), 1.0F - eased},
                   {static_cast<std::uint16_t>(boneIndex(BoneId::SpineUpper)), eased}}};
    } else if (point.y < chest) {
        const float t = std::clamp((point.y - spine_upper) /
            std::max(.001F, chest - spine_upper), 0.0F, 1.0F);
        const float eased = t * t * (3.0F - 2.0F * t);
        values = {{{static_cast<std::uint16_t>(boneIndex(BoneId::SpineUpper)), 1.0F - eased},
                   {static_cast<std::uint16_t>(boneIndex(BoneId::Chest)), eased}}};
    } else {
        if(point.y>.832F)return neckWeights(fit,point.y);
        values = {{{static_cast<std::uint16_t>(boneIndex(BoneId::Chest)), 1.0F}, {}}};
    }
    return normalizeTopFour(values);
}

[[nodiscard]] NormalizedInfluences neckWeights(const EquipmentFit& fit, float y) {
    const float head_pivot=fit.bind_points[boneIndex(BoneId::Head)].y;
    const float neck_joint=fit.bind_points[boneIndex(BoneId::Neck)].y;
    const auto eased=[](float value){value=std::clamp(value,0.0F,1.0F);return value*value*(3.0F-2.0F*value);};
    const float head=eased((y-(head_pivot-.022F))/.030F);
    const float chest=(1.0F-eased((y-(neck_joint-.010F))/.030F))*(1.0F-head);
    const std::array<SkinInfluence,3U> values{{
        {static_cast<std::uint16_t>(boneIndex(BoneId::Chest)),chest},
        {static_cast<std::uint16_t>(boneIndex(BoneId::Neck)),1.0F-head-chest},
        {static_cast<std::uint16_t>(boneIndex(BoneId::Head)),head}}};
    return normalizeTopFour(values);
}

[[nodiscard]] std::vector<std::uint32_t> appendRing(
    AppearanceMeshBuilder& builder, const GearPiece& piece, Vec3 center,
    Vec3 u, Vec3 v, float radius_x, float radius_z, std::uint32_t segments,
    foundation::Color color, std::uint32_t region, float uv_y = 0.0F,
    std::span<const SkinInfluence> override_weights = {}) {
    std::uint8_t count = 0U;
    const auto influences = boneWeight(piece, count);
    const std::span<const SkinInfluence> weights = override_weights.empty()
        ? std::span<const SkinInfluence>(influences.data(), count) : override_weights;
    std::vector<std::uint32_t> ring;
    ring.reserve(segments);
    for (std::uint32_t index = 0U; index < segments; ++index) {
        const float angle = 6.28318530717958647692F * static_cast<float>(index) /
                            static_cast<float>(segments);
        const float xx = std::cos(angle), zz = std::sin(angle);
        const Vec3 position{center.x + u.x * radius_x * xx + v.x * radius_z * zz,
                            center.y + u.y * radius_x * xx + v.y * radius_z * zz,
                            center.z + u.z * radius_x * xx + v.z * radius_z * zz};
        Vec3 normal{u.x * xx / std::max(radius_x, 1.0e-5F) +
                        v.x * zz / std::max(radius_z, 1.0e-5F),
                    u.y * xx / std::max(radius_x, 1.0e-5F) +
                        v.y * zz / std::max(radius_z, 1.0e-5F),
                    u.z * xx / std::max(radius_x, 1.0e-5F) +
                        v.z * zz / std::max(radius_z, 1.0e-5F)};
        const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        normal = {normal.x / length, normal.y / length, normal.z / length};
        ring.push_back(builder.appendVertex({position, normal,
            {static_cast<float>(index) / static_cast<float>(segments) * 3.0F, uv_y},
            color, static_cast<std::uint16_t>(region), weights}));
    }
    return ring;
}

void appendTubePath(AppearanceMeshBuilder& builder, const GearPiece& piece,
                    std::span<const Vec3> points, float radius,
                    foundation::Color color, std::uint32_t region,
                    std::uint32_t segments,
                    const std::function<NormalizedInfluences(Vec3)>* weight_function = nullptr) {
    std::vector<std::uint32_t> previous;
    for (std::size_t index = 0U; index < points.size(); ++index) {
        const Vec3 delta = index + 1U < points.size()
            ? Vec3{points[index + 1U].x - points[index].x,
                   points[index + 1U].y - points[index].y,
                   points[index + 1U].z - points[index].z}
            : Vec3{points[index].x - points[index - 1U].x,
                   points[index].y - points[index - 1U].y,
                   points[index].z - points[index - 1U].z};
        const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        const Vec3 direction{delta.x / length, delta.y / length, delta.z / length};
        const Vec3 guide = std::abs(direction.z) < .9F ? Vec3{0.0F,0.0F,1.0F}
                                                        : Vec3{0.0F,1.0F,0.0F};
        Vec3 u{direction.y * guide.z - direction.z * guide.y,
               direction.z * guide.x - direction.x * guide.z,
               direction.x * guide.y - direction.y * guide.x};
        const float u_length = std::sqrt(u.x*u.x+u.y*u.y+u.z*u.z);
        u = {u.x/u_length,u.y/u_length,u.z/u_length};
        Vec3 v{u.y * direction.z - u.z * direction.y,
               u.z * direction.x - u.x * direction.z,
               u.x * direction.y - u.y * direction.x};
        NormalizedInfluences dynamic_weights{};
        std::span<const SkinInfluence> weights{};
        if (weight_function != nullptr) {
            dynamic_weights = (*weight_function)(points[index]);
            weights = std::span<const SkinInfluence>(dynamic_weights.values.data(),
                                                      dynamic_weights.count);
        }
        auto current = appendRing(builder,piece,points[index],u,v,radius,radius,
                                  segments,color,region,
                                  static_cast<float>(index)/static_cast<float>(points.size()),
                                  weights);
        if (!previous.empty()) bridge(builder, previous, current);
        previous = std::move(current);
    }
}

void bridge(AppearanceMeshBuilder& builder, std::span<const std::uint32_t> first,
            std::span<const std::uint32_t> second) {
    for (std::size_t index = 0U; index < first.size(); ++index) {
        const std::size_t next = (index + 1U) % first.size();
        builder.triangle(first[index], second[index], first[next]);
        builder.triangle(first[next], second[index], second[next]);
    }
}

void appendCylinder(AppearanceMeshBuilder& builder, const GearPiece& piece,
                    Vec3 first_point, Vec3 second_point, float radius,
                    foundation::Color color, std::uint32_t region,
                    std::uint32_t segments) {
    Vec3 direction{second_point.x-first_point.x,second_point.y-first_point.y,
                   second_point.z-first_point.z};
    const float direction_length=std::sqrt(direction.x*direction.x+direction.y*direction.y+
                                            direction.z*direction.z);
    direction={direction.x/direction_length,direction.y/direction_length,direction.z/direction_length};
    const Vec3 guide=std::abs(direction.z)<.9F?Vec3{0.0F,0.0F,1.0F}:Vec3{0.0F,1.0F,0.0F};
    Vec3 u{direction.y*guide.z-direction.z*guide.y,
           direction.z*guide.x-direction.x*guide.z,
           direction.x*guide.y-direction.y*guide.x};
    const float u_length=std::sqrt(u.x*u.x+u.y*u.y+u.z*u.z);
    u={u.x/u_length,u.y/u_length,u.z/u_length};
    const Vec3 v{u.y*direction.z-u.z*direction.y,
                 u.z*direction.x-u.x*direction.z,
                 u.x*direction.y-u.y*direction.x};
    const auto first=appendRing(builder,piece,first_point,u,v,radius,radius,segments,color,region);
    const auto second=appendRing(builder,piece,second_point,u,v,radius,radius,segments,color,region);
    bridge(builder,first,second);
    std::uint8_t count=0U;const auto influence_array=boneWeight(piece,count);
    const std::span<const SkinInfluence> weights(influence_array.data(),count);
    const auto cap=[&](const std::vector<std::uint32_t>& loop,Vec3 center,Vec3 normal){
        const auto middle=builder.appendVertex({center,normal,{},color,
            static_cast<std::uint16_t>(region),weights});
        for(std::size_t index=0U;index<loop.size();++index)
            builder.triangle(middle,loop[index],loop[(index+1U)%loop.size()]);};
    cap(first,first_point,{-direction.x,-direction.y,-direction.z});
    cap(second,second_point,direction);
}

} // namespace

foundation::Result<AppearanceMesh, foundation::Error> GearSurfaceGenerator::build(
    const GearArtifact& artifact) {
    AppearanceMeshBuilder builder;
    std::vector<AppearanceVertexTag> tags;
    std::unordered_map<std::uint32_t,std::uint32_t> duplicate_sources;
    for (const GearPiece& piece : artifact.pieces) {
        const std::uint32_t first_vertex = static_cast<std::uint32_t>(builder.mesh().vertices.size());
        const auto* definition = EquipmentCatalog::findItem(piece.definition_id);
        const std::string_view style = definition == nullptr ? std::string_view{} : definition->style;
        const Vec3 center = piece.center;
        const EquipmentItem* item = artifact.equipment.item(piece.slot);
        const float variant_shade = item == nullptr ? 1.0F : item->variant.shade;
        const foundation::Color variant_color{
            std::clamp(artifact.palette.r * variant_shade, 0.0F, 1.0F),
            std::clamp(artifact.palette.g * variant_shade, 0.0F, 1.0F),
            std::clamp(artifact.palette.b * variant_shade, 0.0F, 1.0F), 1.0F};
        const foundation::Color dark = tint(variant_color, 0.58F);
        const foundation::Color edge = tint(variant_color, 0.73F);
        switch (piece.slot) {
        case EquipmentSlot::Head:
            {
                const std::uint32_t segments = artifact.detail_level == 3U ? 48U :
                    artifact.detail_level == 1U ? 16U : 32U;
                const std::uint32_t rows = artifact.detail_level == 3U ? 14U :
                    artifact.detail_level == 1U ? 6U : 10U;
                const bool helmet=definition!=nullptr&&definition->kind==EquipmentKind::Helmet;
                const float padding = (helmet?.008F:.0035F) / artifact.fit.height;
                const float extra = (helmet?.017F:(style=="beanie"?.018F:.014F)) /
                                    artifact.fit.height;
                const std::uint32_t shell_material=helmet?1U:0U;
                const float top_y = artifact.fit.face_anatomy.head_sections.back().y /
                                    artifact.fit.height;
                const auto web = tint(variant_color, .58F);
                const auto rubber = tint(rgb(0x292b28U), variant_shade);
                std::uint8_t weight_count = 0U;
                const auto weight_array = boneWeight(piece, weight_count);
                const std::span<const SkinInfluence> weights(weight_array.data(), weight_count);
                std::vector<std::uint32_t> first, previous, last;
                std::vector<std::uint32_t> deferred_inner;
                first.reserve(segments); previous.reserve(segments); last.reserve(segments);
                for (std::uint32_t row = 0U; row < rows; ++row) {
                    const float t = static_cast<float>(row) / static_cast<float>(rows);
                    std::vector<std::uint32_t> loop;
                    loop.reserve(segments);
                    for (std::uint32_t column = 0U; column < segments; ++column) {
                        const float theta = -3.14159265358979323846F +
                            6.28318530717958647692F * static_cast<float>(column) /
                            static_cast<float>(segments);
                        const float bottom = artifact.fit.headBottom(theta);
                        Vec3 point = artifact.fit.facePoint(mix(bottom, top_y - .0001F, t), theta);
                        const float round = std::cos(t * 1.57079632679489661923F);
                        point.x += std::sin(theta) * (padding + (helmet?.004F:.0015F)) * round;
                        point.z += std::cos(theta) * (padding + (helmet?.004F:.0015F)) * round;
                        point.y += extra * std::sin(t * 1.57079632679489661923F);
                        if(style=="beret"){
                            point.x+=.013F*std::sin(t*1.57079632679489661923F);
                            point.y+=.005F*std::sin(theta)*round;
                        }
                        if(style=="patrol"&&t>.45F)
                            point.y-=.006F*std::sin((t-.45F)/.55F*3.14159265358979323846F);
                        const float color_factor = 1.0F +
                            .025F * std::sin(theta * 10.0F + t * 8.0F);
                        const auto color = style == "cover" || !helmet
                            ? clothColor(artifact, variant_shade, color_factor)
                            : tint(edge, color_factor);
                        loop.push_back(builder.appendVertex({point,
                            {std::sin(theta) * round, t, std::cos(theta) * round},
                            {static_cast<float>(column) / static_cast<float>(segments) * 3.0F,
                             t * 4.0F}, color, static_cast<std::uint16_t>(shell_material), weights}));
                    }
                    if (!previous.empty()) bridge(builder, previous, loop); else first = loop;
                    previous = loop; last = std::move(loop);
                }
                Vec3 crown = artifact.fit.facePoint(top_y, 0.0F);
                crown.y += extra;
                if(style=="beret")crown.x+=.013F;
                const auto crown_color=helmet?edge:clothColor(artifact,variant_shade);
                const auto tip = builder.appendVertex({crown, {0.0F, 1.0F, 0.0F}, {},
                    crown_color, static_cast<std::uint16_t>(shell_material), weights});
                for (std::uint32_t column = 0U; column < segments; ++column)
                    builder.triangle(last[column], tip, last[(column + 1U) % segments]);
                std::vector<Vec3> rim;
                rim.reserve(segments + 1U);
                for (const auto index : first) rim.push_back(builder.mesh().vertices[index].position);
                rim.push_back(rim.front());
                appendTubePath(builder, piece, rim, .0018F, helmet?rubber:web,
                               shell_material,
                               tubeSegments(artifact.detail_level));
                std::vector<std::uint32_t> inner;
                inner.reserve(segments);
                for (const auto index : first) {
                    Vec3 point = builder.mesh().vertices[index].position;
                    const float angle = std::atan2(point.x, point.z);
                    point.x -= std::sin(angle) * padding * .7F;
                    point.z -= std::cos(angle) * padding * .7F;
                    point.y += .0007F;
                    inner.push_back(builder.appendVertex({point, {0.0F, 1.0F, 0.0F}, {}, web, 1U, weights}));
                }
                if(helmet)bridge(builder, first, inner);else deferred_inner=std::move(inner);
                if(style=="cap"||style=="patrol"){
                    const float y=artifact.fit.headBottom(0.0F)+.0015F;
                    const auto section=FaceAnatomyEvaluator::sectionAt(
                        artifact.fit.face_anatomy,y*artifact.fit.height);
                    const float radius_x=section.half_width/artifact.fit.height;
                    const float z=artifact.fit.faceFrontZ(0.0F,y);
                    std::array<std::array<std::uint32_t,17U>,5U> grid{};
                    std::array<Vec3,17U> edge_points{};
                    for(std::uint32_t row=0U;row<=4U;++row){const float t=static_cast<float>(row)/4.0F;
                        for(std::uint32_t column=0U;column<=16U;++column){
                            const float x=(static_cast<float>(column)/16.0F*2.0F-1.0F)*radius_x*.90F;
                            const Vec3 point{x,y-.007F*t*t,z+.037F*t*(.73F+.27F*std::cos(x/radius_x*1.57079632679489661923F))-.009F*(x/radius_x)*(x/radius_x)};
                            grid[row][column]=builder.appendVertex({point,{0.0F,1.0F,0.0F},
                                {static_cast<float>(column)/16.0F,t},clothColor(artifact,variant_shade,.90F),0U,weights});
                            if(row==4U)edge_points[column]=point;
                        }}
                    for(std::uint32_t row=0U;row<4U;++row)for(std::uint32_t column=0U;column<16U;++column){
                        builder.triangle(grid[row][column],grid[row+1U][column],grid[row][column+1U]);
                        builder.triangle(grid[row][column+1U],grid[row+1U][column],grid[row+1U][column+1U]);}
                    appendTubePath(builder,piece,edge_points,.0012F,web,0U,
                                   tubeSegments(artifact.detail_level));
                }
                if(style=="boonie"){
                    std::vector<std::uint32_t> prior;
                    std::vector<Vec3> outer;
                    for(std::uint32_t row=0U;row<3U;++row){std::vector<std::uint32_t> loop;loop.reserve(segments);
                        for(std::uint32_t column=0U;column<segments;++column){const float angle=6.28318530717958647692F*static_cast<float>(column)/static_cast<float>(segments);
                            Vec3 point=artifact.fit.facePoint(artifact.fit.headBottom(angle),angle);
                            const float radius=.004F+static_cast<float>(row)*.012F;
                            point.x+=std::sin(angle)*radius;point.y+=-.002F+static_cast<float>(row)*.001F*std::sin(angle*2.0F);point.z+=std::cos(angle)*radius;
                            loop.push_back(builder.appendVertex({point,{0.0F,1.0F,0.0F},{},clothColor(artifact,variant_shade),0U,weights}));
                            if(row==2U)outer.push_back(point);}
                        if(!prior.empty())bridge(builder,prior,loop);prior=std::move(loop);}
                    outer.push_back(outer.front());appendTubePath(builder,piece,outer,.0012F,web,0U,
                        tubeSegments(artifact.detail_level));
                }
                const float jaw_y = artifact.fit.face_anatomy.face.chin.y / artifact.fit.height + .006F;
                if(helmet)for (const float sign : {-1.0F, 1.0F}) {
                    const float angle = sign * 1.57079632679489661923F;
                    Vec3 point = artifact.fit.facePoint(artifact.fit.headBottom(angle), angle);
                    const auto jaw_section = FaceAnatomyEvaluator::sectionAt(
                        artifact.fit.face_anatomy, jaw_y * artifact.fit.height);
                    const auto temple_section = FaceAnatomyEvaluator::sectionAt(
                        artifact.fit.face_anatomy, .904F * artifact.fit.height);
                    const std::array<Vec3, 3U> strap{{
                        {point.x + sign * .005F, point.y, point.z},
                        {sign * temple_section.half_width / artifact.fit.height * 1.03F, .908F, .022F},
                        {sign * jaw_section.half_width / artifact.fit.height * .45F, jaw_y - .004F,
                         artifact.fit.faceFrontZ(sign * .015F, jaw_y) + .002F}}};
                    appendRibbon(builder, piece, strap, .0045F, web, 0U);
                    appendBox(builder, piece, {point.x + sign * .006F, point.y + .005F, point.z},
                              {.006F, .014F, .022F}, rubber, 1U, artifact.detail_level);
                }
                if (helmet&&style == "cover") for (const float sign : {-1.0F, 1.0F}) {
                    std::array<Vec3, 9U> seam{};
                    for (std::uint32_t index = 0U; index < seam.size(); ++index) {
                        const float theta = sign * (.18F + static_cast<float>(index) * .12F);
                        seam[index] = artifact.fit.facePoint(.976F, theta);
                        seam[index].x += std::sin(theta) * .009F;
                        seam[index].y += .009F;
                        seam[index].z += std::cos(theta) * .009F;
                    }
                    appendTubePath(builder, piece, seam, .0008F, web, 0U,
                                   tubeSegments(artifact.detail_level));
                }
                if(!deferred_inner.empty()){
                    std::vector<std::uint32_t> duplicated_first;duplicated_first.reserve(first.size());
                    for(const auto source:first){const auto& vertex=builder.mesh().vertices[source];
                        const auto duplicate=builder.appendVertex({vertex.position,vertex.normal,
                            vertex.uv,vertex.color,1U,std::span<const SkinInfluence>(
                                vertex.influences.data(),vertex.influence_count)});
                        duplicated_first.push_back(duplicate);duplicate_sources.emplace(duplicate,source);}
                    for(std::size_t index=0U;index<duplicated_first.size();++index){
                        const auto next=(index+1U)%duplicated_first.size();
                        builder.appendTriangle(duplicated_first[index],deferred_inner[index],
                                               duplicated_first[next]);
                        builder.appendTriangle(duplicated_first[next],deferred_inner[index],
                                               deferred_inner[next]);
                    }
                }
            }
            break;
        case EquipmentSlot::Face:
            {
                if(definition==nullptr||style=="balaclava")break;
                const auto rubber=tint(rgb(0x292b28U),variant_shade);
                const auto metal=tint(rgb(0x545951U),variant_shade);
                if(definition->kind==EquipmentKind::Eyewear){
                    const bool goggles=style=="goggles";
                    std::array<Vec3,2U> ends{};std::size_t end_index=0U;
                    for(const float sign:{1.0F,-1.0F}){
                        const Vec3 landmark=sign>0.0F?artifact.fit.face_anatomy.face.right_eye:
                                                            artifact.fit.face_anatomy.face.left_eye;
                        const float eye_x=landmark.x/artifact.fit.height;
                        const float eye_y=landmark.y/artifact.fit.height;
                        const float eye_width=.0083F*artifact.fit.face.eye_width_scale;
                        const float eye_height=.0034F*artifact.fit.face.eye_height_scale;
                        const float radius=std::max(eye_width*1.12F,eye_height*1.65F)*.82F;
                        const float eye_z=artifact.fit.faceFrontZ(eye_x,eye_y)-radius*.80F+
                                          artifact.fit.face.eye_depth*.16F;
                        const Vec3 center{eye_x,eye_y,eye_z+radius+.0025F};
                        const float rx=eye_width*(goggles?1.5F:1.25F);
                        const float ry=eye_height*(goggles?1.75F:1.4F);
                        std::array<Vec3,25U> loop{};
                        for(std::uint32_t index=0U;index<=24U;++index){const float angle=
                            static_cast<float>(index)/24.0F*6.28318530717958647692F;
                            const float sine=std::sin(angle);loop[index]={center.x+rx*std::cos(angle),
                                center.y+ry*sine,center.z-.001F*sine*sine};}
                        appendTubePath(builder,piece,loop,goggles?.0018F:.0008F,rubber,1U,
                                       tubeSegments(artifact.detail_level));
                        ends[end_index++]={center.x-sign*rx,center.y,center.z};
                        const Vec3 outer{center.x+sign*rx,center.y,center.z};
                        Vec3 side=artifact.fit.facePoint(eye_y,sign*1.57079632679489661923F);
                        side.x+=sign*.003F;
                        const std::array<Vec3,3U> arm{{outer,
                            {outer.x+sign*.004F,outer.y,outer.z-.010F},side}};
                        appendTubePath(builder,piece,arm,goggles?.0017F:.0008F,dark,1U,
                                       tubeSegments(artifact.detail_level));
                    }
                    appendTubePath(builder,piece,ends,.0009F,metal,1U,
                                   tubeSegments(artifact.detail_level));
                }else if(style=="respirator"){
                    const float nose_base=std::max(
                        artifact.fit.face.eye_y_ratio-.031F*artifact.fit.face.nose_length_scale,
                        artifact.fit.face.mouth_y_ratio+.010F);
                    const float y=nose_base-.006F;
                    const float z=artifact.fit.faceFrontZ(0.0F,y)+.009F;
                    appendBox(builder,piece,{0.0F,y,z},{.038F,.033F,.021F},rubber,1U,
                              artifact.detail_level);
                    for(const float sign:{-1.0F,1.0F}){
                        const Vec3 point{sign*.024F,y-.004F,z};
                        const Vec3 end{point.x,point.y,point.z+.012F};
                        appendCylinder(builder,piece,point,end,.010F,edge,1U,12U);
                        for(int line=-2;line<=2;++line){const float yy=static_cast<float>(line)*.002F;
                            const std::array<Vec3,2U> grille{{{end.x-.006F,end.y+yy,end.z},
                                                              {end.x+.006F,end.y+yy,end.z}}};
                            appendTubePath(builder,piece,grille,.0005F,rubber,1U,
                                           tubeSegments(artifact.detail_level));}
                    }
                }
            }
            break;
        case EquipmentSlot::Neck:
            {
                constexpr std::uint32_t count=32U;
                std::vector<std::uint32_t> previous;
                for(std::uint32_t row=0U;row<=5U;++row){
                    const float y=.847F+static_cast<float>(row)*.0045F;
                    const auto section=FaceAnatomyEvaluator::sectionAt(
                        artifact.fit.face_anatomy,y*artifact.fit.height);
                    const float radius=.003F+std::sin(static_cast<float>(row)/5.0F*
                        3.14159265358979323846F)*.002F;
                    std::vector<std::uint32_t> ring;ring.reserve(count);
                    for(std::uint32_t index=0U;index<count;++index){const float angle=
                        6.28318530717958647692F*static_cast<float>(index)/static_cast<float>(count);
                        const Vec3 point{(section.half_width/artifact.fit.height+radius)*std::cos(angle),
                            y,section.center_z/artifact.fit.height+
                              (section.half_depth/artifact.fit.height+radius)*std::sin(angle)};
                        const auto dynamic=neckWeights(artifact.fit,y);
                        ring.push_back(builder.appendVertex({point,{std::cos(angle),0.0F,std::sin(angle)},
                            {static_cast<float>(index)/static_cast<float>(count)*3.0F,0.0F},
                            clothColor(artifact,variant_shade,row%2U?.87F:1.0F),0U,
                            std::span<const SkinInfluence>(dynamic.values.data(),dynamic.count)}));}
                    if(!previous.empty())bridge(builder,previous,ring);previous=std::move(ring);
                }
                if(style=="scarf"){
                    const Vec3 point=artifact.fit.front(.018F,.807F,.008F);
                    const std::function<NormalizedInfluences(Vec3)> weights=
                        [&artifact](Vec3 p){return torsoWeights(artifact.fit,p);};
                    appendBox(builder,piece,point,{.023F,.072F,.007F},
                              clothColor(artifact,variant_shade),0U,artifact.detail_level,
                              nullptr,{},&weights);
                }
            }
            break;
        case EquipmentSlot::TorsoArmor:
            {
                const float gap = artifact.fit.armor_thickness + .003F;
                const float y0 = artifact.fit.mapTorsoY(style == "heavy" ? .603F : .635F);
                const float y1 = artifact.fit.mapTorsoY(.793F);
                const float width_factor = style == "heavy" ? .94F : .79F;
                const auto panel_color = clothColor(artifact, variant_shade, .87F);
                const std::function<NormalizedInfluences(Vec3)> weights =
                    [&artifact](Vec3 point) { return torsoWeights(artifact.fit, point); };
                const auto panel = [&](bool back) {
                    constexpr std::uint32_t rows = 8U, columns = 12U;
                    const float sign = back ? -1.0F : 1.0F;
                    std::array<std::array<std::uint32_t, columns + 1U>, rows + 1U> grid{};
                    std::array<std::array<Vec3, columns + 1U>, rows + 1U> positions{};
                    for (std::uint32_t row = 0U; row <= rows; ++row) {
                        const float t = static_cast<float>(row) / static_cast<float>(rows);
                        const float y = mix(y0, y1, t);
                        const float radius_x = artifact.fit.profile(y).x;
                        const float taper = 1.0F - .16F * std::max(0.0F, (t - .68F) / .32F);
                        for (std::uint32_t column = 0U; column <= columns; ++column) {
                            const float x = (static_cast<float>(column) /
                                static_cast<float>(columns) * 2.0F - 1.0F) *
                                radius_x * width_factor * taper;
                            Vec3 position = artifact.fit.front(x, y, gap);
                            position.z *= sign;
                            Vec3 normal{x, 0.0F, sign * .2F};
                            const float length = std::sqrt(normal.x*normal.x+normal.z*normal.z);
                            normal = {normal.x/length,0.0F,normal.z/length};
                            const auto vertex_weights = weights(position);
                            grid[row][column] = builder.appendVertex({position,normal,
                                {static_cast<float>(column)/static_cast<float>(columns)*3.0F,
                                 t*5.0F},panel_color,0U,
                                std::span<const SkinInfluence>(vertex_weights.values.data(),
                                                               vertex_weights.count)});
                            positions[row][column] = position;
                        }
                    }
                    for (std::uint32_t row=0U;row<rows;++row)for(std::uint32_t column=0U;column<columns;++column){
                        builder.triangle(grid[row][column],grid[row+1U][column],grid[row][column+1U]);
                        builder.triangle(grid[row][column+1U],grid[row+1U][column],grid[row+1U][column+1U]);
                    }
                    std::array<Vec3,columns+1U> boundary{};
                    for(const std::uint32_t row:{0U,rows}){
                        for(std::uint32_t column=0U;column<=columns;++column)boundary[column]=positions[row][column];
                        appendTubePath(builder,piece,boundary,.0015F,dark,0U,
                                       tubeSegments(artifact.detail_level),&weights);
                    }
                    std::array<Vec3,rows+1U> side_boundary{};
                    for(const std::uint32_t column:{0U,columns}){
                        for(std::uint32_t row=0U;row<=rows;++row)side_boundary[row]=positions[row][column];
                        appendTubePath(builder,piece,side_boundary,.0015F,dark,0U,
                                       tubeSegments(artifact.detail_level),&weights);
                    }
                };
                panel(false); panel(true);
                const float strap_y0=artifact.fit.mapTorsoY(.665F);
                const float strap_y1=artifact.fit.mapTorsoY(.790F);
                const float shoulder_y=artifact.fit.bind_points[boneIndex(BoneId::UpperArmL)].y+.012F;
                for(const float sign:{-1.0F,1.0F}){
                    const float x=sign*artifact.fit.profile(strap_y1).x*.65F;
                    std::array<Vec3,16U> points{};std::size_t at=0U;
                    for(std::uint32_t i=0U;i<7U;++i){const float y=mix(strap_y0,strap_y1,static_cast<float>(i)/6.0F);points[at++]=artifact.fit.front(x*.86F,y,gap+.001F);}
                    points[at++]={x,shoulder_y,.02F};points[at++]={x,shoulder_y,-.02F};
                    for(std::uint32_t i=0U;i<7U;++i){const float y=mix(strap_y1,strap_y0,static_cast<float>(i)/6.0F);auto p=artifact.fit.front(x*.86F,y,gap+.001F);p.z=-p.z;points[at++]=p;}
                    appendRibbon(builder,piece,points,.022F,dark,0U,{1.0F,0.0F,0.0F},&weights);
                }
                const float side_y=artifact.fit.mapTorsoY(.673F);
                const auto side_profile=artifact.fit.profile(side_y);
                for(const float sign:{-1.0F,1.0F})
                    appendBox(builder,piece,{sign*(side_profile.x+.004F),side_y,0.0F},
                              {.009F,.037F,side_profile.y*1.7F},dark,0U,
                              artifact.detail_level,nullptr,{},&weights);
                if(style=="heavy"){
                    const float lower_y=artifact.fit.mapTorsoY(.603F);
                    const auto position=artifact.fit.front(0.0F,lower_y,gap+.004F);
                    appendBox(builder,piece,position,{artifact.fit.profile(lower_y).x*1.25F,.040F,.009F},
                              edge,0U,artifact.detail_level,nullptr,{},&weights);
                }
            }
            break;
        case EquipmentSlot::ChestRig:
            {
                const float y = artifact.fit.mapTorsoY(.711F);
                const float gap = artifact.fit.armor_thickness + .010F;
                if(artifact.fit.armor_thickness==0.0F){
                    const std::function<NormalizedInfluences(Vec3)> strap_weights=
                        [&artifact](Vec3 point){return torsoWeights(artifact.fit,point);};
                    const float y0=artifact.fit.mapTorsoY(.665F),y1=artifact.fit.mapTorsoY(.790F);
                    const float shoulder_y=artifact.fit.bind_points[boneIndex(BoneId::UpperArmL)].y+.012F;
                    for(const float sign:{-1.0F,1.0F}){const float x=sign*artifact.fit.profile(y1).x*.65F;
                        std::array<Vec3,16U> points{};std::size_t at=0U;
                        for(std::uint32_t index=0U;index<7U;++index){const float yy=mix(y0,y1,static_cast<float>(index)/6.0F);points[at++]=artifact.fit.front(x*.86F,yy,gap);}
                        points[at++]={x,shoulder_y,.02F};points[at++]={x,shoulder_y,-.02F};
                        for(std::uint32_t index=0U;index<7U;++index){const float yy=mix(y1,y0,static_cast<float>(index)/6.0F);auto p=artifact.fit.front(x*.86F,yy,gap);p.z=-p.z;points[at++]=p;}
                        appendRibbon(builder,piece,points,.013F,dark,0U,{1.0F,0.0F,0.0F},&strap_weights);
                    }
                }
                const std::uint32_t pouch_count = definition->visual.count;
                const float width = artifact.fit.profile(y).x * 1.40F /
                                    static_cast<float>(pouch_count);
                const float height = style == "ammo" ? .066F : .056F;
                for (std::uint32_t index = 0U; index < pouch_count; ++index) {
                    const float x = (static_cast<float>(index) -
                        (static_cast<float>(pouch_count) - 1.0F) * .5F) * (width + .003F);
                    const Vec3 pouch_center = artifact.fit.front(x, y, gap + .010F);
                    Vec3 normal{x * .8F, 0.0F, 1.0F};
                    const float normal_length = std::sqrt(normal.x * normal.x + normal.z * normal.z);
                    normal = {normal.x / normal_length, 0.0F, normal.z / normal_length};
                    const Quaternion rotation = fromZ(normal);
                    const NormalizedInfluences pouch_weights = torsoWeights(artifact.fit, pouch_center);
                    const std::span<const SkinInfluence> weights(
                        pouch_weights.values.data(), pouch_weights.count);
                    const auto offset = [&pouch_center, &rotation](float ox, float oy, float oz) {
                        const Vec3 transformed = rotate({ox, oy, oz}, rotation);
                        return Vec3{pouch_center.x + transformed.x,
                                    pouch_center.y + transformed.y,
                                    pouch_center.z + transformed.z};
                    };
                    const Vec3 size{width, height, .024F};
                    appendBox(builder, piece, pouch_center, size, piece.color, 0U,
                              artifact.detail_level, &rotation, weights);
                    appendBox(builder, piece, offset(0.0F, size.y * .30F, size.z * .48F),
                              {size.x * 1.025F, size.y * .24F, .003F}, edge, 0U,
                              artifact.detail_level, &rotation, weights);
                    const float front = size.z * .54F;
                    appendBox(builder, piece, offset(0.0F, 0.0F, front),
                              {.008F, size.y * .50F, .003F}, dark, 0U,
                              artifact.detail_level, &rotation, weights);
                    appendBox(builder, piece, offset(0.0F, -size.y * .12F, front + .001F),
                              {.014F, .010F, .004F},
                              tint(rgb(0x292b28U), variant_shade), 1U,
                              artifact.detail_level, &rotation, weights);
                    if (style == "medical" || style == "tools") {
                        const auto light = tint(rgb(0xb9b097U), variant_shade * .6F);
                        for (const float sign : {-1.0F, 1.0F})
                            appendBox(builder, piece,
                                      offset(sign * size.x * .22F, -size.y * .06F,
                                             front + .001F),
                                      {.004F, size.y * .55F, .002F}, light, 0U,
                                      artifact.detail_level, &rotation, weights);
                    }
                }
                const auto profile = artifact.fit.profile(y);
                const std::array<Vec3,3U> line{{
                    artifact.fit.front(-profile.x * .78F, y - .035F, gap),
                    artifact.fit.front(0.0F, y - .035F, gap),
                    artifact.fit.front(profile.x * .78F, y - .035F, gap)}};
                const std::function<NormalizedInfluences(Vec3)> weights =
                    [&artifact](Vec3 point) { return torsoWeights(artifact.fit, point); };
                appendRibbon(builder, piece, line, .010F, dark, 0U,
                             {0.0F, 1.0F, 0.0F}, &weights);
            }
            break;
        case EquipmentSlot::Legs:
            if (definition == nullptr || !definition->visual.pads) break;
            for (const bool left : {true, false}) {
                GearPiece leg_piece = piece;
                leg_piece.bone = left ? BoneId::ShinL : BoneId::ShinR;
                const auto shin = artifact.fit.bind_points[boneIndex(leg_piece.bone)];
                const Vec3 knee{shin.x, shin.y + .009F,
                                shin.z + .038F * artifact.fit.body.leg_thickness_scale};
                appendBox(builder, leg_piece, knee,
                          {.047F * artifact.fit.body.leg_thickness_scale, .058F, .009F},
                          tint(rgb(0x292b28U), variant_shade), 1U,
                          artifact.detail_level);
                for (int ridge = -1; ridge <= 1; ++ridge) {
                    appendBox(builder, leg_piece,
                              {knee.x, knee.y + static_cast<float>(ridge) * .014F,
                               knee.z + .006F},
                              {.036F, .005F, .003F}, edge, 1U,
                              artifact.detail_level);
                }
            }
            break;
        case EquipmentSlot::Belt:
            {
                const float y = artifact.fit.hip_y + .009F;
                const auto profile = artifact.fit.profile(y);
                const float radius_x = profile.x + .004F;
                const float radius_z = profile.y + .004F;
                const float height = definition != nullptr && definition->visual.style == "utility"
                    ? .014F : .009F;
                const auto lower = appendRing(builder, piece,
                    {0.0F, y - height * .5F, 0.0F}, {1.0F, 0.0F, 0.0F},
                    {0.0F, 0.0F, 1.0F}, radius_x, radius_z, 48U, dark, 0U);
                const auto upper = appendRing(builder, piece,
                    {0.0F, y + height * .5F, 0.0F}, {1.0F, 0.0F, 0.0F},
                    {0.0F, 0.0F, 1.0F}, radius_x, radius_z, 48U, dark, 0U);
                bridge(builder, lower, upper);
                appendBox(builder, piece, {0.0F, y, radius_z + .002F},
                          {.024F, height * .85F, .006F},
                          tint(rgb(0x292b28U), variant_shade), 1U,
                          artifact.detail_level);
            }
            break;
        case EquipmentSlot::Back:
            {
                const Vec3 d=artifact.fit.pack_dimensions;
                const auto& socket=artifact.fit.socket(EquipmentSocketId::BackCenter);
                const Vec3 pack_center{socket.position.x,socket.position.y,
                                       socket.position.z-d.z*.5F};
                appendBox(builder,piece,pack_center,d,piece.color,0U,artifact.detail_level);
                appendBox(builder,piece,{pack_center.x,pack_center.y+d.y*.39F,pack_center.z-.002F},
                          {d.x*1.03F,d.y*.20F,d.z*1.04F},edge,0U,artifact.detail_level);
                const Vec3 back{pack_center.x,pack_center.y,pack_center.z-d.z*.5F-.004F};
                const auto pouch=[&](Vec3 pouch_center,Vec3 size,Vec3 normal,std::string_view pouch_style){
                    const Quaternion rotation=fromZ(normal);
                    const auto offset=[&](float x,float y,float z){const auto p=rotate({x,y,z},rotation);return Vec3{pouch_center.x+p.x,pouch_center.y+p.y,pouch_center.z+p.z};};
                    appendBox(builder,piece,pouch_center,size,piece.color,0U,artifact.detail_level,&rotation);
                    appendBox(builder,piece,offset(0.0F,size.y*.30F,size.z*.48F),
                              {size.x*1.025F,size.y*.24F,.003F},edge,0U,artifact.detail_level,&rotation);
                    const float front=size.z*.54F;
                    appendBox(builder,piece,offset(0.0F,0.0F,front),{.008F,size.y*.50F,.003F},dark,0U,artifact.detail_level,&rotation);
                    appendBox(builder,piece,offset(0.0F,-size.y*.12F,front+.001F),{.014F,.010F,.004F},
                              tint(rgb(0x292b28U),variant_shade),1U,artifact.detail_level,&rotation);
                    if(pouch_style=="medical"||pouch_style=="tools")for(const float sign:{-1.0F,1.0F})
                        appendBox(builder,piece,offset(sign*size.x*.22F,-size.y*.06F,front+.001F),
                                  {.004F,size.y*.55F,.002F},tint(rgb(0xb9b097U),variant_shade*.6F),0U,
                                  artifact.detail_level,&rotation);
                };
                for(const float sign:{-1.0F,1.0F}){
                    Vec3 normal{sign,0.0F,-.3F};const float length=std::sqrt(1.09F);normal={normal.x/length,0.0F,normal.z/length};
                    pouch({pack_center.x+sign*d.x*.53F,pack_center.y-d.y*.12F,pack_center.z-.006F},
                          {d.x*.28F,d.y*.50F,d.z*.68F},normal,
                          style=="medical"?std::string_view{"medical"}:std::string_view{"utility"});
                }
                const std::uint32_t front_pouches=style=="medical"?2U:1U;
                for(std::uint32_t index=0U;index<front_pouches;++index)
                    pouch({back.x+(static_cast<float>(index)-(static_cast<float>(front_pouches)-1.0F)*.5F)*d.x*.45F,
                           back.y-d.y*.12F,back.z-.008F},
                          {d.x*.72F/static_cast<float>(front_pouches),d.y*.44F,.021F},
                          {0.0F,0.0F,-1.0F},style);
                for(const float sign:{-1.0F,1.0F}){
                    const float x=sign*d.x*.29F;
                    const std::array<Vec3,3U> strap{{
                        {pack_center.x+x,pack_center.y+d.y*.42F,pack_center.z-d.z*.54F},
                        {pack_center.x+x,pack_center.y,pack_center.z-d.z*.54F},
                        {pack_center.x+x,pack_center.y-d.y*.42F,pack_center.z-d.z*.54F}}};
                    appendRibbon(builder,piece,strap,.009F,dark,0U);
                    appendBox(builder,piece,{pack_center.x+x,pack_center.y-d.y*.16F,pack_center.z-d.z*.55F},
                              {.016F,.013F,.006F},tint(rgb(0x292b28U),variant_shade),1U,artifact.detail_level);
                }
                const std::array<Vec3,4U> handle{{
                    {pack_center.x-.019F,pack_center.y+d.y*.5F,pack_center.z},
                    {pack_center.x-.016F,pack_center.y+d.y*.55F,pack_center.z},
                    {pack_center.x+.016F,pack_center.y+d.y*.55F,pack_center.z},
                    {pack_center.x+.019F,pack_center.y+d.y*.5F,pack_center.z}}};
                appendTubePath(builder,piece,handle,.0022F,dark,0U,
                               tubeSegments(artifact.detail_level));
                const std::function<NormalizedInfluences(Vec3)> weights=
                    [&artifact](Vec3 point){return torsoWeights(artifact.fit,point);};
                const float y0=artifact.fit.mapTorsoY(.665F),y1=artifact.fit.mapTorsoY(.790F);
                const float shoulder_y=artifact.fit.bind_points[boneIndex(BoneId::UpperArmL)].y+.012F;
                for(const float sign:{-1.0F,1.0F}){
                    const float x=sign*artifact.fit.profile(y1).x*.65F;std::array<Vec3,16U> points{};std::size_t at=0U;
                    for(std::uint32_t i=0U;i<7U;++i){const float y=mix(y0,y1,static_cast<float>(i)/6.0F);points[at++]=artifact.fit.front(x*.86F,y,artifact.fit.armor_thickness+.011F);}
                    points[at++]={x,shoulder_y,.02F};points[at++]={x,shoulder_y,-.02F};
                    for(std::uint32_t i=0U;i<7U;++i){const float y=mix(y1,y0,static_cast<float>(i)/6.0F);auto p=artifact.fit.front(x*.86F,y,artifact.fit.armor_thickness+.011F);p.z=-p.z;points[at++]=p;}
                    appendRibbon(builder,piece,points,.015F,dark,0U,{1.0F,0.0F,0.0F},&weights);
                }
                if(definition!=nullptr&&definition->visual.roll){
                    const float y=pack_center.y-d.y*.49F-.028F;
                    appendCylinder(builder,piece,{-d.x*.6F,y,pack_center.z},
                                   {d.x*.6F,y,pack_center.z},.026F,
                                   clothColor(artifact,variant_shade,.80F),0U,16U);
                    for(const float sign:{-1.0F,1.0F})
                        appendCylinder(builder,piece,{sign*d.x*.34F-.003F,y,pack_center.z},
                                       {sign*d.x*.34F+.003F,y,pack_center.z},.027F,
                                       dark,0U,16U);
                }
                if(style=="radio"){
                    const auto rubber=tint(rgb(0x292b28U),variant_shade);
                    const auto metal=tint(rgb(0x545951U),variant_shade);
                    appendBox(builder,piece,{pack_center.x+.012F,pack_center.y+d.y*.5F+.008F,
                              pack_center.z},{d.x*.72F,.020F,d.z*.80F},rubber,1U,
                              artifact.detail_level);
                    const Vec3 point{pack_center.x-d.x*.30F,pack_center.y+d.y*.52F,
                                     pack_center.z-d.z*.12F};
                    appendCylinder(builder,piece,point,
                        {point.x+.016F,point.y+.23F,point.z-.012F},.0014F,metal,2U,10U);
                    appendCylinder(builder,piece,point,
                        {point.x+.002F,point.y+.033F,point.z-.002F},.0035F,rubber,1U,10U);
                    const float cable_y=artifact.fit.mapTorsoY(.77F);
                    const std::array<Vec3,3U> cable{{
                        {point.x+.033F,point.y,point.z},
                        {pack_center.x+d.x*.55F,pack_center.y+d.y*.4F,pack_center.z+.025F},
                        artifact.fit.front(artifact.fit.profile(cable_y).x*.7F,cable_y,
                                           .016F+artifact.fit.armor_thickness)}};
                    appendTubePath(builder,piece,cable,.0015F,rubber,1U,
                                   tubeSegments(artifact.detail_level),&weights);
                }
                if(style=="engineer"){
                    const Vec3 point{back.x-d.x*.30F,back.y+.005F,back.z-.03F};
                    appendCylinder(builder,piece,{point.x,point.y-.05F,point.z},
                                   {point.x,point.y+.065F,point.z},.0038F,edge,1U,10U);
                    appendBox(builder,piece,{point.x,point.y-.071F,point.z},
                              {.035F,.045F,.004F},tint(rgb(0x545951U),variant_shade),2U,
                              artifact.detail_level);
                    appendCylinder(builder,piece,{point.x-.012F,point.y+.079F,point.z},
                                   {point.x+.012F,point.y+.079F,point.z},.0035F,
                                   tint(rgb(0x292b28U),variant_shade),1U,10U);
                }
            }
            break;
        case EquipmentSlot::LeftHip:
        case EquipmentSlot::RightHip:
        case EquipmentSlot::LeftThigh:
        case EquipmentSlot::RightThigh:
        case EquipmentSlot::Utility1:
        case EquipmentSlot::Utility2:
        case EquipmentSlot::Utility3:
            {
                const auto socket_id = piece.slot == EquipmentSlot::LeftHip ? EquipmentSocketId::HipL :
                    piece.slot == EquipmentSlot::RightHip ? EquipmentSocketId::HipR :
                    piece.slot == EquipmentSlot::LeftThigh ? EquipmentSocketId::ThighL :
                    piece.slot == EquipmentSlot::RightThigh ? EquipmentSocketId::ThighR :
                    piece.slot == EquipmentSlot::Utility1 ? EquipmentSocketId::WaistFront :
                    piece.slot == EquipmentSlot::Utility2 ? EquipmentSocketId::WaistBack :
                    EquipmentSocketId::ChestLeft;
                const Vec3 socket_normal = artifact.fit.socket(socket_id).normal;
                const Quaternion rotation = fromZ(socket_normal);
                const auto offset = [&center, &rotation](float x, float y, float z) {
                    const Vec3 transformed = rotate({x, y, z}, rotation);
                    return Vec3{center.x + transformed.x, center.y + transformed.y,
                                center.z + transformed.z};
                };
                const foundation::Color rubber = tint(rgb(0x292b28U), variant_shade);
                if(style=="binoculars"){
                    for(const float sign:{-1.0F,1.0F}){
                        const Vec3 first=offset(sign*.012F,0.0F,0.0F);
                        const Vec3 delta=rotate({0.0F,-.030F,.003F},rotation);
                        appendCylinder(builder,piece,first,
                            {first.x+delta.x,first.y+delta.y,first.z+delta.z},.009F,
                            rubber,1U,12U);
                    }
                    appendBox(builder,piece,center,{.028F,.010F,.012F},
                              tint(rgb(0x545951U),variant_shade),1U,
                              artifact.detail_level,&rotation);
                    break;
                }
                if(style=="radio"){
                    appendBox(builder,piece,center,{.020F,.037F,.016F},rubber,1U,
                              artifact.detail_level,&rotation);
                    const Vec3 top=offset(-.007F,.018F,0.0F);
                    const Vec3 antenna_delta=rotate({0.0F,.045F,0.0F},rotation);
                    appendCylinder(builder,piece,top,
                        {top.x+antenna_delta.x,top.y+antenna_delta.y,top.z+antenna_delta.z},
                        .0012F,tint(rgb(0x545951U),variant_shade),2U,10U);
                    for(std::uint32_t index=0U;index<4U;++index)
                        appendBox(builder,piece,offset(0.0F,.007F-static_cast<float>(index)*.004F,.009F),
                                  {.014F,.0015F,.001F},edge,1U,artifact.detail_level,&rotation);
                    break;
                }
                if (style == "canteen") {
                    appendEllipsoid(builder, piece, center, {.025F, .037F, .018F},
                                    edge, 0U, 12U, 8U, &rotation);
                    appendBox(builder, piece, offset(0.0F, .037F, 0.0F),
                              {.016F, .013F, .015F}, rubber, 1U,
                              artifact.detail_level, &rotation);
                    const std::array<Vec3, 2U> line{{offset(-.020F, .020F, .017F),
                                                     offset(.020F, .020F, .017F)}};
                    appendRibbon(builder, piece, line, .006F, dark, 0U);
                    break;
                }
                Vec3 pouch_size = style == "map" ? Vec3{.050F, .057F, .012F}
                                                 : Vec3{.043F, .057F, .025F};
                pouch_size = {pouch_size.x * item->variant.size,
                              pouch_size.y * item->variant.size,
                              pouch_size.z * item->variant.size};
                appendBox(builder, piece, center, pouch_size, piece.color, 0U,
                          artifact.detail_level, &rotation);
                appendBox(builder, piece,
                          offset(0.0F, pouch_size.y * .30F, pouch_size.z * .48F),
                          {pouch_size.x * 1.025F, pouch_size.y * .24F, .003F},
                          edge, 0U, artifact.detail_level, &rotation);
                const float front = pouch_size.z * .54F;
                appendBox(builder, piece, offset(0.0F, 0.0F, front),
                          {.008F, pouch_size.y * .50F, .003F}, dark, 0U,
                          artifact.detail_level, &rotation);
                appendBox(builder, piece, offset(0.0F, -pouch_size.y * .12F, front + .001F),
                          {.014F, .010F, .004F}, rubber, 1U,
                          artifact.detail_level, &rotation);
                if (style == "medical" || style == "tools") {
                    const foundation::Color light = tint(rgb(0xb9b097U), variant_shade * .6F);
                    for (const float sign : {-1.0F, 1.0F})
                        appendBox(builder, piece,
                                  offset(sign * pouch_size.x * .22F,
                                         -pouch_size.y * .06F, front + .001F),
                                  {.004F, pouch_size.y * .55F, .002F}, light, 0U,
                                  artifact.detail_level, &rotation);
                }
                if (piece.slot == EquipmentSlot::LeftThigh ||
                    piece.slot == EquipmentSlot::RightThigh) {
                    const bool left = piece.slot == EquipmentSlot::LeftThigh;
                    const BoneId thigh = left ? BoneId::ThighL : BoneId::ThighR;
                    GearPiece strap_piece = piece; strap_piece.bone = thigh;
                    Vec3 strap_center = artifact.fit.bind_points[boneIndex(thigh)];
                    strap_center.y = center.y;
                    const float radius = .055F * artifact.fit.body.leg_thickness_scale + .003F;
                    const auto one = appendRing(builder, strap_piece,
                        {strap_center.x, strap_center.y - .004F, strap_center.z},
                        {1.0F,0.0F,0.0F},{0.0F,0.0F,1.0F},radius,radius,32U,dark,0U);
                    const auto two = appendRing(builder, strap_piece,
                        {strap_center.x, strap_center.y + .004F, strap_center.z},
                        {1.0F,0.0F,0.0F},{0.0F,0.0F,1.0F},radius,radius,32U,dark,0U);
                    bridge(builder, one, two);
                }
            }
            break;
        case EquipmentSlot::PrimaryWeapon:
            {
                const std::array<Vec3, 2U> points{{
                    {center.x - .016F, center.y + .028F, center.z + .003F},
                    {center.x + .016F, center.y + .028F, center.z + .003F}}};
                appendRibbon(builder, piece, points, .009F, dark, 0U);
            }
            break;
        case EquipmentSlot::SecondaryWeapon:
        case EquipmentSlot::MeleeWeapon:
        case EquipmentSlot::Throwable:
            {
                const foundation::Color rubber=tint(rgb(0x292b28U),variant_shade);
                if(style=="knife"){
                    appendBox(builder,piece,center,{.020F,.052F,.012F},rubber,1U,
                              artifact.detail_level);break;
                }
                const auto socket_id=style=="sidearm"?EquipmentSocketId::WeaponHip:
                                     EquipmentSocketId::WaistFront;
                const Quaternion rotation=fromZ(artifact.fit.socket(socket_id).normal);
                const Vec3 size=style=="sidearm"?Vec3{.030F,.057F,.018F}:
                                                    Vec3{.033F,.029F,.017F};
                const auto offset=[&](float x,float y,float z){const auto p=rotate({x,y,z},rotation);
                    return Vec3{center.x+p.x,center.y+p.y,center.z+p.z};};
                appendBox(builder,piece,center,size,piece.color,0U,artifact.detail_level,&rotation);
                appendBox(builder,piece,offset(0.0F,size.y*.30F,size.z*.48F),
                          {size.x*1.025F,size.y*.24F,.003F},edge,0U,
                          artifact.detail_level,&rotation);
                const float front=size.z*.54F;
                appendBox(builder,piece,offset(0.0F,0.0F,front),
                          {.008F,size.y*.50F,.003F},dark,0U,artifact.detail_level,&rotation);
                appendBox(builder,piece,offset(0.0F,-size.y*.12F,front+.001F),
                          {.014F,.010F,.004F},rubber,1U,artifact.detail_level,&rotation);
            }
            break;
        default:
            break;
        }
        const std::uint32_t end_vertex = static_cast<std::uint32_t>(builder.mesh().vertices.size());
        if (end_vertex > first_vertex) {
            AppearanceVertexTag tag{};
            tag.name = "gear." + std::string(
                EquipmentCatalog::slots()[equipmentSlotIndex(piece.slot)].identifier);
            tag.vertices.reserve(end_vertex - first_vertex);
            for (std::uint32_t vertex = first_vertex; vertex < end_vertex; ++vertex)
                if(!duplicate_sources.contains(vertex))tag.vertices.push_back(vertex);
            tags.push_back(std::move(tag));
        }
    }
    for (auto& vertex : builder.mesh().vertices) {
        vertex.position.x *= artifact.fit.height;
        vertex.position.y *= artifact.fit.height;
        vertex.position.z *= artifact.fit.height;
    }
    AppearanceMesh mesh = std::move(builder).finalize();
    if(mesh.vertices.empty()){
        mesh.minimum={};mesh.maximum={};mesh.sphere_center={};mesh.sphere_radius=0.0F;
        mesh.tags=std::move(tags);
        return foundation::Result<AppearanceMesh,foundation::Error>::success(std::move(mesh));
    }
    mesh.minimum = {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
                    std::numeric_limits<float>::infinity()};
    mesh.maximum = {-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                    -std::numeric_limits<float>::infinity()};
    for (const auto& vertex : mesh.vertices) {
        mesh.minimum.x = std::min(mesh.minimum.x, vertex.position.x);
        mesh.minimum.y = std::min(mesh.minimum.y, vertex.position.y);
        mesh.minimum.z = std::min(mesh.minimum.z, vertex.position.z);
        mesh.maximum.x = std::max(mesh.maximum.x, vertex.position.x);
        mesh.maximum.y = std::max(mesh.maximum.y, vertex.position.y);
        mesh.maximum.z = std::max(mesh.maximum.z, vertex.position.z);
    }
    mesh.sphere_center = {(mesh.minimum.x + mesh.maximum.x) * .5F,
                          (mesh.minimum.y + mesh.maximum.y) * .5F,
                          (mesh.minimum.z + mesh.maximum.z) * .5F};
    float radius_squared = 0.0F;
    for (const auto& vertex : mesh.vertices) {
        const float x=vertex.position.x-mesh.sphere_center.x;
        const float y=vertex.position.y-mesh.sphere_center.y;
        const float z=vertex.position.z-mesh.sphere_center.z;
        radius_squared=std::max(radius_squared,x*x+y*y+z*z);
    }
    mesh.sphere_radius=std::sqrt(radius_squared);
    std::array<std::vector<std::uint32_t>, 3U> material_indices{};
    for (std::size_t triangle = 0U; triangle < mesh.indices.size() / 3U; ++triangle) {
        const std::size_t offset = triangle * 3U;
        const auto material = std::min<std::size_t>(2U,
            mesh.vertices[mesh.indices[offset]].material_region);
        material_indices[material].insert(material_indices[material].end(),
            {mesh.indices[offset], mesh.indices[offset + 1U], mesh.indices[offset + 2U]});
    }
    mesh.indices.clear();
    mesh.groups.clear();
    for (std::uint16_t material = 0U; material < material_indices.size(); ++material) {
        const auto& source = material_indices[material];
        if (source.empty()) continue;
        mesh.groups.push_back({static_cast<std::uint32_t>(mesh.indices.size()),
                               static_cast<std::uint32_t>(source.size()), material});
        mesh.indices.insert(mesh.indices.end(), source.begin(), source.end());
    }
    const std::vector<AppearanceVertex> hinted_vertices = mesh.vertices;
    struct EdgeEntry final { std::uint32_t triangle; bool direction; };
    const std::size_t triangle_count = mesh.indices.size() / 3U;
    std::unordered_map<std::uint64_t, EdgeEntry> edges;
    std::vector<std::vector<std::pair<std::uint32_t, std::uint8_t>>> neighbours(triangle_count);
    for (std::uint32_t triangle = 0U; triangle < triangle_count; ++triangle) {
        for (std::uint32_t edge = 0U; edge < 3U; ++edge) {
            const auto actual_a = mesh.indices[triangle * 3U + edge];
            const auto actual_b = mesh.indices[triangle * 3U + (edge + 1U) % 3U];
            const auto found_a=duplicate_sources.find(actual_a),found_b=duplicate_sources.find(actual_b);
            const auto a=found_a==duplicate_sources.end()?actual_a:found_a->second;
            const auto b=found_b==duplicate_sources.end()?actual_b:found_b->second;
            const auto low = std::min(a, b), high = std::max(a, b);
            const std::uint64_t key = static_cast<std::uint64_t>(low) * mesh.vertices.size() + high;
            const EdgeEntry entry{triangle, a < b};
            const auto found = edges.find(key);
            if (found == edges.end()) edges.emplace(key, entry);
            else {
                const std::uint8_t different = found->second.direction == entry.direction ? 1U : 0U;
                neighbours[triangle].push_back({found->second.triangle, different});
                neighbours[found->second.triangle].push_back({triangle, different});
            }
        }
    }
    std::vector<std::int8_t> flips(triangle_count, -1);
    for (std::uint32_t start = 0U; start < triangle_count; ++start) {
        if (flips[start] != -1) continue;
        flips[start] = 0;
        std::vector<std::uint32_t> queue{start};
        double score = 0.0;
        for (std::size_t at = 0U; at < queue.size(); ++at) {
            const auto triangle = queue[at];
            for (const auto [next, delta] : neighbours[triangle]) if (flips[next] == -1) {
                flips[next] = static_cast<std::int8_t>(flips[triangle] ^ delta);
                queue.push_back(next);
            }
            const auto a = mesh.indices[triangle * 3U];
            const auto b = mesh.indices[triangle * 3U + 1U];
            const auto c = mesh.indices[triangle * 3U + 2U];
            const auto& pa = mesh.vertices[a].position;
            const auto& pb0 = mesh.vertices[b].position;
            const auto& pc0 = mesh.vertices[c].position;
            const Vec3 pb{pb0.x-pa.x,pb0.y-pa.y,pb0.z-pa.z};
            const Vec3 pc{pc0.x-pa.x,pc0.y-pa.y,pc0.z-pa.z};
            const Vec3 normal{pb.y*pc.z-pb.z*pc.y,pb.z*pc.x-pb.x*pc.z,pb.x*pc.y-pb.y*pc.x};
            const Vec3 hint{hinted_vertices[a].normal.x+hinted_vertices[b].normal.x+hinted_vertices[c].normal.x,
                            hinted_vertices[a].normal.y+hinted_vertices[b].normal.y+hinted_vertices[c].normal.y,
                            hinted_vertices[a].normal.z+hinted_vertices[b].normal.z+hinted_vertices[c].normal.z};
            const double dot = static_cast<double>(normal.x)*hint.x+
                static_cast<double>(normal.y)*hint.y+static_cast<double>(normal.z)*hint.z;
            score += flips[triangle] ? -dot : dot;
        }
        const std::uint8_t invert = score < 0.0 ? 1U : 0U;
        for (const auto triangle : queue) if ((flips[triangle] ^ invert) != 0)
            std::swap(mesh.indices[triangle * 3U + 1U], mesh.indices[triangle * 3U + 2U]);
    }
    for (auto& vertex : mesh.vertices) vertex.normal = {};
    for (std::size_t offset = 0U; offset < mesh.indices.size(); offset += 3U) {
        const std::uint32_t a = mesh.indices[offset];
        const std::uint32_t b = mesh.indices[offset + 1U];
        const std::uint32_t c = mesh.indices[offset + 2U];
        const auto& pa = mesh.vertices[a].position;
        const auto& pb = mesh.vertices[b].position;
        const auto& pc = mesh.vertices[c].position;
        const float cbx = pc.x - pb.x, cby = pc.y - pb.y, cbz = pc.z - pb.z;
        const float abx = pa.x - pb.x, aby = pa.y - pb.y, abz = pa.z - pb.z;
        const Vec3 normal{cby * abz - cbz * aby,
                          cbz * abx - cbx * abz,
                          cbx * aby - cby * abx};
        for (const std::uint32_t index : {a, b, c}) {
            const auto duplicate=duplicate_sources.find(index);
            const auto target=duplicate==duplicate_sources.end()?index:duplicate->second;
            mesh.vertices[target].normal.x += normal.x;
            mesh.vertices[target].normal.y += normal.y;
            mesh.vertices[target].normal.z += normal.z;
        }
    }
    for(const auto& [duplicate,source]:duplicate_sources)
        mesh.vertices[duplicate].normal=mesh.vertices[source].normal;
    for (auto& vertex : mesh.vertices) {
        const float length = std::sqrt(vertex.normal.x * vertex.normal.x +
                                       vertex.normal.y * vertex.normal.y +
                                       vertex.normal.z * vertex.normal.z);
        if (length > 0.0F) {
            vertex.normal.x /= length;
            vertex.normal.y /= length;
            vertex.normal.z /= length;
        }
    }
    for (auto& vertex : mesh.vertices) {
        vertex.material_region = static_cast<std::uint16_t>(
            vertex.material_region == 0U ? AppearanceMaterialRegion::EquipmentCloth :
            vertex.material_region == 1U ? AppearanceMaterialRegion::EquipmentPaint :
                                           AppearanceMaterialRegion::EquipmentMetal);
    }
    mesh.tags = std::move(tags);
    return foundation::Result<AppearanceMesh, foundation::Error>::success(std::move(mesh));
}

} // namespace genomes::infantry
