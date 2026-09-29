#include <genomes/render/SkinnedMeshOptimizer.hpp>

#include <cmath>
#include <utility>

namespace genomes::render {
namespace {

bool orderSensitive(const MaterialDescriptor& material) noexcept {
    return (material.alpha_mode != MaterialAlphaMode::Opaque &&
            material.alpha_mode != MaterialAlphaMode::Mask) ||
           material.instance_tint ||
           material.opacity < 1.0F || material.base_color.a < 1.0F;
}

} // namespace

foundation::Result<geometry::IndexOptimizationStats, foundation::Error>
optimizeSkinnedDrawOrder(SkinnedMeshPrototype& mesh) {
    using Result = foundation::Result<geometry::IndexOptimizationStats, foundation::Error>;
    for (const auto& material : mesh.materials) {
        if (!material.valid()) {
            return Result::failure({foundation::ErrorCode::InvalidArgument,
                                    "cannot optimize an invalid material descriptor"});
        }
    }
    // Validate before inspecting vertices via the index stream.
    for (const auto index : mesh.indices) {
        if (index >= mesh.vertices.size()) {
            return Result::failure({foundation::ErrorCode::OutOfRange,
                                    "skinned optimization index is out of range"});
        }
    }
    std::vector<geometry::IndexOptimizationRange> ranges;
    ranges.reserve(mesh.material_groups.size() + 1U);
    if (mesh.material_groups.empty()) {
        if (mesh.materials.size() > 1U && !mesh.indices.empty()) {
            return Result::failure({foundation::ErrorCode::InvalidArgument,
                                    "multiple materials require explicit draw groups"});
        }
        if (!mesh.indices.empty()) {
            // Unspecified material semantics are conservative, not implicitly opaque.
            ranges.push_back({0U, mesh.indices.size(),
                              mesh.materials.empty() || orderSensitive(mesh.materials.front())});
        }
    } else {
        for (const auto& group : mesh.material_groups) {
            if (!group.valid(mesh.indices.size(), mesh.materials.size())) {
                return Result::failure({foundation::ErrorCode::InvalidArgument,
                                        "invalid skinned material group"});
            }
            ranges.push_back({group.first_index, group.index_count,
                              orderSensitive(mesh.materials[group.material_index])});
        }
    }
    for (auto& range : ranges) {
        for (std::size_t index = range.first_index;
             index < range.first_index + range.index_count; ++index) {
            const float alpha = mesh.vertices[mesh.indices[index]].color.a;
            if (!std::isfinite(alpha)) {
                return Result::failure({foundation::ErrorCode::InvalidArgument,
                                        "non-finite skinned vertex alpha"});
            }
            range.preserve_order = range.preserve_order || alpha < 1.0F;
        }
    }
    auto prepared = geometry::optimizeIndexOrder(mesh.indices, mesh.vertices.size(), ranges);
    if (!prepared) return Result::failure(prepared.error());
    const auto stats = prepared.value().stats;
    mesh.indices.swap(prepared.value().indices);
    return Result::success(stats);
}

} // namespace genomes::render
