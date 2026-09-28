#pragma once

#include <genomes/render/RenderTypes.hpp>

#include <span>

namespace genomes::render {

// Deterministic CPU fallback used by headless diagnostics and platforms
// without the skinned GPU pass. The matrix layout is the immutable palette
// layout published in SkinnedBonePalette.
[[nodiscard]] RenderMesh deformSkinnedCPU(
    const SkinnedMeshPrototype& prototype,
    std::span<const std::array<float, 16U>> palette);

} // namespace genomes::render
