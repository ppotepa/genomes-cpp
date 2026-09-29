#pragma once

#include <genomes/geometry/IndexOrderOptimizer.hpp>
#include <genomes/render/RenderTypes.hpp>

namespace genomes::render {

// Atomic index-only preparation of a completed skinned prototype. All vertex,
// skeleton, morph, material and bounds data remain byte-for-byte untouched.
// A failure leaves mesh unchanged. Does not silently repair invalid materials.
[[nodiscard]] foundation::Result<geometry::IndexOptimizationStats, foundation::Error>
optimizeSkinnedDrawOrder(SkinnedMeshPrototype& mesh);

} // namespace genomes::render
