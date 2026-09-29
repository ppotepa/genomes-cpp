#pragma once

#include <genomes/render/RenderTypes.hpp>

#if GENOMES_HAS_INFANTRY
#include <genomes/infantry/FaceAnimation.hpp>
#include <genomes/infantry/InfantryModelCompiler.hpp>
#endif

#include <array>
#include <memory>
#include <span>
#include <vector>

namespace genomes::runtime::infantry_presentation {

#if GENOMES_HAS_INFANTRY

enum class PrototypePreparation {
    ReferenceOrder,
    OptimizeDrawOrder,
};

// The domain artifact is never changed. ReferenceOrder keeps the unprepared
// presentation index stream for differential tests/captures. The default uses
// the optional CPU index optimizer once per cached prototype, never per pose.
[[nodiscard]] std::shared_ptr<const render::SkinnedMeshPrototype> makePrototype(
    const infantry::InfantryModelArtifact& model,
    PrototypePreparation preparation = PrototypePreparation::OptimizeDrawOrder);

[[nodiscard]] std::vector<std::array<float, 16U>> makeBindPalette(
    const infantry::SkeletonData& skeleton);

[[nodiscard]] std::vector<std::array<float, 16U>> makePalette(
    const infantry::SkeletonData& skeleton,
    std::span<const infantry::RigTransform> pose_bones);

[[nodiscard]] std::vector<render::SkinnedBoneTransform> makeLocalPoses(
    const infantry::SkeletonData& skeleton,
    std::span<const infantry::RigTransform> pose_bones);
#endif

} // namespace genomes::runtime::infantry_presentation
