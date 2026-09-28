#include <genomes/infantry/GearSurfaceGenerator.hpp>

#include <genomes/infantry/EquipmentCatalog.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <vector>

namespace genomes::infantry {

namespace {

using foundation::Vec2;
using foundation::Vec3;

[[nodiscard]] foundation::Color tint(foundation::Color color, float factor) noexcept {
    return {std::clamp(color.r * factor, 0.0F, 1.0F),
            std::clamp(color.g * factor, 0.0F, 1.0F),
            std::clamp(color.b * factor, 0.0F, 1.0F), color.a};
}

[[nodiscard]] std::array<SkinInfluence, 4U> boneWeight(const GearPiece& piece,
                                                         std::uint8_t& count) noexcept {
    count = 1U;
    return {SkinInfluence{static_cast<std::uint16_t>(boneIndex(piece.bone)), 1.0F},
            {}, {}, {}};
}

void appendBox(AppearanceMeshBuilder& builder, const GearPiece& piece, Vec3 center,
               Vec3 dimensions, foundation::Color color, std::uint32_t region) {
    constexpr std::array<Vec3, 8U> corners{{
        {-1.0F, -1.0F, -1.0F}, {1.0F, -1.0F, -1.0F},
        {1.0F, 1.0F, -1.0F}, {-1.0F, 1.0F, -1.0F},
        {-1.0F, -1.0F, 1.0F}, {1.0F, -1.0F, 1.0F},
        {1.0F, 1.0F, 1.0F}, {-1.0F, 1.0F, 1.0F}}};
    constexpr std::array<std::array<std::uint32_t, 4U>, 6U> faces{{
        {{0U, 1U, 2U, 3U}}, {{5U, 4U, 7U, 6U}}, {{4U, 0U, 3U, 7U}},
        {{1U, 5U, 6U, 2U}}, {{3U, 2U, 6U, 7U}}, {{4U, 5U, 1U, 0U}}}};
    constexpr std::array<Vec3, 6U> normals{{
        {0.0F, 0.0F, -1.0F}, {0.0F, 0.0F, 1.0F}, {-1.0F, 0.0F, 0.0F},
        {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}, {0.0F, -1.0F, 0.0F}}};
    constexpr std::array<Vec2, 4U> uv{{{0.0F, 0.0F}, {1.0F, 0.0F},
                                        {1.0F, 1.0F}, {0.0F, 1.0F}}};
    const Vec3 half{dimensions.x * 0.5F, dimensions.y * 0.5F, dimensions.z * 0.5F};
    std::uint8_t count = 0U;
    const auto influences = boneWeight(piece, count);
    const std::span<const SkinInfluence> weights(influences.data(), count);
    for (std::size_t face_index = 0U; face_index < faces.size(); ++face_index) {
        std::array<std::uint32_t, 4U> indices{};
        for (std::size_t corner = 0U; corner < indices.size(); ++corner) {
            const Vec3 unit = corners[faces[face_index][corner]];
            indices[corner] = builder.appendVertex({
                {center.x + unit.x * half.x, center.y + unit.y * half.y,
                 center.z + unit.z * half.z},
                normals[face_index], uv[corner], color,
                static_cast<std::uint16_t>(region), weights});
        }
        builder.triangle(indices[0], indices[1], indices[2]);
        builder.triangle(indices[0], indices[2], indices[3]);
    }
}

void appendEllipsoid(AppearanceMeshBuilder& builder, const GearPiece& piece, Vec3 center,
                     Vec3 radii, foundation::Color color, std::uint32_t region) {
    constexpr std::size_t segments = 16U;
    constexpr std::size_t rows = 6U;
    std::uint8_t count = 0U;
    const auto influences = boneWeight(piece, count);
    const std::span<const SkinInfluence> weights(influences.data(), count);
    const auto vertex = [&builder, color, region, weights](Vec3 position, Vec3 normal,
                                                            Vec2 uv) {
        return builder.appendVertex({position, normal, uv,
                                     color, static_cast<std::uint16_t>(region), weights});
    };
    const auto south = vertex({center.x, center.y - radii.y, center.z},
                              {0.0F, -1.0F, 0.0F}, {0.5F, 0.0F});
    std::vector<std::uint32_t> previous;
    for (std::size_t row = 1U; row < rows; ++row) {
        const float phi = -1.57079632679F +
            3.14159265359F * static_cast<float>(row) / static_cast<float>(rows);
        const float cp = std::cos(phi);
        const float sp = std::sin(phi);
        std::vector<std::uint32_t> current;
        current.reserve(segments);
        for (std::size_t segment = 0U; segment < segments; ++segment) {
            const float angle = 6.28318530718F * static_cast<float>(segment) /
                                static_cast<float>(segments);
            const float ca = std::cos(angle);
            const float sa = std::sin(angle);
            current.push_back(vertex({center.x + radii.x * cp * ca,
                                      center.y + radii.y * sp,
                                      center.z + radii.z * cp * sa},
                                     {cp * ca, sp, cp * sa},
                                     {static_cast<float>(segment) / static_cast<float>(segments),
                                      static_cast<float>(row) / static_cast<float>(rows)}));
        }
        if (previous.empty()) {
            for (std::size_t segment = 0U; segment < segments; ++segment) {
                const std::size_t next = (segment + 1U) % segments;
                builder.triangle(south, current[next], current[segment]);
            }
        } else {
            for (std::size_t segment = 0U; segment < segments; ++segment) {
                const std::size_t next = (segment + 1U) % segments;
                builder.triangle(previous[segment], current[segment], previous[next]);
                builder.triangle(previous[next], current[segment], current[next]);
            }
        }
        previous = std::move(current);
    }
    const auto north = vertex({center.x, center.y + radii.y, center.z},
                              {0.0F, 1.0F, 0.0F}, {0.5F, 1.0F});
    for (std::size_t segment = 0U; segment < segments; ++segment) {
        const std::size_t next = (segment + 1U) % segments;
        builder.triangle(north, previous[segment], previous[next]);
    }
}

} // namespace

foundation::Result<AppearanceMesh, foundation::Error> GearSurfaceGenerator::build(
    const GearArtifact& artifact) {
    AppearanceMeshBuilder builder;
    for (const GearPiece& piece : artifact.pieces) {
        const auto* definition = EquipmentCatalog::findItem(piece.definition_id);
        const std::string_view style = definition == nullptr ? std::string_view{} : definition->style;
        const Vec3 center = piece.center;
        const Vec3 dimensions = piece.dimensions;
        const foundation::Color dark = tint(piece.color, 0.58F);
        const foundation::Color edge = tint(piece.color, 0.78F);
        if (piece.slot != EquipmentSlot::TorsoBase && piece.slot != EquipmentSlot::Legs &&
            piece.slot != EquipmentSlot::Feet && piece.slot != EquipmentSlot::Hands) {
            appendBox(builder, piece, center, dimensions, piece.color, piece.material_region);
        }
        switch (piece.slot) {
        case EquipmentSlot::Head:
            appendEllipsoid(builder, piece, {center.x, center.y + dimensions.y * 0.10F, center.z},
                            {dimensions.x * 0.60F, dimensions.y * 0.52F, dimensions.z * 0.66F},
                            piece.color, piece.material_region);
            appendBox(builder, piece, {center.x, center.y - dimensions.y * 0.22F,
                                       center.z + dimensions.z * 0.05F},
                      {dimensions.x * 1.18F, dimensions.y * 0.10F, dimensions.z * 1.05F},
                      edge, piece.material_region);
            break;
        case EquipmentSlot::TorsoArmor:
            appendBox(builder, piece, {center.x, center.y, center.z + dimensions.z * 0.58F},
                      {dimensions.x * 0.86F, dimensions.y * 0.82F, dimensions.z * 0.20F},
                      edge, piece.material_region);
            break;
        case EquipmentSlot::ChestRig:
            for (int index = -1; index <= 1; ++index) {
                appendBox(builder, piece,
                          {center.x + static_cast<float>(index) * dimensions.x * 0.28F,
                           center.y - dimensions.y * 0.05F, center.z + dimensions.z * 0.57F},
                          {dimensions.x * 0.24F, dimensions.y * 0.52F, dimensions.z * 0.18F},
                          dark, piece.material_region);
            }
            break;
        case EquipmentSlot::Back:
            appendEllipsoid(builder, piece, {center.x, center.y, center.z - dimensions.z * 0.52F},
                            {dimensions.x * 0.52F, dimensions.y * 0.48F, dimensions.z * 0.58F},
                            piece.color, piece.material_region);
            break;
        case EquipmentSlot::LeftHip:
        case EquipmentSlot::RightHip:
        case EquipmentSlot::LeftThigh:
        case EquipmentSlot::RightThigh:
        case EquipmentSlot::Utility1:
        case EquipmentSlot::Utility2:
        case EquipmentSlot::Utility3:
            appendBox(builder, piece,
                      {center.x, center.y + dimensions.y * 0.48F,
                       center.z + dimensions.z * 0.04F},
                      {dimensions.x * 0.86F, dimensions.y * 0.12F, dimensions.z * 0.92F},
                      edge, piece.material_region);
            break;
        case EquipmentSlot::PrimaryWeapon:
            appendBox(builder, piece, center,
                      {dimensions.x * 0.72F, dimensions.y * 0.72F, dimensions.z * 0.42F},
                      dark, piece.material_region);
            appendBox(builder, piece, {center.x, center.y + dimensions.y * 0.02F,
                                       center.z + dimensions.z * 0.42F},
                      {dimensions.x * 0.30F, dimensions.y * 0.30F, dimensions.z * 0.70F},
                      edge, piece.material_region);
            break;
        case EquipmentSlot::SecondaryWeapon:
        case EquipmentSlot::MeleeWeapon:
        case EquipmentSlot::Throwable:
            appendEllipsoid(builder, piece, center,
                            {dimensions.x * 0.52F, dimensions.y * 0.48F, dimensions.z * 0.48F},
                            style == "grenade" ? edge : dark, piece.material_region);
            break;
        default:
            break;
        }
    }
    return foundation::Result<AppearanceMesh, foundation::Error>::success(
        std::move(builder).finalize());
}

} // namespace genomes::infantry
