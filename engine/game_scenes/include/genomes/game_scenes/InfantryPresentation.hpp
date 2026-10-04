#pragma once

#include <genomes/render/RenderTypes.hpp>
#include <genomes/foundation/Types.hpp>

#if GENOMES_HAS_INFANTRY
#include <genomes/infantry/AnimationWeaponOverlay.hpp>
#include <genomes/infantry/FaceAnimation.hpp>
#include <genomes/infantry/InfantryModelCompiler.hpp>
#include <genomes/infantry/AppearanceCatalog.hpp>
#include <genomes/weapons/WeaponPoseTasks.hpp>
#include <genomes/weapons/WeaponCatalog.hpp>
#endif

#include <array>
#include <memory>
#include <span>
#include <vector>

namespace genomes::proc {
class GenerationClient;
}

namespace genomes::game_scenes::infantry_presentation {

#if GENOMES_HAS_INFANTRY

enum class PrototypePreparation {
    ReferenceOrder,
    OptimizeDrawOrder,
};

enum class WeaponPoseAttachment {
    Stowed,
    RightHand,
};

// The domain artifact is never changed. ReferenceOrder keeps the unprepared
// presentation index stream for differential tests/captures. The default uses
// the optional CPU index optimizer once per cached prototype, never per pose.
[[nodiscard]] std::shared_ptr<const render::SkinnedMeshPrototype> makePrototype(
    const infantry::InfantryModelArtifact& model,
    PrototypePreparation preparation = PrototypePreparation::OptimizeDrawOrder,
    proc::GenerationClient* generation = nullptr,
    WeaponPoseAttachment weapon_attachment = WeaponPoseAttachment::Stowed,
    const weapons::WeaponArtifact* weapon_artifact = nullptr);

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

// Scene boundary adapter: weapons owns WeaponPoseTasks, infantry owns the
// neutral animation overlay. The animator never includes the weapons module.
[[nodiscard]] infantry::AnimationWeaponOverlay copyWeaponPoseTasks(
    const weapons::WeaponPoseTasks& tasks, foundation::Vec3 root_position) noexcept;
#endif

} // namespace genomes::game_scenes::infantry_presentation
