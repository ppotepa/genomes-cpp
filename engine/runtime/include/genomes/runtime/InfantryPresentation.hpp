#pragma once

#include <genomes/infantry/FaceAnimation.hpp>
#include <genomes/infantry/InfantryModelCompiler.hpp>
#include <genomes/render/RenderTypes.hpp>

#include <array>
#include <memory>
#include <span>
#include <vector>

namespace genomes::runtime::infantry_presentation {

[[nodiscard]] std::shared_ptr<const render::SkinnedMeshPrototype> makePrototype(
    const infantry::InfantryModelArtifact& model,
    const infantry::FaceOutput* face_output = nullptr);

[[nodiscard]] std::vector<std::array<float, 16U>> makeBindPalette(
    const infantry::SkeletonData& skeleton);

[[nodiscard]] std::vector<std::array<float, 16U>> makePalette(
    const infantry::SkeletonData& skeleton,
    std::span<const infantry::RigTransform> pose_bones);

} // namespace genomes::runtime::infantry_presentation
