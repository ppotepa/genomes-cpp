#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/geometry/IndexOrderOptimizer.hpp>
#include <genomes/geometry/MeshData.hpp>

namespace genomes::geometry {

enum class OptimizationPolicy : std::uint8_t { Static, Skinned };
struct MeshOptimizationReport final {
    IndexOptimizationStats index_stats{};
    bool vertices_remapped{false};
};
struct MeshOptimizationResult final {
    MeshData mesh{};
    MeshOptimizationReport report{};
};

[[nodiscard]] foundation::Result<MeshOptimizationResult, foundation::Error>
optimizeMesh(const MeshData&, OptimizationPolicy);

} // namespace genomes::geometry
