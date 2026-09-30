#pragma once
#include <genomes/render/RenderTypes.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace genomes::render {
struct MaterialDrawRange final {
    std::uint32_t first_index{0}, index_count{0};
    MaterialDescriptor material{};
    float minimum_vertex_alpha{1.0F};
};
using MaterialPlanResult=foundation::Result<std::vector<MaterialDrawRange>,foundation::Error>;

// Validate once at prototype upload, not every animation tick. This operation
// changes neither draw ordering nor vertex IDs and cannot infer unknown textures.
template<class Mesh> MaterialPlanResult buildMaterialDrawPlan(const Mesh& mesh) {
    const auto fail=[](const char* text) {
        return MaterialPlanResult::failure({foundation::ErrorCode::InvalidArgument,text});
    };
    if (mesh.indices.empty() || mesh.indices.size()%3U!=0U ||
        mesh.indices.size()>std::numeric_limits<std::uint32_t>::max())
        return fail("invalid material index stream");
    std::vector<MeshMaterialGroup> groups=mesh.material_groups;
    if (groups.empty()) {
        if (mesh.materials.size()>1U) return fail("multiple materials require explicit draw groups");
        groups.push_back({0U,static_cast<std::uint32_t>(mesh.indices.size()),0U});
    }
    MaterialDescriptor fallback{};
    fallback.material_id=foundation::stable_id("material.render.fallback");
    fallback.revision=1U;fallback.double_sided=true;
    std::vector<MaterialDrawRange> output;output.reserve(groups.size());
    std::size_t cursor=0;
    for (const auto& group:groups) {
        const auto count=std::max<std::size_t>(1U,mesh.materials.size());
        if (group.first_index!=cursor || group.first_index%3U!=0U || !group.valid(mesh.indices.size(),count))
            return fail("material groups must exactly partition triangles");
        const auto material=mesh.materials.empty()?fallback:mesh.materials[group.material_index];
        if (!material.valid() || material.base_color.a<0.0F || material.base_color.a>1.0F)
            return fail("invalid material descriptor");
        if (material.alpha_mode!=MaterialAlphaMode::Opaque &&
            material.alpha_mode!=MaterialAlphaMode::Mask && material.alpha_mode!=MaterialAlphaMode::Blend)
            return fail("invalid material alpha mode");
        if (material.base_color_texture || material.normal_texture || material.roughness_metalness_texture)
            return fail("texture IDs require a texture resource catalogue; cannot silently ignore them");
        float alpha=1.0F;
        for (std::size_t k=cursor;k<cursor+group.index_count;++k) {
            if (mesh.indices[k]>=mesh.vertices.size()) return fail("material range has invalid vertex index");
            const float a=mesh.vertices[mesh.indices[k]].color.a;
            if (!std::isfinite(a) || a<0.0F || a>1.0F) return fail("invalid vertex alpha");
            alpha=std::min(alpha,a);
        }
        output.push_back({group.first_index,group.index_count,material,alpha});
        cursor+=group.index_count;
    }
    if (cursor!=mesh.indices.size()) return fail("material ranges leave uncovered indices");
    return MaterialPlanResult::success(std::move(output));
}

[[nodiscard]] inline MaterialAlphaMode effectiveAlpha(const MaterialDrawRange& range,
                                                      foundation::Color tint) noexcept {
    if (range.material.alpha_mode==MaterialAlphaMode::Mask) return MaterialAlphaMode::Mask;
    const float a=range.material.opacity*range.material.base_color.a*
        (range.material.vertex_color?range.minimum_vertex_alpha:1.0F)*
        (range.material.instance_tint?tint.a:1.0F);
    return range.material.alpha_mode==MaterialAlphaMode::Blend || a<0.99999F
        ? MaterialAlphaMode::Blend : MaterialAlphaMode::Opaque;
}
} // namespace genomes::render
