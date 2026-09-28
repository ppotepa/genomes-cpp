#pragma once

#include <genomes/infantry/AppearanceMeshBuilder.hpp>
#include <genomes/infantry/BodyPhenotype.hpp>
#include <genomes/infantry/SkeletonData.hpp>

namespace genomes::infantry {

// Shared profile operation for articulated body zones. The compiler owns
// ordering of semantic zones; this class owns the reusable body-surface
// emission rule and its continuous skin-weight transition.
class BodySurfaceGenerator final {
public:
    [[nodiscard]] static bool buildTorso(
        AppearanceMeshBuilder&, const BodyPhenotype&, const SkeletonData&,
        std::size_t segments, foundation::Color, geometry::Ring& neck_ring);

    [[nodiscard]] static bool buildLimbs(
        AppearanceMeshBuilder&, const BodyPhenotype&, const SkeletonData&,
        std::size_t segments, foundation::Color cloth, foundation::Color skin);

    static void appendEllipsoid(
        AppearanceMeshBuilder&, foundation::Vec3 center, foundation::Vec3 radii,
        std::size_t segments, std::size_t rows, foundation::Color,
        std::uint16_t material_region,
        const std::array<SkinInfluence, 4U>&, std::uint8_t influence_count);

    static void appendProfiledLimb(
        AppearanceMeshBuilder&, foundation::Vec3 start, foundation::Vec3 end,
        float radius_start, float radius_end, std::size_t segments,
        foundation::Color, std::uint16_t material_region,
        const std::array<SkinInfluence, 4U>& start_weights, std::uint8_t start_count,
        const std::array<SkinInfluence, 4U>& end_weights, std::uint8_t end_count);
};

} // namespace genomes::infantry
