#pragma once
#include "shaders/SkinnedLayoutProfileV1.hlsli"
#include <genomes/render/RenderTypes.hpp>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace genomes::render::diligent_contract {
inline constexpr std::uint32_t kSkinnedLayoutProfileVersion=
    GENOMES_SKINNED_LAYOUT_PROFILE_VERSION;
inline constexpr std::size_t kBoneCount=GENOMES_SKINNED_LAYOUT_PROFILE_BONE_COUNT;
inline constexpr std::size_t kInfluenceCount=GENOMES_SKINNED_LAYOUT_PROFILE_INFLUENCE_COUNT;
inline constexpr std::size_t kMorphCount=GENOMES_SKINNED_LAYOUT_PROFILE_MORPH_COUNT;
static_assert(kSkinnedLayoutProfileVersion==1U);
static_assert(kBoneCount==69U && kInfluenceCount==4U && kMorphCount==4U,
              "SkinnedLayoutProfileV1 is an immutable GPU ABI");
struct alignas(16) SkinnedPassConstants final {
    float view_projection[16]{};
    float object_position_scale[4]{};
    float object_scale_rotation[4]{};
    float camera_position[4]{};
    float character_key_direction_intensity[4]{};
    float character_key_color[4]{};
    float character_fill_direction_intensity[4]{};
    float character_fill_color[4]{};
    float character_hemisphere_sky[4]{};
    float character_hemisphere_ground[4]{};
    float morph_weights[kMorphCount]{};
    float bone_palette[kBoneCount][16]{};
};
struct SkinnedGpuVertex final {
    float position[3]{},normal[3]{},uv[2]{},color[4]{};
    float bone_indices[kInfluenceCount]{},bone_weights[kInfluenceCount]{};
    float morph_position[kMorphCount][3]{},morph_normal[kMorphCount][3]{};
    std::uint32_t material_region{0};
};
struct alignas(16) SceneConstants final {
    float view_projection[16]{};
    float shadow_uv_projection[16]{};
    float camera_position[4]{};
    float key_direction_intensity[4]{},key_color[4]{};
    float fill_direction_intensity[4]{},fill_color[4]{};
    float hemisphere_sky[4]{},hemisphere_ground[4]{};
    float shadow_parameters[4]{};
};
struct alignas(16) MaterialConstants final {
    float base_color[4]{};
    float factors[4]{}; // roughness, metalness, opacity, alpha cutoff
    float tint[4]{};
    std::uint32_t flags[4]{}; // vertex color, instance tint, alpha mode, receive shadow
};
struct InstanceGpuVertex final {
    float position_rotation[4]{};
    float scale[4]{};
    float tint[4]{};
};
struct SkinnedInstanceGpuVertex final {
    float position_rotation[4]{};
    float scale[4]{};
    float tint[4]{};
    float morph_weights[4]{};
    std::uint32_t palette_index{0U};
    std::uint32_t debug_weight_bone{std::numeric_limits<std::uint32_t>::max()};
    std::uint32_t reserved[2]{};
};
struct DebugGpuVertex final { float position[3]{},color[4]{}; };
struct UiGpuVertex final { float position[2]{},uv[2]{},color[4]{}; };

static_assert(std::is_standard_layout_v<RenderMeshVertex> && sizeof(RenderMeshVertex)==52U);
static_assert(offsetof(RenderMeshVertex,color)==32U && offsetof(RenderMeshVertex,material_region)==48U);
static_assert(std::is_standard_layout_v<SkinnedGpuVertex> && sizeof(SkinnedGpuVertex)==180U);
static_assert(offsetof(SkinnedGpuVertex,material_region)==176U);
static_assert(offsetof(SkinnedPassConstants,camera_position)==96U);
static_assert(offsetof(SkinnedPassConstants,morph_weights)==208U);
static_assert(offsetof(SkinnedPassConstants,bone_palette)==224U);
static_assert(sizeof(SkinnedPassConstants)==4640U);
static_assert(sizeof(SceneConstants)==256U);
static_assert(offsetof(SceneConstants,shadow_parameters)==240U);
static_assert(sizeof(MaterialConstants)==64U && offsetof(MaterialConstants,tint)==32U && offsetof(MaterialConstants,flags)==48U);
static_assert(sizeof(InstanceGpuVertex)==48U && sizeof(UiGpuVertex)==32U);
static_assert(sizeof(SkinnedInstanceGpuVertex)==80U &&
              offsetof(SkinnedInstanceGpuVertex,palette_index)==64U);

inline bool finite(foundation::Vec3 v) noexcept { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
inline bool validInstance(const RenderInstance& v) noexcept {
    return finite(v.position)&&finite(v.scale)&&v.scale.x>1.0e-6F&&v.scale.y>1.0e-6F&&v.scale.z>1.0e-6F&&
        std::isfinite(v.rotation_y)&&std::isfinite(v.tint.r)&&std::isfinite(v.tint.g)&&std::isfinite(v.tint.b)&&
        std::isfinite(v.tint.a)&&v.tint.a>=0.0F&&v.tint.a<=1.0F;
}
inline bool validPalette(const SkinnedMeshPrototype& mesh,const SkinnedBonePalette& p) noexcept {
    if (p.matrices.size()!=kBoneCount || p.debug_weight_bone< -1 || p.debug_weight_bone>=static_cast<std::int32_t>(kBoneCount)) return false;
    if (mesh.skeleton && (!mesh.skeleton->valid() || mesh.skeleton->bones.size()!=kBoneCount ||
                         p.skeleton_id!=mesh.skeleton->skeleton_id)) return false;
    for (const auto& m:p.matrices) {
        for (float f:m) if (!std::isfinite(f)) return false;
        if (std::abs(m[3])+std::abs(m[7])+std::abs(m[11])>1.0e-5F || std::abs(m[15]-1)>1.0e-5F) return false;
    }
    for (float f:p.morph_weights) if (!std::isfinite(f)) return false;
    return true;
}
} // namespace genomes::render::diligent_contract
