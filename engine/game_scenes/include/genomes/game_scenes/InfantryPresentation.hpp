#pragma once

#include <genomes/render/RenderTypes.hpp>
#include <genomes/foundation/Types.hpp>

#if GENOMES_HAS_INFANTRY
#include <genomes/infantry/FaceAnimation.hpp>
#include <genomes/infantry/InfantryModelCompiler.hpp>
#include <genomes/infantry/AppearanceCatalog.hpp>
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

// Presentation-only material variants retain geometry, indices, skeleton and
// morph buffers from the immutable prototype and receive a distinct revision.
[[nodiscard]] std::shared_ptr<const render::SkinnedMeshPrototype> makeMaterialVariant(
    const render::SkinnedMeshPrototype& prototype,
    foundation::StableId appearance_preset,
    const infantry::FrozenAppearanceCatalog& catalog);

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
