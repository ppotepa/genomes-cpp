#pragma once

#include <genomes/infantry/EquipmentFit.hpp>
#include <genomes/infantry/ReferenceSurfaceBuilder.hpp>

#include <cstddef>

namespace genomes::infantry {

struct ReferenceJacketBuild final {
    std::size_t vertices{0U};
    std::size_t triangles{0U};
};

struct ReferenceJacketTopology final {
    std::vector<ReferenceSurfaceBuilder::Ring> rings;
    std::uint32_t segments{0U};
    std::uint32_t port_span{0U};
};

struct ReferenceAvatarSurface final {
    AppearanceMesh mesh;
    std::array<MorphTarget,4U> morphs{};
};

class ReferenceBodySurfaceGenerator final {
public:
    [[nodiscard]] static foundation::Result<ReferenceAvatarSurface,foundation::Error> build(
        const EquipmentFit&, const SkeletonData&, foundation::Color uniform,
        std::uint32_t detail_level);
    [[nodiscard]] static ReferenceJacketBuild appendJacket(
        ReferenceSurfaceBuilder&, const EquipmentFit&, const SkeletonData&,
        foundation::Color uniform, std::uint32_t detail_level,
        ReferenceJacketTopology* topology = nullptr);
    [[nodiscard]] static ReferenceJacketBuild appendSleeve(
        ReferenceSurfaceBuilder&, const ReferenceJacketTopology&, const EquipmentFit&,
        foundation::Color uniform, bool left);
    [[nodiscard]] static ReferenceJacketBuild appendHand(
        ReferenceSurfaceBuilder&, const EquipmentFit&, bool left,
        std::uint32_t detail_level, foundation::Color palm_color,
        foundation::Color digit_color);
    [[nodiscard]] static ReferenceJacketBuild appendPants(
        ReferenceSurfaceBuilder&, const EquipmentFit&, foundation::Color uniform,
        std::uint32_t detail_level);
    [[nodiscard]] static ReferenceJacketBuild appendBoot(
        ReferenceSurfaceBuilder&, const EquipmentFit&, bool left,
        std::uint32_t detail_level, foundation::Color color);
    [[nodiscard]] static ReferenceJacketBuild appendClothDetails(
        ReferenceSurfaceBuilder&, const EquipmentFit&, foundation::Color uniform,
        std::uint32_t detail_level);
};

} // namespace genomes::infantry
