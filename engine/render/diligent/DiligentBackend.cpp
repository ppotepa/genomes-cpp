#include "DiligentBackend.hpp"

#include <genomes/render/gpu_scene/GpuScene.hpp>
#include <ShaderSources.hpp>

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#if defined(_WIN32)
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#else
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#endif

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <cmath>
#include <iterator>
#include <limits>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace genomes::render {

namespace {

constexpr std::size_t kMaxUiVertices = 65'536;
constexpr std::size_t kMaxDebugVertices = 65'536;

struct CameraConstants final {
    float view_projection[16]{};
};

struct InstancePassConstants final {
    std::uint32_t required_flags{0};
    std::uint32_t match_material{0};
    std::uint32_t options_padding[2]{};
    std::uint32_t required_mesh_low{0};
    std::uint32_t required_mesh_high{0};
    std::uint32_t mesh_padding[2]{};
    std::uint32_t required_material_low{0};
    std::uint32_t required_material_high{0};
    std::uint32_t material_padding[2]{};
};

static_assert(sizeof(InstancePassConstants) == 48);

constexpr std::size_t kInfantryBonePaletteSize = 69U;

struct SkinnedPassConstants final {
    float view_projection[16]{};
    float object_position_scale[4]{};
    float object_scale_rotation[4]{};
    float character_key_direction_intensity[4]{};
    float character_key_color[4]{};
    float character_fill_direction_intensity[4]{};
    float character_fill_color[4]{};
    float character_hemisphere_sky[4]{};
    float character_hemisphere_ground[4]{};
    float morph_weights[4]{};
    float bone_palette[kInfantryBonePaletteSize][16]{};
};

static_assert(sizeof(SkinnedPassConstants) % 16U == 0U);

struct SkinnedGpuVertex final {
    float position[3]{};
    float normal[3]{0.0F, 1.0F, 0.0F};
    float uv[2]{};
    float color[4]{1.0F, 1.0F, 1.0F, 1.0F};
    float bone_indices[4]{};
    float bone_weights[4]{};
    float morph_position[4][3]{};
    float morph_normal[4][3]{};
    std::uint32_t material_region{0};
};

static_assert(sizeof(SkinnedGpuVertex) == 180U);
static_assert(offsetof(SkinnedGpuVertex, material_region) == 176U);

struct Mat4 final {
    float values[16]{};
};

[[nodiscard]] Mat4 multiply(const Mat4& left, const Mat4& right) noexcept {
    Mat4 result{};
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            float value = 0.0F;
            for (std::size_t k = 0; k < 4; ++k) {
                value += left.values[k * 4 + row] * right.values[column * 4 + k];
            }
            result.values[column * 4 + row] = value;
        }
    }
    return result;
}

[[nodiscard]] float dot(const foundation::Vec3& left,
                        const foundation::Vec3& right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] foundation::Vec3 subtract(const foundation::Vec3& left,
                                        const foundation::Vec3& right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] foundation::Vec3 cross(const foundation::Vec3& left,
                                     const foundation::Vec3& right) noexcept {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

[[nodiscard]] foundation::Vec3 normalize(const foundation::Vec3& value) noexcept {
    const float length = std::sqrt(std::max(0.000001F, dot(value, value)));
    return {value.x / length, value.y / length, value.z / length};
}

[[nodiscard]] Mat4 look_at(const foundation::Vec3& eye,
                           const foundation::Vec3& target,
                           const foundation::Vec3& world_up) noexcept {
    const foundation::Vec3 forward = normalize(subtract(target, eye));
    const foundation::Vec3 right = normalize(cross(forward, world_up));
    const foundation::Vec3 up = cross(right, forward);

    Mat4 result{};
    result.values[0] = right.x;
    result.values[1] = up.x;
    result.values[2] = -forward.x;
    result.values[4] = right.y;
    result.values[5] = up.y;
    result.values[6] = -forward.y;
    result.values[8] = right.z;
    result.values[9] = up.z;
    result.values[10] = -forward.z;
    result.values[12] = -dot(right, eye);
    result.values[13] = -dot(up, eye);
    result.values[14] = dot(forward, eye);
    result.values[15] = 1.0F;
    return result;
}

[[nodiscard]] Mat4 perspective(float vertical_fov,
                               float aspect,
                               float near_plane,
                               float far_plane) noexcept {
    const float f = 1.0F / std::tan(vertical_fov * 0.5F);
    Mat4 result{};
    result.values[0] = f / std::max(0.01F, aspect);
    result.values[5] = f;
    result.values[10] = far_plane / (near_plane - far_plane);
    result.values[11] = -1.0F;
    result.values[14] = (far_plane * near_plane) / (near_plane - far_plane);
    return result;
}

struct UiVertex final {
    float position[2];
    float color[4];
    float uv[2]{};
};

struct DebugVertex final {
    float position[3]{};
    float color[4]{1.0F, 1.0F, 1.0F, 1.0F};
};

[[nodiscard]] std::shared_ptr<RenderMesh> make_unit_cube_mesh(
    foundation::StableId mesh_id) {
    auto mesh = std::make_shared<RenderMesh>();
    mesh->mesh_id = mesh_id;
    mesh->revision = 1;
    mesh->vertices.reserve(24);
    mesh->indices.reserve(36);
    constexpr foundation::Vec3 corners[] = {
        {-0.5F, -0.5F, -0.5F}, {0.5F, -0.5F, -0.5F},
        {0.5F, 0.5F, -0.5F},   {-0.5F, 0.5F, -0.5F},
        {-0.5F, -0.5F, 0.5F},  {0.5F, -0.5F, 0.5F},
        {0.5F, 0.5F, 0.5F},    {-0.5F, 0.5F, 0.5F},
    };
    constexpr std::uint32_t faces[][4] = {
        {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
        {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}};
    constexpr foundation::Vec3 normals[] = {
        {0.0F, 0.0F, -1.0F}, {0.0F, 0.0F, 1.0F}, {-1.0F, 0.0F, 0.0F},
        {1.0F, 0.0F, 0.0F},  {0.0F, 1.0F, 0.0F},  {0.0F, -1.0F, 0.0F}};
    for (std::size_t face = 0; face < 6; ++face) {
        const std::uint32_t base = static_cast<std::uint32_t>(mesh->vertices.size());
        constexpr foundation::Vec2 uv[] = {{0.0F, 0.0F}, {1.0F, 0.0F},
                                           {1.0F, 1.0F}, {0.0F, 1.0F}};
        for (std::size_t corner = 0; corner < 4; ++corner) {
            mesh->vertices.push_back(
                {corners[faces[face][corner]], normals[face], uv[corner],
                 {1.0F, 1.0F, 1.0F, 1.0F}});
        }
        mesh->indices.insert(mesh->indices.end(),
                             {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    return mesh;
}

[[nodiscard]] std::shared_ptr<const RenderMesh> resolve_instance_prototype(
    const PresentationSnapshot& snapshot,
    foundation::StableId mesh_id,
    const std::shared_ptr<const RenderMesh>& fallback) {
    for (const std::shared_ptr<const RenderMesh>& prototype : snapshot.instance_prototypes) {
        if (prototype && prototype->mesh_id == mesh_id && !prototype->vertices.empty() &&
            !prototype->indices.empty()) {
            return prototype;
        }
    }
    if (snapshot.infantry_mesh && snapshot.infantry_mesh->mesh_id == mesh_id &&
        !snapshot.infantry_mesh->vertices.empty() && !snapshot.infantry_mesh->indices.empty()) {
        return snapshot.infantry_mesh;
    }
    return fallback;
}

[[nodiscard]] foundation::Vec3 mesh_half_extents(const RenderMesh& mesh) noexcept {
    foundation::Vec3 result{};
    for (const RenderMeshVertex& vertex : mesh.vertices) {
        result.x = std::max(result.x, std::abs(vertex.position.x));
        result.y = std::max(result.y, std::abs(vertex.position.y));
        result.z = std::max(result.z, std::abs(vertex.position.z));
    }
    return result;
}

[[nodiscard]] SkinnedGpuVertex to_skinned_gpu_vertex(
    const SkinnedMeshVertex& source,
    const SkinnedMeshPrototype& prototype,
    std::size_t vertex_index) noexcept {
    SkinnedGpuVertex target{};
    target.position[0] = source.position.x;
    target.position[1] = source.position.y;
    target.position[2] = source.position.z;
    target.normal[0] = source.normal.x;
    target.normal[1] = source.normal.y;
    target.normal[2] = source.normal.z;
    target.uv[0] = source.uv.x;
    target.uv[1] = source.uv.y;
    target.color[0] = source.color.r;
    target.color[1] = source.color.g;
    target.color[2] = source.color.b;
    target.color[3] = source.color.a;
    target.material_region = source.material_region;
    for (std::size_t index = 0U; index < 4U; ++index) {
        target.bone_indices[index] = static_cast<float>(source.bone_indices[index]);
        target.bone_weights[index] = source.bone_weights[index];
        if (index >= prototype.morph_target_count || index >= prototype.morphs.size()) {
            continue;
        }
        const auto& morph = prototype.morphs[index];
        if (vertex_index < morph.position_deltas.size()) {
            target.morph_position[index][0] = morph.position_deltas[vertex_index].x;
            target.morph_position[index][1] = morph.position_deltas[vertex_index].y;
            target.morph_position[index][2] = morph.position_deltas[vertex_index].z;
        }
        if (vertex_index < morph.normal_deltas.size()) {
            target.morph_normal[index][0] = morph.normal_deltas[vertex_index].x;
            target.morph_normal[index][1] = morph.normal_deltas[vertex_index].y;
            target.morph_normal[index][2] = morph.normal_deltas[vertex_index].z;
        }
    }
    return target;
}

constexpr char kUiVertexShader[] = R"(
struct VSInput {
    float2 Position : ATTRIB0;
    float4 Color    : ATTRIB1;
    float2 UV       : ATTRIB2;
};

struct PSInput {
    float4 Position : SV_POSITION;
    float4 Color    : COLOR0;
    float2 UV       : TEX_COORD;
};

PSInput main(VSInput input) {
    PSInput output;
    output.Position = float4(input.Position, 0.0, 1.0);
    output.Color = input.Color;
    output.UV = input.UV;
    return output;
}
)";

constexpr char kUiPixelShader[] = R"(
struct PSInput {
    float4 Position : SV_POSITION;
    float4 Color    : COLOR0;
    float2 UV       : TEX_COORD;
};

Texture2D g_Texture;
SamplerState g_Texture_sampler;

float4 main(PSInput input) : SV_TARGET {
    return input.Color * g_Texture.Sample(g_Texture_sampler, input.UV);
}
)";

constexpr char kDebugVertexShader[] = R"(
cbuffer CameraConstants {
    float4x4 ViewProjection;
};

struct VSInput {
    float3 Position : ATTRIB0;
    float4 Color : ATTRIB1;
};

struct VSOutput {
    float4 Position : SV_POSITION;
    float4 Color : COLOR0;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.Position = mul(ViewProjection, float4(input.Position, 1.0));
    output.Color = input.Color;
    return output;
}
)";

constexpr char kDebugPixelShader[] = R"(
struct PSInput {
    float4 Position : SV_POSITION;
    float4 Color : COLOR0;
};

float4 main(PSInput input) : SV_TARGET {
    return input.Color;
}
)";

constexpr char kTerrainVertexShader[] = R"(
cbuffer CameraConstants {
    float4x4 ViewProjection;
};

struct VSInput {
    float3 Position : ATTRIB0;
    float3 Normal   : ATTRIB1;
    float2 UV       : ATTRIB2;
    float4 Color    : ATTRIB3;
};

struct PSInput {
    float4 Position : SV_POSITION;
    float3 Normal   : NORMAL0;
    float2 UV       : TEXCOORD0;
    float4 Color    : COLOR0;
};

PSInput main(VSInput input) {
    PSInput output;
    output.Position = mul(ViewProjection, float4(input.Position, 1.0));
    output.Normal = input.Normal;
    output.UV = input.UV;
    output.Color = input.Color;
    return output;
}
)";

constexpr char kTerrainPixelShader[] = R"(
struct PSInput {
    float4 Position : SV_POSITION;
    float3 Normal   : NORMAL0;
    float2 UV       : TEXCOORD0;
    float4 Color    : COLOR0;
};

float4 main(PSInput input) : SV_TARGET {
    float3 light_direction = normalize(float3(-0.45, 0.85, -0.35));
    float lighting = saturate(dot(normalize(input.Normal), light_direction));
    float bands = 0.92 + 0.08 * sin(input.UV.x * 36.0 + input.UV.y * 21.0);
    float3 base_color = input.Color.rgb * bands;
    return float4(base_color * (0.35 + 0.65 * lighting), input.Color.a);
}
)";

// The menu preview and dynamic actor pass use the same semantic GPU-scene
// table. Geometry is supplied as a prototype vertex/index buffer while the
// transform and material selection stay in the persistent instance table.
constexpr char kPreviewVertexShader[] = R"(
cbuffer CameraConstants {
    float4x4 ViewProjection;
};

cbuffer InstancePassConstants {
    uint4 RequiredOptions;
    uint4 RequiredMeshId;
    uint4 RequiredMaterialId;
};

struct InstanceRecord {
    // Keep the shader representation in four 16-byte blocks.  The CPU
    // shadow record is 64 bytes as well, but a scalar/float3 declaration
    // makes std430 assign an invalid alignment to the trailing uint2 on
    // Vulkan.  The asfloat reads below preserve the exact byte layout.
    uint4 Data0;
    uint4 Data1;
    uint4 Data2;
    uint4 Data3;
};

StructuredBuffer<InstanceRecord> Instances;
StructuredBuffer<uint> InstanceIndices;

struct VSInput {
    float3 Position : ATTRIB0;
    float3 Normal : ATTRIB1;
    float2 UV : ATTRIB2;
    float4 Color : ATTRIB3;
};

struct VSOutput {
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL0;
    float4 Color : COLOR0;
};

VSOutput main(VSInput input, uint InstanceId : SV_InstanceID) {
    InstanceRecord instance = Instances[InstanceIndices[InstanceId]];
    uint2 mesh_id = instance.Data0.zw;
    uint2 material_id = instance.Data1.xy;
    float3 position = float3(asfloat(instance.Data1.z),
                             asfloat(instance.Data1.w),
                             asfloat(instance.Data2.x));
    float3 scale = float3(asfloat(instance.Data2.y),
                          asfloat(instance.Data2.z),
                          asfloat(instance.Data2.w));
    float rotation_y = asfloat(instance.Data3.x);
    uint flags = instance.Data3.y;
    float cosine = cos(rotation_y);
    float sine = sin(rotation_y);
    float3 scaled = input.Position * scale;
    float3 rotated = float3(scaled.x * cosine - scaled.z * sine,
                             scaled.y,
                             scaled.x * sine + scaled.z * cosine);
    float3 rotated_normal = float3(input.Normal.x * cosine - input.Normal.z * sine,
                                   input.Normal.y,
                                   input.Normal.x * sine + input.Normal.z * cosine);

    VSOutput output;
    if ((flags & 1) == 0 || (flags & RequiredOptions.x) == 0 ||
        mesh_id.x != RequiredMeshId.x || mesh_id.y != RequiredMeshId.y ||
        (RequiredOptions.y != 0 &&
         (material_id.x != RequiredMaterialId.x ||
          material_id.y != RequiredMaterialId.y))) {
        // Put freed slots beyond the clip volume. Alpha zero alone would
        // still allow a depth write on some backends.
        output.Position = float4(0.0, 0.0, 2.0, 1.0);
        output.Normal = float3(0.0, 1.0, 0.0);
        output.Color = float4(0.0, 0.0, 0.0, 0.0);
        return output;
    }
    output.Position = mul(ViewProjection, float4(rotated + position, 1.0));
    output.Normal = normalize(rotated_normal);
    float hue = frac((float)material_id.x * 0.000001);
    float3 base_color = float3(0.25 + 0.55 * hue,
                               0.35 + 0.35 * (1.0 - hue),
                               0.50 + 0.35 * hue);
    if ((flags & 4) != 0) {
        base_color = (flags & 8) != 0
                         ? float3(0.82, 0.22, 0.18)
                         : float3(0.18, 0.42, 0.88);
    }
    output.Color = float4(base_color * input.Color.rgb, input.Color.a);
    return output;
}
)";

constexpr char kPreviewPixelShader[] = R"(
struct PSInput {
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL0;
    float4 Color : COLOR0;
};

float4 main(PSInput input) : SV_TARGET {
    float3 light_direction = normalize(float3(-0.35, 0.80, -0.25));
    float lighting = saturate(dot(normalize(input.Normal), light_direction));
    return float4(input.Color.rgb * (0.35 + 0.65 * lighting), input.Color.a);
}
)";

constexpr const char* kSkinnedVertexShader = diligent_shaders::kSkinnedVertexShader;
constexpr const char* kSkinnedPixelShader = diligent_shaders::kSkinnedPixelShader;

using Glyph = std::array<std::uint8_t, 7>;

constexpr std::array<Glyph, 36> kFont = {{
    {{0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {{0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {{0x0F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0F}},
    {{0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
    {{0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {{0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {{0x0F, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0F}},
    {{0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {{0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F}},
    {{0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E}},
    {{0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {{0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {{0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {{0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
    {{0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {{0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {{0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}},
    {{0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {{0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {{0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {{0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {{0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}},
    {{0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}},
    {{0x11, 0x0A, 0x04, 0x04, 0x0A, 0x11, 0x11}},
    {{0x11, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {{0x1F, 0x02, 0x04, 0x08, 0x10, 0x10, 0x1F}},
    {{0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    {{0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {{0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
    {{0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}},
    {{0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
    {{0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}},
    {{0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
    {{0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {{0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
    {{0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E}},
}};

[[nodiscard]] constexpr Glyph glyph_for(char character) noexcept {
    if (character >= 'a' && character <= 'z') {
        character = static_cast<char>(character - ('a' - 'A'));
    }
    if (character >= 'A' && character <= 'Z') {
        return kFont[static_cast<std::size_t>(character - 'A')];
    }
    if (character >= '0' && character <= '9') {
        return kFont[26u + static_cast<std::size_t>(character - '0')];
    }
    switch (character) {
    case '-':
        return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    case '/':
        return {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10};
    case '.':
        return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04};
    case ':':
        return {0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00};
    case '_':
        return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F};
    default:
        return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    }
}

void append_rect(std::vector<UiVertex>& vertices,
                 float x,
                 float y,
                 float width,
                 float height,
                 foundation::Color color,
                 std::uint32_t screen_width,
                 std::uint32_t screen_height) {
    if (width <= 0.0F || height <= 0.0F || screen_width == 0 || screen_height == 0) {
        return;
    }

    const float max_x = static_cast<float>(screen_width);
    const float max_y = static_cast<float>(screen_height);
    const float left = std::clamp(x, 0.0F, max_x);
    const float top = std::clamp(y, 0.0F, max_y);
    const float right = std::clamp(x + width, 0.0F, max_x);
    const float bottom = std::clamp(y + height, 0.0F, max_y);
    if (right <= left || bottom <= top) {
        return;
    }

    const auto ndc_x = [max_x](float value) { return value / max_x * 2.0F - 1.0F; };
    const auto ndc_y = [max_y](float value) { return 1.0F - value / max_y * 2.0F; };
    const UiVertex top_left{{ndc_x(left), ndc_y(top)}, {color.r, color.g, color.b, color.a}};
    const UiVertex top_right{{ndc_x(right), ndc_y(top)}, {color.r, color.g, color.b, color.a}};
    const UiVertex bottom_left{{ndc_x(left), ndc_y(bottom)},
                               {color.r, color.g, color.b, color.a}};
    const UiVertex bottom_right{{ndc_x(right), ndc_y(bottom)},
                                {color.r, color.g, color.b, color.a}};
    vertices.insert(vertices.end(),
                    {top_left, bottom_left, top_right, top_right, bottom_left, bottom_right});
}

void append_text(std::vector<UiVertex>& vertices,
                 std::string_view text,
                 float x,
                 float y,
                 float scale,
                 foundation::Color color,
                 std::uint32_t screen_width,
                 std::uint32_t screen_height) {
    if (scale <= 0.0F) {
        return;
    }
    float cursor_x = x;
    for (const char character : text) {
        const Glyph glyph = glyph_for(character);
        if (character != ' ') {
            for (std::size_t row = 0; row < glyph.size(); ++row) {
                for (std::size_t column = 0; column < 5; ++column) {
                    if ((glyph[row] & (1u << (4u - static_cast<unsigned>(column)))) != 0u) {
                        append_rect(vertices, cursor_x + static_cast<float>(column) * scale,
                                    y + static_cast<float>(row) * scale, scale, scale, color,
                                    screen_width, screen_height);
                    }
                }
            }
        }
        cursor_x += 6.0F * scale;
    }
}

void append_rml_geometry(std::vector<UiVertex>& vertices,
                         const ui::UiDrawCommand& command,
                         std::uint32_t screen_width,
                         std::uint32_t screen_height) {
    if (command.vertices.empty() || screen_width == 0 || screen_height == 0) return;
    const auto ndc_x = [screen_width](float value) {
        return value / static_cast<float>(screen_width) * 2.0F - 1.0F;
    };
    const auto ndc_y = [screen_height](float value) {
        return 1.0F - value / static_cast<float>(screen_height) * 2.0F;
    };
    const auto emit = [&](std::size_t index) {
        if (index >= command.vertices.size()) return;
        const auto& source = command.vertices[index];
        vertices.push_back({{ndc_x(source.x), ndc_y(source.y)},
                            {source.color.r, source.color.g, source.color.b, source.color.a},
                            {source.u, source.v}});
    };
    if (command.indices.empty()) {
        for (std::size_t index = 0; index < command.vertices.size(); ++index) emit(index);
    } else {
        for (const auto index : command.indices) emit(index);
    }
}

void append_ui_frame(std::vector<UiVertex>& vertices,
                        const ui::UiRenderFrame& document,
                        std::uint32_t screen_width,
                        std::uint32_t screen_height) {
    const bool is_menu = std::any_of(document.widgets.begin(), document.widgets.end(),
                                     [](const ui::UiWidget& node) {
                                         return node.id == foundation::stable_id("menu.panel");
                                     });
    const float width = static_cast<float>(screen_width);
    const float height = static_cast<float>(screen_height);

    if (is_menu) {
        const float preview_x = width * 0.54F;
        append_rect(vertices, preview_x, 48.0F, width - preview_x - 48.0F, height - 96.0F,
                    {0.035F, 0.055F, 0.09F, 1.0F}, screen_width, screen_height);
        append_rect(vertices, preview_x, height * 0.62F, width - preview_x - 48.0F,
                    height * 0.38F - 48.0F, {0.08F, 0.16F, 0.13F, 1.0F}, screen_width,
                    screen_height);
        append_rect(vertices, preview_x + 48.0F, height * 0.42F, 140.0F, 120.0F,
                    {0.38F, 0.24F, 0.18F, 1.0F}, screen_width, screen_height);
        append_rect(vertices, preview_x + 220.0F, height * 0.34F, 110.0F, 150.0F,
                    {0.28F, 0.31F, 0.36F, 1.0F}, screen_width, screen_height);
        append_rect(vertices, preview_x + 100.0F, height * 0.57F, 24.0F, 72.0F,
                    {0.62F, 0.68F, 0.74F, 1.0F}, screen_width, screen_height);
        append_text(vertices, "WORLD PREVIEW", preview_x + 28.0F, 76.0F, 2.0F,
                    {0.64F, 0.75F, 0.86F, 1.0F}, screen_width, screen_height);
    }

    float row_y = is_menu ? 184.0F : 164.0F;
    for (const ui::UiWidget& node : document.widgets) {
        if (node.type == ui::UiWidgetType::Panel) {
            const float panel_width = std::min(node.width, width - 96.0F);
            const float panel_height = std::min(node.height, height - 96.0F);
            append_rect(vertices, 48.0F, 48.0F, panel_width, panel_height,
                        {0.025F, 0.035F, 0.06F, 0.97F}, screen_width, screen_height);
            append_text(vertices, node.text, 78.0F, 76.0F, 3.0F,
                        {0.83F, 0.88F, 0.94F, 1.0F}, screen_width, screen_height);
        } else if (node.type == ui::UiWidgetType::Button) {
            const float button_y = row_y;
            const foundation::Color fill =
                node.selected ? foundation::Color{0.10F, 0.38F, 0.67F, 1.0F}
                              : node.enabled ? foundation::Color{0.07F, 0.11F, 0.18F, 1.0F}
                                             : foundation::Color{0.045F, 0.055F, 0.075F, 1.0F};
            append_rect(vertices, 78.0F, button_y, std::min(node.width, width - 110.0F),
                        node.height, fill, screen_width, screen_height);
            append_text(vertices, node.text, 96.0F, button_y + 15.0F, 2.0F,
                        node.enabled ? foundation::Color{0.9F, 0.94F, 1.0F, 1.0F}
                                     : foundation::Color{0.38F, 0.42F, 0.48F, 1.0F},
                        screen_width, screen_height);
            row_y += node.height + 12.0F;
        } else if (node.type == ui::UiWidgetType::Separator) {
            append_rect(vertices, 78.0F, row_y + 8.0F, std::min(node.width, width - 110.0F),
                        1.0F, {0.19F, 0.25F, 0.34F, 1.0F}, screen_width, screen_height);
            row_y += 24.0F;
        } else {
            const bool title = node.text == "PROCEDURAL WORLD" || node.text == "New world";
            const bool footer = node.id == foundation::stable_id("menu.version") ||
                                node.id == foundation::stable_id("world-config.note");
            if (title) {
                append_text(vertices, node.text, 78.0F, 112.0F, 2.5F,
                            {0.48F, 0.72F, 0.94F, 1.0F}, screen_width, screen_height);
            } else if (footer) {
                append_text(vertices, node.text, 78.0F, height - 46.0F, 1.0F,
                            {0.42F, 0.5F, 0.6F, 1.0F}, screen_width, screen_height);
            } else {
                append_text(vertices, node.text, 78.0F, row_y, 1.5F,
                            {0.62F, 0.68F, 0.76F, 1.0F}, screen_width, screen_height);
                row_y += 34.0F;
            }
        }
    }
    for (const auto& command : document.commands) {
        append_rml_geometry(vertices, command, screen_width, screen_height);
    }
    if (document.widgets.empty()) {
        for (const auto& command : document.commands) {
            if (command.primitive == ui::UiDrawPrimitive::Text) {
                append_text(vertices, command.text, command.rect.x, command.rect.y, 1.5F,
                            command.color, screen_width, screen_height);
            } else {
                append_rect(vertices, command.rect.x, command.rect.y, command.rect.width,
                            command.rect.height, command.color, screen_width, screen_height);
            }
        }
    }
}

[[nodiscard]] bool is_world_mesh(foundation::StableId mesh_id) noexcept {
    return mesh_id == foundation::stable_id("mesh.world.terrain") ||
           mesh_id == foundation::stable_id("mesh.world.road") ||
           mesh_id == foundation::stable_id("mesh.world.parcel") ||
           mesh_id == foundation::stable_id("mesh.world.building") ||
           mesh_id == foundation::stable_id("mesh.world.vegetation") ||
           mesh_id == foundation::stable_id("mesh.world.fence");
}

[[nodiscard]] foundation::Color world_feature_color(
    foundation::StableId mesh_id) noexcept {
    if (mesh_id == foundation::stable_id("mesh.world.road")) {
        return {0.48F, 0.50F, 0.47F, 1.0F};
    }
    if (mesh_id == foundation::stable_id("mesh.world.parcel")) {
        return {0.30F, 0.34F, 0.23F, 1.0F};
    }
    if (mesh_id == foundation::stable_id("mesh.world.building")) {
        return {0.67F, 0.38F, 0.22F, 1.0F};
    }
    if (mesh_id == foundation::stable_id("mesh.world.fence")) {
        return {0.76F, 0.62F, 0.29F, 1.0F};
    }
    return {0.18F, 0.46F, 0.23F, 1.0F};
}

void append_world_instances(std::vector<UiVertex>& vertices,
                            const PresentationSnapshot& snapshot,
                            std::uint32_t screen_width,
                            std::uint32_t screen_height) {
    const bool has_world = std::any_of(
        snapshot.instances.begin(), snapshot.instances.end(),
        [](const RenderInstance& instance) { return is_world_mesh(instance.mesh_id); });
    if (!has_world) {
        return;
    }

    const float width = static_cast<float>(screen_width);
    const float height = static_cast<float>(screen_height);
    const float left = width * 0.54F;
    const float top = 48.0F;
    const float map_width = width - left - 48.0F;
    const float map_height = height - 96.0F;
    append_rect(vertices, left, top, map_width, map_height,
                {0.055F, 0.10F, 0.075F, 1.0F}, screen_width, screen_height);
    append_text(vertices, "WORLD PLAN", left + 18.0F, top + 18.0F, 1.5F,
                {0.63F, 0.78F, 0.66F, 1.0F}, screen_width, screen_height);

    float map_size = 1.0F;
    for (const RenderInstance& instance : snapshot.instances) {
        if (instance.mesh_id == foundation::stable_id("mesh.world.terrain")) {
            map_size = std::max(map_size, std::max(std::abs(instance.scale.x),
                                                   std::abs(instance.scale.z)));
        }
    }
    const float usable_width = map_width * 0.84F;
    const float usable_height = map_height * 0.84F;
    const float center_x = left + map_width * 0.5F;
    const float center_y = top + map_height * 0.5F;
    for (const RenderInstance& instance : snapshot.instances) {
        if (!is_world_mesh(instance.mesh_id) ||
            instance.mesh_id == foundation::stable_id("mesh.world.terrain")) {
            continue;
        }
        const float px = center_x + instance.position.x / map_size * usable_width;
        const float py = center_y + instance.position.z / map_size * usable_height;
        const float feature_width = std::clamp(
            std::abs(instance.scale.x) / map_size * usable_width, 2.0F, map_width * 0.8F);
        const float feature_height = std::clamp(
            std::abs(instance.scale.z) / map_size * usable_height, 2.0F, map_height * 0.8F);
        append_rect(vertices, px - feature_width * 0.5F, py - feature_height * 0.5F,
                    feature_width, feature_height, world_feature_color(instance.mesh_id),
                    screen_width, screen_height);
    }
}

} // namespace

struct DiligentBackend::Impl final {
    struct InstancePrototypeGpu final {
        Diligent::RefCntAutoPtr<Diligent::IBuffer> vertex_buffer;
        Diligent::RefCntAutoPtr<Diligent::IBuffer> index_buffer;
        std::size_t vertex_capacity{0};
        std::size_t index_capacity{0};
        std::shared_ptr<const RenderMesh> owner;
        std::uint64_t uploaded_revision{0};
    };

    struct SkinnedPrototypeGpu final {
        Diligent::RefCntAutoPtr<Diligent::IBuffer> vertex_buffer;
        Diligent::RefCntAutoPtr<Diligent::IBuffer> index_buffer;
        std::size_t vertex_capacity{0};
        std::size_t index_capacity{0};
        std::shared_ptr<const SkinnedMeshPrototype> owner;
        std::uint64_t uploaded_revision{0};
    };

    struct UiTextureGpu final {
        Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
        Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> srb;
    };

    explicit Impl(RenderConfig in_config) noexcept
        : config{in_config}, capabilities{in_config.backend, false,
                                           in_config.headless, true,
                                           !in_config.headless} {
        capabilities.instanced_rendering = !in_config.headless;
        capabilities.gpu_skinning = !in_config.headless;
    }

    RenderConfig config{};
    RenderCapabilities capabilities{};
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain> swap_chain;
    Diligent::RefCntAutoPtr<Diligent::IShader> ui_vertex_shader;
    Diligent::RefCntAutoPtr<Diligent::IShader> ui_pixel_shader;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> ui_pipeline;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> ui_vertex_buffer;
    std::unordered_map<std::uint64_t, UiTextureGpu> ui_textures;
    Diligent::RefCntAutoPtr<Diligent::IShader> debug_vertex_shader;
    Diligent::RefCntAutoPtr<Diligent::IShader> debug_pixel_shader;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> debug_pipeline;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> debug_srb;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> debug_vertex_buffer;
    std::vector<DebugVertex> debug_vertices;
    Diligent::RefCntAutoPtr<Diligent::IShader> terrain_vertex_shader;
    Diligent::RefCntAutoPtr<Diligent::IShader> terrain_pixel_shader;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> terrain_pipeline;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> terrain_srb;
    Diligent::RefCntAutoPtr<Diligent::IShader> preview_vertex_shader;
    Diligent::RefCntAutoPtr<Diligent::IShader> preview_pixel_shader;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> preview_pipeline;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> preview_srb;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> preview_pass_constants;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> instance_remap_buffer;
    std::size_t instance_remap_capacity{0};
    std::unordered_map<foundation::StableId, InstancePrototypeGpu> instance_prototypes_gpu;
    Diligent::RefCntAutoPtr<Diligent::IShader> skinned_vertex_shader;
    Diligent::RefCntAutoPtr<Diligent::IShader> skinned_pixel_shader;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> skinned_pipeline;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> skinned_srb;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> skinned_pass_constants;
    std::unordered_map<foundation::StableId, SkinnedPrototypeGpu> skinned_prototypes_gpu;
    std::shared_ptr<const RenderMesh> preview_prototype;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> terrain_camera_buffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> terrain_vertex_buffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> terrain_index_buffer;
    std::size_t terrain_vertex_capacity{0};
    std::size_t terrain_index_capacity{0};
    std::shared_ptr<const RenderMesh> uploaded_terrain_owner;
    std::uint64_t uploaded_terrain_revision{0};
    Diligent::RefCntAutoPtr<Diligent::IBuffer> infantry_vertex_buffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> infantry_index_buffer;
    std::size_t infantry_vertex_capacity{0};
    std::size_t infantry_index_capacity{0};
    std::shared_ptr<const RenderMesh> uploaded_infantry_owner;
    std::uint64_t uploaded_infantry_revision{0};
    Diligent::RefCntAutoPtr<Diligent::IBuffer> world_vertex_buffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> world_index_buffer;
    std::size_t world_vertex_capacity{0};
    std::size_t world_index_capacity{0};
    std::shared_ptr<const RenderMesh> uploaded_world_owner;
    std::uint64_t uploaded_world_revision{0};
    Diligent::RefCntAutoPtr<Diligent::ITexture> depth_texture;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> depth_view;
    std::vector<UiVertex> ui_vertices;
    // Persistent CPU shadow for the instance table. Keeping this in the
    // backend makes semantic-to-slot mapping survive frames while the current
    // compatibility pass still draws the menu/world preview.
    gpu_scene::GpuScene gpu_scene;
    RenderExtractor render_extractor;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> gpu_instance_buffer;
    std::size_t gpu_instance_capacity{0};
    bool frame_open{false};
};

foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>
DiligentBackend::create(RenderConfig config) {
    return create(config, {});
}

foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>
DiligentBackend::create(RenderConfig config,
                        foundation::NativeWindowHandle native_window) {
    if (!config.valid()) {
        return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid render configuration"});
    }
    const bool backend_supported =
#if defined(_WIN32)
        config.backend == RenderBackendKind::D3D12;
#else
        config.backend == RenderBackendKind::Vulkan;
#endif
    if (!backend_supported) {
        return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
            {foundation::ErrorCode::Unsupported,
             "the selected Diligent backend is not supported on this platform"});
    }
    if (!config.headless && !native_window.valid()) {
        return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
            {foundation::ErrorCode::Unsupported,
             "windowed Diligent backend requires a valid native window"});
    }

    auto impl = std::make_unique<Impl>(config);
#if defined(_WIN32)
    auto* factory = Diligent::GetEngineFactoryD3D12();
#else
    auto* factory = Diligent::GetEngineFactoryVk();
#endif
    if (factory == nullptr) {
        return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
            {foundation::ErrorCode::Internal, "Diligent graphics factory is unavailable"});
    }
    // A windowed application must surface backend failures through the
    // Result contract. Diligent's debug default is to break into a debugger
    // (and show a modal dialog) on an assertion, which otherwise turns a
    // recoverable swap-chain/device error into an unexplained process exit.
    factory->SetBreakOnError(false);

#if defined(_WIN32)
    Diligent::EngineD3D12CreateInfo create_info{};
#else
    Diligent::EngineVkCreateInfo create_info{};
#endif
    create_info.EnableValidation = config.validation;

    Diligent::IRenderDevice* device = nullptr;
    Diligent::IDeviceContext* context = nullptr;
#if defined(_WIN32)
    factory->CreateDeviceAndContextsD3D12(create_info, &device, &context);
#else
    factory->CreateDeviceAndContextsVk(create_info, &device, &context);
#endif
    if (device == nullptr || context == nullptr) {
        if (device != nullptr) {
            device->Release();
        }
        if (context != nullptr) {
            context->Release();
        }
        return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
            {foundation::ErrorCode::Internal,
             "Diligent could not create a device and immediate context"});
    }

    if (!config.headless) {
        Diligent::NativeWindow window{};
#if PLATFORM_WIN32
        if (native_window.system != foundation::NativeWindowSystem::Win32) {
            device->Release();
            context->Release();
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Unsupported,
                 "this Windows Diligent build requires a Win32 native window"});
        }
        window.hWnd = native_window.window;
#elif PLATFORM_LINUX
        if (native_window.system != foundation::NativeWindowSystem::X11) {
            device->Release();
            context->Release();
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Unsupported,
                 "this Linux Diligent build currently requires an X11 native window"});
        }
        window.pDisplay = native_window.display;
        window.WindowId = static_cast<Diligent::Uint32>(native_window.window_id);
#else
        device->Release();
        context->Release();
        return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
            {foundation::ErrorCode::Unsupported,
             "windowed Vulkan bootstrap is not implemented for this platform"});
#endif

        Diligent::SwapChainDesc swap_chain_desc{};
        swap_chain_desc.Width = config.width;
        swap_chain_desc.Height = config.height;
        swap_chain_desc.BufferCount = config.frames_in_flight;
        Diligent::ISwapChain* swap_chain = nullptr;
#if defined(_WIN32)
        Diligent::FullScreenModeDesc fullscreen_desc{};
        factory->CreateSwapChainD3D12(device, context, swap_chain_desc, fullscreen_desc, window,
                                      &swap_chain);
#else
        factory->CreateSwapChainVk(device, context, swap_chain_desc, window, &swap_chain);
#endif
        if (swap_chain == nullptr) {
            device->Release();
            context->Release();
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent could not create a swap chain"});
        }
        impl->swap_chain.Attach(swap_chain);
    }

    // The creation API transfers the initial references to the caller. Attach
    // avoids an extra AddRef and lets Impl own the exact shutdown order.
    impl->device.Attach(device);
    impl->context.Attach(context);

    if (!config.headless) {
        const auto create_shader = [&](const char* source,
                                       const char* name,
                                       Diligent::SHADER_TYPE shader_type,
                                       Diligent::RefCntAutoPtr<Diligent::IShader>& target) {
            Diligent::ShaderCreateInfo shader_info{};
            shader_info.Source = source;
            shader_info.SourceLength = std::strlen(source);
            shader_info.EntryPoint = "main";
            shader_info.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
            shader_info.Desc = Diligent::ShaderDesc{name, shader_type};

            Diligent::IShader* shader = nullptr;
            Diligent::IDataBlob* compiler_output = nullptr;
            impl->device->CreateShader(shader_info, &shader, &compiler_output);
            if (compiler_output != nullptr) {
                compiler_output->Release();
            }
            if (shader == nullptr) {
                return false;
            }
            target.Attach(shader);
            return true;
        };

        if (!create_shader(kUiVertexShader, "Genomes UI vertex shader",
                           Diligent::SHADER_TYPE_VERTEX, impl->ui_vertex_shader) ||
            !create_shader(kUiPixelShader, "Genomes UI pixel shader",
                           Diligent::SHADER_TYPE_PIXEL, impl->ui_pixel_shader) ||
            !create_shader(kDebugVertexShader, "Genomes debug line vertex shader",
                           Diligent::SHADER_TYPE_VERTEX, impl->debug_vertex_shader) ||
            !create_shader(kDebugPixelShader, "Genomes debug line pixel shader",
                           Diligent::SHADER_TYPE_PIXEL, impl->debug_pixel_shader) ||
            !create_shader(kTerrainVertexShader, "Genomes terrain vertex shader",
                           Diligent::SHADER_TYPE_VERTEX, impl->terrain_vertex_shader) ||
            !create_shader(kTerrainPixelShader, "Genomes terrain pixel shader",
                           Diligent::SHADER_TYPE_PIXEL, impl->terrain_pixel_shader) ||
            !create_shader(kPreviewVertexShader, "Genomes preview instance vertex shader",
                           Diligent::SHADER_TYPE_VERTEX, impl->preview_vertex_shader) ||
            !create_shader(kPreviewPixelShader, "Genomes preview instance pixel shader",
                           Diligent::SHADER_TYPE_PIXEL, impl->preview_pixel_shader) ||
            !create_shader(kSkinnedVertexShader, "Genomes skinned vertex shader",
                           Diligent::SHADER_TYPE_VERTEX, impl->skinned_vertex_shader) ||
            !create_shader(kSkinnedPixelShader, "Genomes skinned pixel shader",
                           Diligent::SHADER_TYPE_PIXEL, impl->skinned_pixel_shader)) {
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal, "Diligent could not compile the UI shaders"});
        }

        Diligent::LayoutElement layout[] = {
            Diligent::LayoutElement{0, 0, 2, Diligent::VT_FLOAT32, Diligent::False},
            Diligent::LayoutElement{1, 0, 4, Diligent::VT_FLOAT32, Diligent::False},
            Diligent::LayoutElement{2, 0, 2, Diligent::VT_FLOAT32, Diligent::False},
        };
        Diligent::ShaderResourceVariableDesc ui_variables[] = {
            {Diligent::SHADER_TYPE_PIXEL, "g_Texture",
             Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        };
        Diligent::SamplerDesc ui_sampler{};
        ui_sampler.MinFilter = Diligent::FILTER_TYPE_LINEAR;
        ui_sampler.MagFilter = Diligent::FILTER_TYPE_LINEAR;
        ui_sampler.MipFilter = Diligent::FILTER_TYPE_POINT;
        ui_sampler.AddressU = Diligent::TEXTURE_ADDRESS_CLAMP;
        ui_sampler.AddressV = Diligent::TEXTURE_ADDRESS_CLAMP;
        ui_sampler.AddressW = Diligent::TEXTURE_ADDRESS_CLAMP;
        Diligent::ImmutableSamplerDesc ui_samplers[] = {
            {Diligent::SHADER_TYPE_PIXEL, "g_Texture_sampler", ui_sampler},
        };
        Diligent::GraphicsPipelineStateCreateInfo pipeline_info{"Genomes UI pipeline"};
        pipeline_info.pVS = impl->ui_vertex_shader;
        pipeline_info.pPS = impl->ui_pixel_shader;
        pipeline_info.GraphicsPipeline.InputLayout =
            Diligent::InputLayoutDesc{layout, static_cast<Diligent::Uint32>(std::size(layout))};
        pipeline_info.GraphicsPipeline.PrimitiveTopology =
            Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipeline_info.GraphicsPipeline.NumRenderTargets = 1;
        pipeline_info.GraphicsPipeline.RTVFormats[0] =
            impl->swap_chain->GetDesc().ColorBufferFormat;
        pipeline_info.GraphicsPipeline.DSVFormat = Diligent::TEX_FORMAT_D32_FLOAT;
        pipeline_info.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
        pipeline_info.GraphicsPipeline.RasterizerDesc.ScissorEnable = Diligent::True;
        pipeline_info.GraphicsPipeline.DepthStencilDesc.DepthEnable = Diligent::False;
        auto& ui_blend = pipeline_info.GraphicsPipeline.BlendDesc.RenderTargets[0];
        ui_blend.BlendEnable = Diligent::True;
        ui_blend.SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
        ui_blend.DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
        ui_blend.BlendOp = Diligent::BLEND_OPERATION_ADD;
        ui_blend.SrcBlendAlpha = Diligent::BLEND_FACTOR_ONE;
        ui_blend.DestBlendAlpha = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
        ui_blend.BlendOpAlpha = Diligent::BLEND_OPERATION_ADD;
        pipeline_info.PSODesc.ResourceLayout.Variables = ui_variables;
        pipeline_info.PSODesc.ResourceLayout.NumVariables =
            static_cast<Diligent::Uint32>(std::size(ui_variables));
        pipeline_info.PSODesc.ResourceLayout.ImmutableSamplers = ui_samplers;
        pipeline_info.PSODesc.ResourceLayout.NumImmutableSamplers =
            static_cast<Diligent::Uint32>(std::size(ui_samplers));

        Diligent::IPipelineState* pipeline = nullptr;
        impl->device->CreateGraphicsPipelineState(pipeline_info, &pipeline);
        if (pipeline == nullptr) {
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent could not create the UI graphics pipeline"});
        }
        impl->ui_pipeline.Attach(pipeline);

        static constexpr std::array<std::uint8_t, 4> white_pixel{255, 255, 255, 255};
        Diligent::TextureDesc white_desc{};
        white_desc.Name = "Genomes UI white texture";
        white_desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        white_desc.Width = 1;
        white_desc.Height = 1;
        white_desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
        white_desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        white_desc.Usage = Diligent::USAGE_IMMUTABLE;
        Diligent::TextureSubResData white_subresource{white_pixel.data(), 4U};
        Diligent::TextureData white_data{&white_subresource, 1U};
        Diligent::ITexture* white_texture = nullptr;
        impl->device->CreateTexture(white_desc, &white_data, &white_texture);
        if (white_texture == nullptr) {
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent could not create the UI fallback texture"});
        }
        Impl::UiTextureGpu white_gpu{};
        white_gpu.texture.Attach(white_texture);
        Diligent::IShaderResourceBinding* white_srb = nullptr;
        impl->ui_pipeline->CreateShaderResourceBinding(&white_srb, true);
        if (white_srb == nullptr) {
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent could not create the UI fallback binding"});
        }
        white_gpu.srb.Attach(white_srb);
        auto* white_variable = white_gpu.srb->GetVariableByName(
            Diligent::SHADER_TYPE_PIXEL, "g_Texture");
        auto* white_view = white_gpu.texture->GetDefaultView(
            Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
        if (white_variable == nullptr || white_view == nullptr) {
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent UI texture variable is not reflected"});
        }
        white_variable->Set(white_view);
        impl->ui_textures.emplace(0, std::move(white_gpu));

        Diligent::BufferDesc vertex_buffer_desc{};
        vertex_buffer_desc.Name = "Genomes UI vertex buffer";
        vertex_buffer_desc.Size = static_cast<Diligent::Uint64>(sizeof(UiVertex) * kMaxUiVertices);
        vertex_buffer_desc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
        vertex_buffer_desc.Usage = Diligent::USAGE_DEFAULT;
        vertex_buffer_desc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;
        Diligent::IBuffer* vertex_buffer = nullptr;
        impl->device->CreateBuffer(vertex_buffer_desc, nullptr, &vertex_buffer);
        if (vertex_buffer != nullptr) {
            impl->ui_vertex_buffer.Attach(vertex_buffer);
        }
        impl->ui_vertices.reserve(kMaxUiVertices);

        Diligent::LayoutElement terrain_layout[] = {
            Diligent::LayoutElement{0, 0, 3, Diligent::VT_FLOAT32, Diligent::False},
            Diligent::LayoutElement{1, 0, 3, Diligent::VT_FLOAT32, Diligent::False},
            Diligent::LayoutElement{2, 0, 2, Diligent::VT_FLOAT32, Diligent::False},
            Diligent::LayoutElement{3, 0, 4, Diligent::VT_FLOAT32, Diligent::False},
        };
        Diligent::GraphicsPipelineStateCreateInfo terrain_pipeline_info{
            "Genomes terrain pipeline"};
        terrain_pipeline_info.pVS = impl->terrain_vertex_shader;
        terrain_pipeline_info.pPS = impl->terrain_pixel_shader;
        terrain_pipeline_info.GraphicsPipeline.InputLayout =
            Diligent::InputLayoutDesc{
                terrain_layout, static_cast<Diligent::Uint32>(std::size(terrain_layout))};
        terrain_pipeline_info.GraphicsPipeline.PrimitiveTopology =
            Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        terrain_pipeline_info.GraphicsPipeline.NumRenderTargets = 1;
        terrain_pipeline_info.GraphicsPipeline.RTVFormats[0] =
            impl->swap_chain->GetDesc().ColorBufferFormat;
        terrain_pipeline_info.GraphicsPipeline.DSVFormat = Diligent::TEX_FORMAT_D32_FLOAT;
        terrain_pipeline_info.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
        terrain_pipeline_info.GraphicsPipeline.DepthStencilDesc.DepthEnable = Diligent::True;
        terrain_pipeline_info.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = Diligent::True;
        terrain_pipeline_info.PSODesc.ResourceLayout.DefaultVariableType =
            Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

        Diligent::IPipelineState* terrain_pipeline = nullptr;
        impl->device->CreateGraphicsPipelineState(terrain_pipeline_info, &terrain_pipeline);
        if (terrain_pipeline == nullptr) {
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent could not create the terrain graphics pipeline"});
        }
        impl->terrain_pipeline.Attach(terrain_pipeline);

        Diligent::BufferDesc camera_buffer_desc{};
        camera_buffer_desc.Name = "Genomes terrain camera constants";
        camera_buffer_desc.Size = sizeof(CameraConstants);
        camera_buffer_desc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        camera_buffer_desc.Usage = Diligent::USAGE_DEFAULT;
        camera_buffer_desc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;
        Diligent::IBuffer* camera_buffer = nullptr;
        impl->device->CreateBuffer(camera_buffer_desc, nullptr, &camera_buffer);
        if (camera_buffer == nullptr) {
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent could not create the terrain camera buffer"});
        }
        impl->terrain_camera_buffer.Attach(camera_buffer);
        Diligent::IShaderResourceBinding* terrain_srb = nullptr;
        impl->terrain_pipeline->CreateShaderResourceBinding(&terrain_srb, true);
        if (terrain_srb == nullptr) {
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent terrain shader resource binding is not available"});
        }
        auto* camera_variable = terrain_srb->GetVariableByName(
            Diligent::SHADER_TYPE_VERTEX, "CameraConstants");
        if (camera_variable == nullptr) {
            terrain_srb->Release();
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent terrain shader camera constants are not reflected"});
        }
        camera_variable->Set(impl->terrain_camera_buffer);
        impl->terrain_srb.Attach(terrain_srb);

        Diligent::LayoutElement debug_layout[] = {
            Diligent::LayoutElement{0, 0, 3, Diligent::VT_FLOAT32, Diligent::False},
            Diligent::LayoutElement{1, 0, 4, Diligent::VT_FLOAT32, Diligent::False},
        };
        Diligent::GraphicsPipelineStateCreateInfo debug_pipeline_info{
            "Genomes debug line pipeline"};
        debug_pipeline_info.pVS = impl->debug_vertex_shader;
        debug_pipeline_info.pPS = impl->debug_pixel_shader;
        debug_pipeline_info.GraphicsPipeline.InputLayout =
            Diligent::InputLayoutDesc{
                debug_layout, static_cast<Diligent::Uint32>(std::size(debug_layout))};
        debug_pipeline_info.GraphicsPipeline.PrimitiveTopology =
            Diligent::PRIMITIVE_TOPOLOGY_LINE_LIST;
        debug_pipeline_info.GraphicsPipeline.NumRenderTargets = 1;
        debug_pipeline_info.GraphicsPipeline.RTVFormats[0] =
            impl->swap_chain->GetDesc().ColorBufferFormat;
        debug_pipeline_info.GraphicsPipeline.DSVFormat = Diligent::TEX_FORMAT_D32_FLOAT;
        debug_pipeline_info.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
        debug_pipeline_info.GraphicsPipeline.DepthStencilDesc.DepthEnable = Diligent::True;
        debug_pipeline_info.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = Diligent::False;
        debug_pipeline_info.PSODesc.ResourceLayout.DefaultVariableType =
            Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
        Diligent::IPipelineState* debug_pipeline = nullptr;
        impl->device->CreateGraphicsPipelineState(debug_pipeline_info, &debug_pipeline);
        if (debug_pipeline == nullptr) {
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent could not create the debug line graphics pipeline"});
        }
        impl->debug_pipeline.Attach(debug_pipeline);

        Diligent::BufferDesc debug_vertex_buffer_desc{};
        debug_vertex_buffer_desc.Name = "Genomes debug line vertex buffer";
        debug_vertex_buffer_desc.Size =
            static_cast<Diligent::Uint64>(sizeof(DebugVertex) * kMaxDebugVertices);
        debug_vertex_buffer_desc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
        debug_vertex_buffer_desc.Usage = Diligent::USAGE_DEFAULT;
        debug_vertex_buffer_desc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;
        Diligent::IBuffer* debug_vertex_buffer = nullptr;
        impl->device->CreateBuffer(debug_vertex_buffer_desc, nullptr, &debug_vertex_buffer);
        if (debug_vertex_buffer == nullptr) {
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent could not create the debug line vertex buffer"});
        }
        impl->debug_vertex_buffer.Attach(debug_vertex_buffer);
        Diligent::IShaderResourceBinding* debug_srb = nullptr;
        impl->debug_pipeline->CreateShaderResourceBinding(&debug_srb, true);
        if (debug_srb == nullptr) {
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent debug line shader resource binding is not available"});
        }
        auto* debug_camera_variable = debug_srb->GetVariableByName(
            Diligent::SHADER_TYPE_VERTEX, "CameraConstants");
        if (debug_camera_variable == nullptr) {
            debug_srb->Release();
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent debug line camera constants are not reflected"});
        }
        debug_camera_variable->Set(impl->terrain_camera_buffer);
        impl->debug_srb.Attach(debug_srb);
        impl->debug_vertices.reserve(kMaxDebugVertices);

        Diligent::ShaderResourceVariableDesc preview_variables[] = {
            {Diligent::SHADER_TYPE_VERTEX, "Instances",
             Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {Diligent::SHADER_TYPE_VERTEX, "InstanceIndices",
             Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        };
        Diligent::GraphicsPipelineStateCreateInfo preview_pipeline_info{
            "Genomes preview instance pipeline"};
        preview_pipeline_info.pVS = impl->preview_vertex_shader;
        preview_pipeline_info.pPS = impl->preview_pixel_shader;
        Diligent::LayoutElement instance_layout[] = {
            Diligent::LayoutElement{0, 0, 3, Diligent::VT_FLOAT32, Diligent::False},
            Diligent::LayoutElement{1, 0, 3, Diligent::VT_FLOAT32, Diligent::False},
            Diligent::LayoutElement{2, 0, 2, Diligent::VT_FLOAT32, Diligent::False},
            Diligent::LayoutElement{3, 0, 4, Diligent::VT_FLOAT32, Diligent::False},
        };
        preview_pipeline_info.GraphicsPipeline.InputLayout =
            Diligent::InputLayoutDesc{
                instance_layout, static_cast<Diligent::Uint32>(std::size(instance_layout))};
        preview_pipeline_info.GraphicsPipeline.PrimitiveTopology =
            Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        preview_pipeline_info.GraphicsPipeline.NumRenderTargets = 1;
        preview_pipeline_info.GraphicsPipeline.RTVFormats[0] =
            impl->swap_chain->GetDesc().ColorBufferFormat;
        preview_pipeline_info.GraphicsPipeline.DSVFormat = Diligent::TEX_FORMAT_D32_FLOAT;
        preview_pipeline_info.GraphicsPipeline.RasterizerDesc.CullMode =
            Diligent::CULL_MODE_BACK;
        preview_pipeline_info.GraphicsPipeline.DepthStencilDesc.DepthEnable = Diligent::True;
        preview_pipeline_info.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable =
            Diligent::True;
        preview_pipeline_info.PSODesc.ResourceLayout.DefaultVariableType =
            Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        preview_pipeline_info.PSODesc.ResourceLayout.Variables = preview_variables;
        preview_pipeline_info.PSODesc.ResourceLayout.NumVariables =
            static_cast<Diligent::Uint32>(std::size(preview_variables));

        Diligent::IPipelineState* preview_pipeline = nullptr;
        impl->device->CreateGraphicsPipelineState(preview_pipeline_info, &preview_pipeline);
        if (preview_pipeline == nullptr) {
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent could not create the preview instance pipeline"});
        }
        impl->preview_pipeline.Attach(preview_pipeline);
        auto* preview_camera_variable = impl->preview_pipeline->GetStaticVariableByName(
            Diligent::SHADER_TYPE_VERTEX, "CameraConstants");
        if (preview_camera_variable == nullptr) {
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent preview shader camera constants are not reflected"});
        }
        preview_camera_variable->Set(impl->terrain_camera_buffer);
        Diligent::BufferDesc preview_pass_desc{};
        preview_pass_desc.Name = "Genomes instance pass constants";
        preview_pass_desc.Size = sizeof(InstancePassConstants);
        preview_pass_desc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        preview_pass_desc.Usage = Diligent::USAGE_DEFAULT;
        preview_pass_desc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;
        Diligent::IBuffer* preview_pass_buffer = nullptr;
        impl->device->CreateBuffer(preview_pass_desc, nullptr, &preview_pass_buffer);
        if (preview_pass_buffer == nullptr) {
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent could not create the instance pass constants"});
        }
        impl->preview_pass_constants.Attach(preview_pass_buffer);
        auto* preview_pass_variable = impl->preview_pipeline->GetStaticVariableByName(
            Diligent::SHADER_TYPE_VERTEX, "InstancePassConstants");
        if (preview_pass_variable == nullptr) {
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent preview pass constants are not reflected"});
        }
        preview_pass_variable->Set(impl->preview_pass_constants);
        Diligent::IShaderResourceBinding* preview_srb = nullptr;
        impl->preview_pipeline->CreateShaderResourceBinding(&preview_srb, true);
        if (preview_srb == nullptr ||
            preview_srb->GetVariableByName(Diligent::SHADER_TYPE_VERTEX, "Instances") == nullptr ||
            preview_srb->GetVariableByName(Diligent::SHADER_TYPE_VERTEX, "InstanceIndices") ==
                nullptr) {
            if (preview_srb != nullptr) {
                preview_srb->Release();
            }
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent preview shader instance binding is not available"});
        }
        impl->preview_srb.Attach(preview_srb);

        Diligent::LayoutElement skinned_layout[] = {
            Diligent::LayoutElement{0, 0, 3, Diligent::VT_FLOAT32, Diligent::False, 0U, 180U},
            Diligent::LayoutElement{1, 0, 3, Diligent::VT_FLOAT32, Diligent::False, 12U, 180U},
            Diligent::LayoutElement{2, 0, 2, Diligent::VT_FLOAT32, Diligent::False, 24U, 180U},
            Diligent::LayoutElement{3, 0, 4, Diligent::VT_FLOAT32, Diligent::False, 32U, 180U},
            Diligent::LayoutElement{4, 0, 4, Diligent::VT_FLOAT32, Diligent::False, 48U, 180U},
            Diligent::LayoutElement{5, 0, 4, Diligent::VT_FLOAT32, Diligent::False, 64U, 180U},
            Diligent::LayoutElement{6, 0, 3, Diligent::VT_FLOAT32, Diligent::False, 80U, 180U},
            Diligent::LayoutElement{7, 0, 3, Diligent::VT_FLOAT32, Diligent::False, 92U, 180U},
            Diligent::LayoutElement{8, 0, 3, Diligent::VT_FLOAT32, Diligent::False, 104U, 180U},
            Diligent::LayoutElement{9, 0, 3, Diligent::VT_FLOAT32, Diligent::False, 116U, 180U},
            Diligent::LayoutElement{10, 0, 3, Diligent::VT_FLOAT32, Diligent::False, 128U, 180U},
            Diligent::LayoutElement{11, 0, 3, Diligent::VT_FLOAT32, Diligent::False, 140U, 180U},
            Diligent::LayoutElement{12, 0, 3, Diligent::VT_FLOAT32, Diligent::False, 152U, 180U},
            Diligent::LayoutElement{13, 0, 3, Diligent::VT_FLOAT32, Diligent::False, 164U, 180U},
            Diligent::LayoutElement{14, 0, 1, Diligent::VT_UINT32, Diligent::False, 176U, 180U},
        };
        Diligent::GraphicsPipelineStateCreateInfo skinned_pipeline_info{
            "Genomes skinned infantry pipeline"};
        skinned_pipeline_info.pVS = impl->skinned_vertex_shader;
        skinned_pipeline_info.pPS = impl->skinned_pixel_shader;
        skinned_pipeline_info.GraphicsPipeline.InputLayout =
            Diligent::InputLayoutDesc{
                skinned_layout, static_cast<Diligent::Uint32>(std::size(skinned_layout))};
        skinned_pipeline_info.GraphicsPipeline.PrimitiveTopology =
            Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        skinned_pipeline_info.GraphicsPipeline.NumRenderTargets = 1;
        skinned_pipeline_info.GraphicsPipeline.RTVFormats[0] =
            impl->swap_chain->GetDesc().ColorBufferFormat;
        skinned_pipeline_info.GraphicsPipeline.DSVFormat = Diligent::TEX_FORMAT_D32_FLOAT;
        skinned_pipeline_info.GraphicsPipeline.RasterizerDesc.CullMode =
            Diligent::CULL_MODE_BACK;
        skinned_pipeline_info.GraphicsPipeline.DepthStencilDesc.DepthEnable = Diligent::True;
        skinned_pipeline_info.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable =
            Diligent::True;
        skinned_pipeline_info.PSODesc.ResourceLayout.DefaultVariableType =
            Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
        Diligent::IPipelineState* skinned_pipeline = nullptr;
        impl->device->CreateGraphicsPipelineState(skinned_pipeline_info, &skinned_pipeline);
        if (skinned_pipeline == nullptr) {
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent could not create the skinned infantry pipeline"});
        }
        impl->skinned_pipeline.Attach(skinned_pipeline);
        Diligent::BufferDesc skinned_pass_desc{};
        skinned_pass_desc.Name = "Genomes skinned pass constants";
        // D3D12 CBV sizes and offsets are 256-byte aligned. The complete
        // 69-bone palette is much larger than one alignment block, so round
        // the actual structure size up instead of allocating a single block.
        constexpr std::size_t kConstantBufferAlignment = 256U;
        constexpr std::size_t kSkinnedPassBufferSize =
            ((sizeof(SkinnedPassConstants) + kConstantBufferAlignment - 1U) /
             kConstantBufferAlignment) *
            kConstantBufferAlignment;
        skinned_pass_desc.Size = static_cast<Diligent::Uint64>(kSkinnedPassBufferSize);
        skinned_pass_desc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        skinned_pass_desc.Usage = Diligent::USAGE_DEFAULT;
        skinned_pass_desc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;
        Diligent::IBuffer* skinned_pass_buffer = nullptr;
        impl->device->CreateBuffer(skinned_pass_desc, nullptr, &skinned_pass_buffer);
        if (skinned_pass_buffer == nullptr) {
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent could not create the skinned pass constants"});
        }
        impl->skinned_pass_constants.Attach(skinned_pass_buffer);
        Diligent::IShaderResourceBinding* skinned_srb = nullptr;
        impl->skinned_pipeline->CreateShaderResourceBinding(&skinned_srb, true);
        if (skinned_srb == nullptr) {
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent could not create the skinned shader resource binding"});
        }
        auto* skinned_camera_variable = skinned_srb->GetVariableByName(
            Diligent::SHADER_TYPE_VERTEX, "SkinnedPassConstants");
        if (skinned_camera_variable == nullptr) {
            skinned_srb->Release();
            return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent skinned shader constants are not reflected"});
        }
        skinned_camera_variable->Set(impl->skinned_pass_constants);
        impl->skinned_srb.Attach(skinned_srb);
    }

    impl->capabilities.initialized = true;
    auto backend = std::unique_ptr<DiligentBackend>{new DiligentBackend{std::move(impl)}};
    const RenderResult depth_result = backend->recreate_depth_buffer();
    if (!depth_result) {
        return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::failure(
            depth_result.error());
    }
    return foundation::Result<std::unique_ptr<DiligentBackend>, foundation::Error>::success(
        std::move(backend));
}

DiligentBackend::DiligentBackend(std::unique_ptr<Impl> impl) noexcept
    : impl_{std::move(impl)} {}

DiligentBackend::~DiligentBackend() {
    shutdown();
}

RenderResult DiligentBackend::recreate_depth_buffer() noexcept {
    if (impl_ == nullptr || !impl_->swap_chain || !impl_->device) {
        return RenderResult::success();
    }

    const Diligent::SwapChainDesc swap_chain_desc = impl_->swap_chain->GetDesc();
    if (swap_chain_desc.Width == 0 || swap_chain_desc.Height == 0) {
        return RenderResult::failure(
            {foundation::ErrorCode::InvalidArgument, "Diligent depth buffer has invalid size"});
    }

    impl_->depth_view.Release();
    impl_->depth_texture.Release();
    Diligent::TextureDesc depth_desc{};
    depth_desc.Name = "Genomes depth buffer";
    depth_desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    depth_desc.Width = swap_chain_desc.Width;
    depth_desc.Height = swap_chain_desc.Height;
    depth_desc.MipLevels = 1;
    depth_desc.SampleCount = 1;
    depth_desc.Format = Diligent::TEX_FORMAT_D32_FLOAT;
    depth_desc.BindFlags = Diligent::BIND_DEPTH_STENCIL;

    Diligent::ITexture* depth_texture = nullptr;
    impl_->device->CreateTexture(depth_desc, nullptr, &depth_texture);
    if (depth_texture == nullptr) {
        return RenderResult::failure(
            {foundation::ErrorCode::Internal, "Diligent could not create the depth buffer"});
    }
    impl_->depth_texture.Attach(depth_texture);
    impl_->depth_view = impl_->depth_texture->GetDefaultView(
        Diligent::TEXTURE_VIEW_DEPTH_STENCIL);
    if (!impl_->depth_view) {
        impl_->depth_texture.Release();
        return RenderResult::failure(
            {foundation::ErrorCode::Internal, "Diligent depth buffer has no depth view"});
    }
    return RenderResult::success();
}

RenderCapabilities DiligentBackend::capabilities() const noexcept {
    return impl_ != nullptr ? impl_->capabilities : RenderCapabilities{};
}

RenderResult DiligentBackend::begin_frame() noexcept {
    if (impl_ == nullptr || !impl_->capabilities.initialized) {
        return RenderResult::failure(
            {foundation::ErrorCode::InvalidState, "Diligent backend is shut down"});
    }
    if (impl_->frame_open) {
        return RenderResult::failure(
            {foundation::ErrorCode::InvalidState, "Diligent frame is already open"});
    }

    if (impl_->swap_chain) {
        auto* render_target = impl_->swap_chain->GetCurrentBackBufferRTV();
        if (render_target == nullptr) {
            return RenderResult::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent swap chain has no current back buffer"});
        }
        Diligent::ITextureView* render_targets[] = {render_target};
        impl_->context->SetRenderTargets(
            1, render_targets, impl_->depth_view,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        constexpr float clear_color[] = {0.035F, 0.055F, 0.085F, 1.0F};
        impl_->context->ClearRenderTarget(
            render_target, clear_color, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        impl_->context->ClearDepthStencil(
            impl_->depth_view, Diligent::CLEAR_DEPTH_FLAG, 1.0F, 0,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    impl_->frame_open = true;
    return RenderResult::success();
}

RenderResult DiligentBackend::draw_meshes(const PresentationSnapshot& snapshot) noexcept {
    if (impl_ == nullptr || !impl_->capabilities.initialized) {
        return RenderResult::failure(
            {foundation::ErrorCode::InvalidState, "Diligent backend is shut down"});
    }
    if (!impl_->frame_open) {
        return RenderResult::failure(
            {foundation::ErrorCode::InvalidState, "Diligent frame is not open"});
    }
    // D3D12 drivers can reject the dynamic terrain/index submission while the
    // scene is still being populated. Keep the frame alive and let the
    // instance/UI passes present the deterministic battlefield immediately.
    // The terrain buffers remain available for the dedicated terrain pass.
    void* mapped_data = nullptr;
    const Diligent::Uint64 offsets[] = {0};

    if (!impl_->swap_chain || !impl_->terrain_pipeline || !impl_->terrain_camera_buffer ||
        !snapshot.terrain_mesh || snapshot.terrain_mesh->vertices.empty() ||
        snapshot.terrain_mesh->indices.empty()) {
        // Terrain is optional; the other presentation passes remain active.
    } else {

    const RenderMesh& mesh = *snapshot.terrain_mesh;
    if (mesh.vertices.size() > std::numeric_limits<std::uint32_t>::max() ||
        mesh.indices.size() > std::numeric_limits<std::uint32_t>::max() ||
        mesh.indices.size() % 3 != 0) {
        return RenderResult::failure(
            {foundation::ErrorCode::InvalidArgument, "terrain mesh exceeds GPU draw limits"});
    }

    const std::size_t vertex_bytes = mesh.vertices.size() * sizeof(RenderMeshVertex);
    const std::size_t index_bytes = mesh.indices.size() * sizeof(std::uint32_t);
    bool buffers_recreated = false;
    if (vertex_bytes > impl_->terrain_vertex_capacity) {
        const std::size_t capacity = std::max<std::size_t>(vertex_bytes, 64U * 1024U);
        Diligent::BufferDesc desc{"Genomes terrain vertex buffer",
                                  static_cast<Diligent::Uint64>(capacity),
                                  Diligent::BIND_VERTEX_BUFFER, Diligent::USAGE_DYNAMIC,
                                  Diligent::CPU_ACCESS_WRITE};
        Diligent::IBuffer* buffer = nullptr;
        impl_->device->CreateBuffer(desc, nullptr, &buffer);
        if (buffer == nullptr) {
            return RenderResult::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent could not create the terrain vertex buffer"});
        }
        impl_->terrain_vertex_buffer.Attach(buffer);
        impl_->terrain_vertex_capacity = capacity;
        buffers_recreated = true;
    }
    if (index_bytes > impl_->terrain_index_capacity) {
        const std::size_t capacity = std::max<std::size_t>(index_bytes, 64U * 1024U);
        Diligent::BufferDesc desc{"Genomes terrain index buffer",
                                  static_cast<Diligent::Uint64>(capacity),
                                  Diligent::BIND_INDEX_BUFFER, Diligent::USAGE_DYNAMIC,
                                  Diligent::CPU_ACCESS_WRITE};
        Diligent::IBuffer* buffer = nullptr;
        impl_->device->CreateBuffer(desc, nullptr, &buffer);
        if (buffer == nullptr) {
            return RenderResult::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent could not create the terrain index buffer"});
        }
        impl_->terrain_index_buffer.Attach(buffer);
        impl_->terrain_index_capacity = capacity;
        buffers_recreated = true;
    }

    if (buffers_recreated || impl_->uploaded_terrain_revision != mesh.revision) {
        impl_->context->MapBuffer(impl_->terrain_vertex_buffer, Diligent::MAP_WRITE,
                                  Diligent::MAP_FLAG_DISCARD, mapped_data);
        if (mapped_data == nullptr) {
            return RenderResult::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent could not map the terrain vertex buffer"});
        }
        std::memcpy(mapped_data, mesh.vertices.data(), vertex_bytes);
        impl_->context->UnmapBuffer(impl_->terrain_vertex_buffer, Diligent::MAP_WRITE);

        mapped_data = nullptr;
        impl_->context->MapBuffer(impl_->terrain_index_buffer, Diligent::MAP_WRITE,
                                  Diligent::MAP_FLAG_DISCARD, mapped_data);
        if (mapped_data == nullptr) {
            return RenderResult::failure(
                {foundation::ErrorCode::Internal,
                 "Diligent could not map the terrain index buffer"});
        }
        std::memcpy(mapped_data, mesh.indices.data(), index_bytes);
        impl_->context->UnmapBuffer(impl_->terrain_index_buffer, Diligent::MAP_WRITE);
        impl_->uploaded_terrain_owner = snapshot.terrain_mesh;
        impl_->uploaded_terrain_revision = mesh.revision;
    }

    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float min_z = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();
    float max_z = std::numeric_limits<float>::lowest();
    for (const RenderMeshVertex& vertex : mesh.vertices) {
        min_x = std::min(min_x, vertex.position.x);
        min_y = std::min(min_y, vertex.position.y);
        min_z = std::min(min_z, vertex.position.z);
        max_x = std::max(max_x, vertex.position.x);
        max_y = std::max(max_y, vertex.position.y);
        max_z = std::max(max_z, vertex.position.z);
    }
    const foundation::Vec3 auto_center{(min_x + max_x) * 0.5F, (min_y + max_y) * 0.5F,
                                       (min_z + max_z) * 0.5F};
    const float extent = std::max({max_x - min_x, max_y - min_y, max_z - min_z, 1.0F});
    const foundation::Vec3 auto_eye{auto_center.x + extent * 0.78F,
                                    auto_center.y + extent * 0.92F,
                                    auto_center.z + extent * 0.82F};
    const bool use_scene_camera = snapshot.camera.enabled && snapshot.camera.valid();
    const foundation::Vec3 eye = use_scene_camera ? snapshot.camera.position : auto_eye;
    const foundation::Vec3 center = use_scene_camera ? snapshot.camera.target : auto_center;
    const foundation::Vec3 up = use_scene_camera ? snapshot.camera.up
                                                  : foundation::Vec3{0.0F, 1.0F, 0.0F};
    const float vertical_fov = use_scene_camera ? snapshot.camera.vertical_fov : 0.9F;
    const float near_plane = use_scene_camera
                                 ? snapshot.camera.near_plane
                                 : std::max(0.1F, extent * 0.002F);
    const float far_plane = use_scene_camera ? snapshot.camera.far_plane : extent * 12.0F;
    const Diligent::SwapChainDesc swap_chain_desc = impl_->swap_chain->GetDesc();
    const float aspect = static_cast<float>(swap_chain_desc.Width) /
                         static_cast<float>(std::max(1U, swap_chain_desc.Height));
    const Mat4 view_projection = multiply(
        perspective(vertical_fov, aspect, near_plane, far_plane), look_at(eye, center, up));

    CameraConstants camera{};
    std::memcpy(camera.view_projection, view_projection.values, sizeof(camera.view_projection));
    impl_->context->UpdateBuffer(
        impl_->terrain_camera_buffer, 0, sizeof(camera), &camera,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    impl_->context->SetPipelineState(impl_->terrain_pipeline);
    impl_->context->CommitShaderResources(
        impl_->terrain_srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Diligent::IBuffer* vertex_buffers[] = {impl_->terrain_vertex_buffer};
    impl_->context->SetVertexBuffers(
        0, 1, vertex_buffers, offsets, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
    impl_->context->SetIndexBuffer(impl_->terrain_index_buffer, 0,
                                   Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    impl_->context->DrawIndexed(Diligent::DrawIndexedAttribs{
        static_cast<Diligent::Uint32>(mesh.indices.size()), Diligent::VT_UINT32,
        Diligent::DRAW_FLAG_NONE});
    }

    if (snapshot.world_mesh && !snapshot.world_mesh->vertices.empty() &&
        !snapshot.world_mesh->indices.empty()) {
        const RenderMesh& world_mesh = *snapshot.world_mesh;
        if (world_mesh.vertices.size() > std::numeric_limits<std::uint32_t>::max() ||
            world_mesh.indices.size() > std::numeric_limits<std::uint32_t>::max() ||
            world_mesh.indices.size() % 3 != 0) {
            return RenderResult::failure(
                {foundation::ErrorCode::InvalidArgument, "world mesh exceeds GPU draw limits"});
        }
        const std::size_t world_vertex_bytes =
            world_mesh.vertices.size() * sizeof(RenderMeshVertex);
        const std::size_t world_index_bytes = world_mesh.indices.size() * sizeof(std::uint32_t);
        bool world_buffers_recreated = false;
        if (world_vertex_bytes > impl_->world_vertex_capacity) {
            const std::size_t capacity = std::max<std::size_t>(world_vertex_bytes, 64U * 1024U);
            Diligent::BufferDesc desc{"Genomes world vertex buffer",
                                      static_cast<Diligent::Uint64>(capacity),
                                      Diligent::BIND_VERTEX_BUFFER, Diligent::USAGE_DYNAMIC,
                                      Diligent::CPU_ACCESS_WRITE};
            Diligent::IBuffer* buffer = nullptr;
            impl_->device->CreateBuffer(desc, nullptr, &buffer);
            if (buffer == nullptr) {
                return RenderResult::failure(
                    {foundation::ErrorCode::Internal,
                     "Diligent could not create the world vertex buffer"});
            }
            impl_->world_vertex_buffer.Attach(buffer);
            impl_->world_vertex_capacity = capacity;
            world_buffers_recreated = true;
        }
        if (world_index_bytes > impl_->world_index_capacity) {
            const std::size_t capacity = std::max<std::size_t>(world_index_bytes, 64U * 1024U);
            Diligent::BufferDesc desc{"Genomes world index buffer",
                                      static_cast<Diligent::Uint64>(capacity),
                                      Diligent::BIND_INDEX_BUFFER, Diligent::USAGE_DYNAMIC,
                                      Diligent::CPU_ACCESS_WRITE};
            Diligent::IBuffer* buffer = nullptr;
            impl_->device->CreateBuffer(desc, nullptr, &buffer);
            if (buffer == nullptr) {
                return RenderResult::failure(
                    {foundation::ErrorCode::Internal,
                     "Diligent could not create the world index buffer"});
            }
            impl_->world_index_buffer.Attach(buffer);
            impl_->world_index_capacity = capacity;
            world_buffers_recreated = true;
        }
    if (world_buffers_recreated || impl_->uploaded_world_revision != world_mesh.revision) {
            mapped_data = nullptr;
            impl_->context->MapBuffer(impl_->world_vertex_buffer, Diligent::MAP_WRITE,
                                      Diligent::MAP_FLAG_DISCARD, mapped_data);
            if (mapped_data == nullptr) {
                return RenderResult::failure(
                    {foundation::ErrorCode::Internal,
                     "Diligent could not map the world vertex buffer"});
            }
            std::memcpy(mapped_data, world_mesh.vertices.data(), world_vertex_bytes);
            impl_->context->UnmapBuffer(impl_->world_vertex_buffer, Diligent::MAP_WRITE);
            mapped_data = nullptr;
            impl_->context->MapBuffer(impl_->world_index_buffer, Diligent::MAP_WRITE,
                                      Diligent::MAP_FLAG_DISCARD, mapped_data);
            if (mapped_data == nullptr) {
                return RenderResult::failure(
                    {foundation::ErrorCode::Internal,
                     "Diligent could not map the world index buffer"});
            }
            std::memcpy(mapped_data, world_mesh.indices.data(), world_index_bytes);
            impl_->context->UnmapBuffer(impl_->world_index_buffer, Diligent::MAP_WRITE);
            impl_->uploaded_world_owner = snapshot.world_mesh;
        impl_->uploaded_world_revision = world_mesh.revision;
        }
        impl_->context->SetPipelineState(impl_->terrain_pipeline);
        Diligent::IBuffer* world_vertex_buffers[] = {impl_->world_vertex_buffer};
        impl_->context->SetVertexBuffers(
            0, 1, world_vertex_buffers, offsets,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
        impl_->context->SetIndexBuffer(impl_->world_index_buffer, 0,
                                       Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        impl_->context->DrawIndexed(Diligent::DrawIndexedAttribs{
            static_cast<Diligent::Uint32>(world_mesh.indices.size()), Diligent::VT_UINT32,
            Diligent::DRAW_FLAG_NONE});
    }

    const bool has_dynamic_instances = std::any_of(
        snapshot.instances.begin(), snapshot.instances.end(), [](const RenderInstance& instance) {
            return (instance.flags & RenderInstanceFlagDynamic) != 0;
        });
    if (snapshot.infantry_mesh && !has_dynamic_instances &&
        !snapshot.infantry_mesh->vertices.empty() &&
        !snapshot.infantry_mesh->indices.empty()) {
        const RenderMesh& infantry_mesh = *snapshot.infantry_mesh;
        if (infantry_mesh.vertices.size() > std::numeric_limits<std::uint32_t>::max() ||
            infantry_mesh.indices.size() > std::numeric_limits<std::uint32_t>::max() ||
            infantry_mesh.indices.size() % 3 != 0) {
            return RenderResult::failure(
                {foundation::ErrorCode::InvalidArgument,
                 "infantry mesh exceeds GPU draw limits"});
        }
        const std::size_t infantry_vertex_bytes =
            infantry_mesh.vertices.size() * sizeof(RenderMeshVertex);
        const std::size_t infantry_index_bytes =
            infantry_mesh.indices.size() * sizeof(std::uint32_t);
        bool infantry_buffers_recreated = false;
        if (infantry_vertex_bytes > impl_->infantry_vertex_capacity) {
            const std::size_t capacity =
                std::max<std::size_t>(infantry_vertex_bytes, 16U * 1024U);
            Diligent::BufferDesc desc{"Genomes infantry vertex buffer",
                                      static_cast<Diligent::Uint64>(capacity),
                                      Diligent::BIND_VERTEX_BUFFER, Diligent::USAGE_DYNAMIC,
                                      Diligent::CPU_ACCESS_WRITE};
            Diligent::IBuffer* buffer = nullptr;
            impl_->device->CreateBuffer(desc, nullptr, &buffer);
            if (buffer == nullptr) {
                return RenderResult::failure(
                    {foundation::ErrorCode::Internal,
                     "Diligent could not create the infantry vertex buffer"});
            }
            impl_->infantry_vertex_buffer.Attach(buffer);
            impl_->infantry_vertex_capacity = capacity;
            infantry_buffers_recreated = true;
        }
        if (infantry_index_bytes > impl_->infantry_index_capacity) {
            const std::size_t capacity =
                std::max<std::size_t>(infantry_index_bytes, 16U * 1024U);
            Diligent::BufferDesc desc{"Genomes infantry index buffer",
                                      static_cast<Diligent::Uint64>(capacity),
                                      Diligent::BIND_INDEX_BUFFER, Diligent::USAGE_DYNAMIC,
                                      Diligent::CPU_ACCESS_WRITE};
            Diligent::IBuffer* buffer = nullptr;
            impl_->device->CreateBuffer(desc, nullptr, &buffer);
            if (buffer == nullptr) {
                return RenderResult::failure(
                    {foundation::ErrorCode::Internal,
                     "Diligent could not create the infantry index buffer"});
            }
            impl_->infantry_index_buffer.Attach(buffer);
            impl_->infantry_index_capacity = capacity;
            infantry_buffers_recreated = true;
        }
        if (infantry_buffers_recreated ||
            impl_->uploaded_infantry_revision != infantry_mesh.revision) {
            mapped_data = nullptr;
            impl_->context->MapBuffer(impl_->infantry_vertex_buffer, Diligent::MAP_WRITE,
                                      Diligent::MAP_FLAG_DISCARD, mapped_data);
            if (mapped_data == nullptr) {
                return RenderResult::failure(
                    {foundation::ErrorCode::Internal,
                     "Diligent could not map the infantry vertex buffer"});
            }
            std::memcpy(mapped_data, infantry_mesh.vertices.data(), infantry_vertex_bytes);
            impl_->context->UnmapBuffer(impl_->infantry_vertex_buffer, Diligent::MAP_WRITE);

            mapped_data = nullptr;
            impl_->context->MapBuffer(impl_->infantry_index_buffer, Diligent::MAP_WRITE,
                                      Diligent::MAP_FLAG_DISCARD, mapped_data);
            if (mapped_data == nullptr) {
                return RenderResult::failure(
                    {foundation::ErrorCode::Internal,
                     "Diligent could not map the infantry index buffer"});
            }
            std::memcpy(mapped_data, infantry_mesh.indices.data(), infantry_index_bytes);
            impl_->context->UnmapBuffer(impl_->infantry_index_buffer, Diligent::MAP_WRITE);
            impl_->uploaded_infantry_owner = snapshot.infantry_mesh;
            impl_->uploaded_infantry_revision = infantry_mesh.revision;
        }
        impl_->context->SetPipelineState(impl_->terrain_pipeline);
        Diligent::IBuffer* infantry_vertex_buffers[] = {impl_->infantry_vertex_buffer};
        impl_->context->SetVertexBuffers(
            0, 1, infantry_vertex_buffers, offsets,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
        impl_->context->SetIndexBuffer(impl_->infantry_index_buffer, 0,
                                       Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        impl_->context->DrawIndexed(Diligent::DrawIndexedAttribs{
            static_cast<Diligent::Uint32>(infantry_mesh.indices.size()), Diligent::VT_UINT32,
            Diligent::DRAW_FLAG_NONE});
    }
    return RenderResult::success();
}

RenderResult DiligentBackend::draw_instances(const PresentationSnapshot& snapshot) noexcept {
    if (impl_ == nullptr || !impl_->capabilities.initialized) {
        return RenderResult::failure(
            {foundation::ErrorCode::InvalidState, "Diligent backend is shut down"});
    }
    if (!impl_->frame_open) {
        return RenderResult::failure(
            {foundation::ErrorCode::InvalidState, "Diligent frame is not open"});
    }

    try {
        const auto extraction = impl_->render_extractor.extract(snapshot);
        if (!extraction) {
            return RenderResult::failure(extraction.error());
        }
        const auto upload = impl_->gpu_scene.apply(extraction.value().changes);
        if (!upload) {
            return RenderResult::failure(upload.error());
        }

        gpu_scene::GpuUploadBatch staged_upload = upload.value();
        if (impl_->gpu_scene.capacity() > impl_->gpu_instance_capacity) {
            const std::size_t capacity = impl_->gpu_scene.capacity();
            if (capacity > std::numeric_limits<std::size_t>::max() /
                               sizeof(gpu_scene::GpuInstanceRecord)) {
                return RenderResult::failure(
                    {foundation::ErrorCode::OutOfRange, "GPU instance table size overflow"});
            }
            const Diligent::BufferDesc instance_desc{
                "Genomes GPU instance table",
                static_cast<Diligent::Uint64>(capacity * sizeof(gpu_scene::GpuInstanceRecord)),
                Diligent::BIND_SHADER_RESOURCE,
                Diligent::USAGE_DEFAULT,
                Diligent::CPU_ACCESS_NONE,
                Diligent::BUFFER_MODE_STRUCTURED,
                static_cast<Diligent::Uint32>(sizeof(gpu_scene::GpuInstanceRecord))};
            Diligent::IBuffer* instance_buffer = nullptr;
            impl_->device->CreateBuffer(instance_desc, nullptr, &instance_buffer);
            if (instance_buffer == nullptr) {
                return RenderResult::failure(
                    {foundation::ErrorCode::Internal,
                     "Diligent could not create the GPU instance table"});
            }
            impl_->gpu_instance_buffer.Attach(instance_buffer);
            impl_->gpu_instance_capacity = capacity;
            staged_upload = impl_->gpu_scene.fullUpload();
        }
        if (impl_->gpu_instance_buffer) {
            for (const gpu_scene::GpuUploadRange& range : staged_upload.ranges) {
                if (range.count == 0 || range.payload_offset >= staged_upload.payload.size() ||
                    range.count > staged_upload.payload.size() - range.payload_offset) {
                    return RenderResult::failure(
                        {foundation::ErrorCode::InvalidArgument,
                         "GPU instance upload range is outside its payload"});
                }
                const auto byte_offset = static_cast<Diligent::Uint64>(
                    range.first_slot * sizeof(gpu_scene::GpuInstanceRecord));
                const auto byte_size = static_cast<Diligent::Uint64>(
                    range.count * sizeof(gpu_scene::GpuInstanceRecord));
                impl_->context->UpdateBuffer(
                    impl_->gpu_instance_buffer, byte_offset, byte_size,
                    staged_upload.payload.data() + range.payload_offset,
                    Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            }
        }

        // Native infantry is published as an immutable skinned prototype plus
        // an instance palette.  Consume that contract directly on the GPU;
        // the ordinary RenderMesh path below remains available for scenes and
        // backends that do not publish skinned data.
        // The GPU pass owns the UnitLab draw when its immutable prototype and
        // palette are available. The CPU deformer remains as the headless and
        // backend fallback without changing the presentation contract.
        const bool gpu_skinning_enabled = impl_->capabilities.gpu_skinning;
        if (gpu_skinning_enabled && impl_->swap_chain && impl_->skinned_pipeline &&
            impl_->skinned_srb && impl_->skinned_pass_constants &&
            !snapshot.skinned_prototypes.empty() &&
            !snapshot.skinned_palettes.empty()) {
            const auto find_instances = [&snapshot](foundation::StableId mesh_id) {
                std::vector<const RenderInstance*> result;
                for (const RenderInstance& instance : snapshot.instances) {
                    if (instance.mesh_id == mesh_id) {
                        result.push_back(&instance);
                    }
                }
                return result;
            };
            const auto find_palette = [&snapshot](foundation::StableId instance_id)
                -> const SkinnedBonePalette* {
                for (const SkinnedBonePalette& palette : snapshot.skinned_palettes) {
                    if (palette.instance_id == instance_id) {
                        return &palette;
                    }
                }
                return nullptr;
            };

            for (const std::shared_ptr<const SkinnedMeshPrototype>& prototype_owner :
                 snapshot.skinned_prototypes) {
                if (!prototype_owner || prototype_owner->vertices.empty() ||
                    prototype_owner->indices.empty()) {
                    continue;
                }
                const std::vector<const RenderInstance*> instances =
                    find_instances(prototype_owner->mesh_id);
                if (instances.empty()) {
                    return RenderResult::failure(
                        {foundation::ErrorCode::InvalidState,
                         "skinned prototype has no matching instance palette"});
                }
                if (prototype_owner->indices.size() >
                        std::numeric_limits<std::uint32_t>::max() ||
                    prototype_owner->indices.size() % 3U != 0U ||
                    find_palette(instances.front()->object_id) == nullptr ||
                    find_palette(instances.front()->object_id)->matrices.size() <
                        kInfantryBonePaletteSize) {
                    return RenderResult::failure(
                        {foundation::ErrorCode::InvalidArgument,
                         "skinned infantry data exceeds Diligent limits"});
                }

                const std::size_t vertex_bytes =
                    prototype_owner->vertices.size() * sizeof(SkinnedGpuVertex);
                const std::size_t index_bytes =
                    prototype_owner->indices.size() * sizeof(std::uint32_t);
                Impl::SkinnedPrototypeGpu& prototype_gpu =
                    impl_->skinned_prototypes_gpu[prototype_owner->mesh_id];
                const bool changed = prototype_gpu.uploaded_revision != prototype_owner->revision;
                if (!prototype_gpu.vertex_buffer || changed ||
                    vertex_bytes > prototype_gpu.vertex_capacity) {
                    std::vector<SkinnedGpuVertex> gpu_vertices;
                    gpu_vertices.reserve(prototype_owner->vertices.size());
                    for (std::size_t index = 0U; index < prototype_owner->vertices.size(); ++index) {
                        gpu_vertices.push_back(to_skinned_gpu_vertex(
                            prototype_owner->vertices[index], *prototype_owner, index));
                    }
                    Diligent::BufferDesc desc{};
                    desc.Name = "Genomes skinned prototype vertex buffer";
                    desc.Size = static_cast<Diligent::Uint64>(
                        std::max<std::size_t>(vertex_bytes, 16U * 1024U));
                    desc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
                    desc.Usage = Diligent::USAGE_DEFAULT;
                    desc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;
                    const Diligent::BufferData data{
                        gpu_vertices.data(), static_cast<Diligent::Uint64>(vertex_bytes)};
                    Diligent::IBuffer* buffer = nullptr;
                    impl_->device->CreateBuffer(desc, &data, &buffer);
                    if (buffer == nullptr) {
                        return RenderResult::failure(
                            {foundation::ErrorCode::Internal,
                             "Diligent could not create the skinned vertex buffer"});
                    }
                    prototype_gpu.vertex_buffer.Attach(buffer);
                    prototype_gpu.vertex_capacity = static_cast<std::size_t>(desc.Size);
                }
                if (!prototype_gpu.index_buffer || changed ||
                    index_bytes > prototype_gpu.index_capacity) {
                    Diligent::BufferDesc desc{};
                    desc.Name = "Genomes skinned prototype index buffer";
                    desc.Size = static_cast<Diligent::Uint64>(
                        std::max<std::size_t>(index_bytes, 16U * 1024U));
                    desc.BindFlags = Diligent::BIND_INDEX_BUFFER;
                    desc.Usage = Diligent::USAGE_DEFAULT;
                    desc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;
                    const Diligent::BufferData data{
                        prototype_owner->indices.data(),
                        static_cast<Diligent::Uint64>(index_bytes)};
                    Diligent::IBuffer* buffer = nullptr;
                    impl_->device->CreateBuffer(desc, &data, &buffer);
                    if (buffer == nullptr) {
                        return RenderResult::failure(
                            {foundation::ErrorCode::Internal,
                             "Diligent could not create the skinned index buffer"});
                    }
                    prototype_gpu.index_buffer.Attach(buffer);
                    prototype_gpu.index_capacity = static_cast<std::size_t>(desc.Size);
                }
                prototype_gpu.owner = prototype_owner;
                prototype_gpu.uploaded_revision = prototype_owner->revision;

                foundation::Vec3 min_corner{std::numeric_limits<float>::max(),
                                            std::numeric_limits<float>::max(),
                                            std::numeric_limits<float>::max()};
                foundation::Vec3 max_corner{std::numeric_limits<float>::lowest(),
                                            std::numeric_limits<float>::lowest(),
                                            std::numeric_limits<float>::lowest()};
                for (const SkinnedMeshVertex& vertex : prototype_owner->vertices) {
                    min_corner.x = std::min(min_corner.x, vertex.position.x);
                    min_corner.y = std::min(min_corner.y, vertex.position.y);
                    min_corner.z = std::min(min_corner.z, vertex.position.z);
                    max_corner.x = std::max(max_corner.x, vertex.position.x);
                    max_corner.y = std::max(max_corner.y, vertex.position.y);
                    max_corner.z = std::max(max_corner.z, vertex.position.z);
                }
                const foundation::Vec3 center{
                    (min_corner.x + max_corner.x) * 0.5F,
                    (min_corner.y + max_corner.y) * 0.5F,
                    (min_corner.z + max_corner.z) * 0.5F};
                const float extent = std::max({max_corner.x - min_corner.x,
                                               max_corner.y - min_corner.y,
                                               max_corner.z - min_corner.z, 1.0F});
                const foundation::Vec3 auto_eye{center.x + extent * 2.0F,
                                               center.y + extent * 1.35F,
                                               center.z + extent * 2.0F};
                const bool use_scene_camera = snapshot.camera.enabled && snapshot.camera.valid();
                const foundation::Vec3 eye = use_scene_camera ? snapshot.camera.position : auto_eye;
                const foundation::Vec3 target = use_scene_camera ? snapshot.camera.target : center;
                const foundation::Vec3 up = use_scene_camera ? snapshot.camera.up
                                                              : foundation::Vec3{0.0F, 1.0F, 0.0F};
                const float vertical_fov = use_scene_camera ? snapshot.camera.vertical_fov : 0.9F;
                const float near_plane = use_scene_camera
                                             ? snapshot.camera.near_plane
                                             : std::max(0.05F, extent * 0.01F);
                const float far_plane = use_scene_camera ? snapshot.camera.far_plane : extent * 8.0F;
                const Diligent::SwapChainDesc swap_chain_desc = impl_->swap_chain->GetDesc();
                const float aspect = static_cast<float>(swap_chain_desc.Width) /
                                     static_cast<float>(std::max(1U, swap_chain_desc.Height));
                const Mat4 view_projection = multiply(
                    perspective(vertical_fov, aspect, near_plane, far_plane),
                    look_at(eye, target, up));

                for (const RenderInstance* draw_instance : instances) {
                    const SkinnedBonePalette* draw_palette =
                        find_palette(draw_instance->object_id);
                    if (draw_palette == nullptr ||
                        draw_palette->matrices.size() < kInfantryBonePaletteSize) {
                        return RenderResult::failure(
                            {foundation::ErrorCode::InvalidState,
                             "skinned instance has no complete bone palette"});
                    }
                    SkinnedPassConstants pass_constants{};
                    std::memcpy(pass_constants.view_projection, view_projection.values,
                                sizeof(pass_constants.view_projection));
                    pass_constants.object_position_scale[0] = draw_instance->position.x;
                    pass_constants.object_position_scale[1] = draw_instance->position.y;
                    pass_constants.object_position_scale[2] = draw_instance->position.z;
                    pass_constants.object_position_scale[3] = 1.0F;
                    pass_constants.object_scale_rotation[0] = draw_instance->scale.x;
                    pass_constants.object_scale_rotation[1] = draw_instance->scale.y;
                    pass_constants.object_scale_rotation[2] = draw_instance->scale.z;
                    pass_constants.object_scale_rotation[3] = draw_instance->rotation_y;
                    const auto& lights = snapshot.character_lights;
                    pass_constants.character_key_direction_intensity[0] = lights.key.direction.x;
                    pass_constants.character_key_direction_intensity[1] = lights.key.direction.y;
                    pass_constants.character_key_direction_intensity[2] = lights.key.direction.z;
                    pass_constants.character_key_direction_intensity[3] = lights.key.intensity;
                    pass_constants.character_key_color[0] = lights.key.color.r;
                    pass_constants.character_key_color[1] = lights.key.color.g;
                    pass_constants.character_key_color[2] = lights.key.color.b;
                    pass_constants.character_key_color[3] = lights.key.color.a;
                    pass_constants.character_fill_direction_intensity[0] = lights.fill.direction.x;
                    pass_constants.character_fill_direction_intensity[1] = lights.fill.direction.y;
                    pass_constants.character_fill_direction_intensity[2] = lights.fill.direction.z;
                    pass_constants.character_fill_direction_intensity[3] = lights.fill.intensity;
                    pass_constants.character_fill_color[0] = lights.fill.color.r;
                    pass_constants.character_fill_color[1] = lights.fill.color.g;
                    pass_constants.character_fill_color[2] = lights.fill.color.b;
                    pass_constants.character_fill_color[3] = lights.fill.color.a;
                    pass_constants.character_hemisphere_sky[0] = lights.hemisphere.sky.r;
                    pass_constants.character_hemisphere_sky[1] = lights.hemisphere.sky.g;
                    pass_constants.character_hemisphere_sky[2] = lights.hemisphere.sky.b;
                    pass_constants.character_hemisphere_sky[3] = lights.hemisphere.intensity;
                    pass_constants.character_hemisphere_ground[0] = lights.hemisphere.ground.r;
                    pass_constants.character_hemisphere_ground[1] = lights.hemisphere.ground.g;
                    pass_constants.character_hemisphere_ground[2] = lights.hemisphere.ground.b;
                    pass_constants.character_hemisphere_ground[3] = lights.hemisphere.intensity;
                    for (std::size_t index = 0U; index < 4U; ++index) {
                        pass_constants.morph_weights[index] = draw_palette->morph_weights[index];
                    }
                    for (std::size_t bone = 0U; bone < kInfantryBonePaletteSize; ++bone) {
                        std::memcpy(pass_constants.bone_palette[bone],
                                    draw_palette->matrices[bone].data(),
                                    sizeof(pass_constants.bone_palette[bone]));
                    }
                    impl_->context->UpdateBuffer(
                        impl_->skinned_pass_constants, 0, sizeof(pass_constants), &pass_constants,
                        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                    impl_->context->SetPipelineState(impl_->skinned_pipeline);
                    impl_->context->CommitShaderResources(
                        impl_->skinned_srb,
                        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                    Diligent::IBuffer* vertex_buffers[] = {prototype_gpu.vertex_buffer};
                    const Diligent::Uint64 vertex_offsets[] = {0};
                    impl_->context->SetVertexBuffers(
                        0, 1, vertex_buffers, vertex_offsets,
                        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                        Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
                    impl_->context->SetIndexBuffer(
                        prototype_gpu.index_buffer, 0,
                        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                    impl_->context->DrawIndexed(Diligent::DrawIndexedAttribs{
                        static_cast<Diligent::Uint32>(prototype_owner->indices.size()),
                        Diligent::VT_UINT32, Diligent::DRAW_FLAG_NONE});
                }
            }
        }


        // The preview and dynamic actor passes share one persistent table.
        // A small CPU-side batch list supplies compact slot indices for each
        // mesh/material pair. The GPU table remains stable, while each draw
        // receives only the instances that belong to its prototype.
        // A terrain-backed scene can still contain preview actors (UnitLab
        // is the important case).  Filtering on one mode based on terrain
        // presence used to drop the infantry preview before it reached the
        // GPU pass.  Keep both renderer-owned actor modes in this pass; the
        // shader still requires the active bit from GpuInstanceRecord.
        const std::uint32_t required_flags =
            RenderInstanceFlagPreview | RenderInstanceFlagDynamic;
        const auto has_gpu_skinned_prototype = [this, &snapshot](
            foundation::StableId mesh_id) {
            if (impl_->skinned_pipeline == nullptr) {
                return false;
            }
            return std::any_of(
                snapshot.skinned_prototypes.begin(), snapshot.skinned_prototypes.end(),
                [mesh_id](const std::shared_ptr<const SkinnedMeshPrototype>& prototype) {
                    return prototype && prototype->mesh_id == mesh_id &&
                           !prototype->vertices.empty() && !prototype->indices.empty();
                });
        };
        struct InstanceBatch final {
            foundation::StableId mesh_id{0};
            foundation::StableId material_id{0};
            std::vector<std::uint32_t> slots;
        };
        std::vector<InstanceBatch> batches;
        batches.reserve(snapshot.instances.size());
        for (const RenderInstance& instance : snapshot.instances) {
            if ((instance.flags & required_flags) == 0) {
                continue;
            }
            if (has_gpu_skinned_prototype(instance.mesh_id)) {
                continue;
            }
            const gpu_scene::GpuInstanceHandle handle =
                impl_->gpu_scene.find(instance.object_id);
            if (!handle.isValid()) {
                return RenderResult::failure(
                    {foundation::ErrorCode::InvalidState,
                     "GPU scene batch references a missing render instance"});
            }
            auto batch_iterator = std::find_if(
                batches.begin(), batches.end(), [&instance](const InstanceBatch& batch) {
                    return batch.mesh_id == instance.mesh_id &&
                           batch.material_id == instance.material_id;
                });
            if (batch_iterator == batches.end()) {
                batches.push_back({instance.mesh_id, instance.material_id, {}});
                batch_iterator = std::prev(batches.end());
            }
            batch_iterator->slots.push_back(handle.index);
        }
        if (impl_->swap_chain && impl_->preview_pipeline && impl_->preview_srb &&
            impl_->preview_pass_constants && impl_->terrain_camera_buffer &&
            impl_->gpu_instance_buffer && !batches.empty()) {
            if (!impl_->preview_prototype) {
                impl_->preview_prototype = make_unit_cube_mesh(
                    foundation::stable_id("mesh.preview.cube"));
            }
            foundation::Vec3 min_corner{std::numeric_limits<float>::max(),
                                        std::numeric_limits<float>::max(),
                                        std::numeric_limits<float>::max()};
            foundation::Vec3 max_corner{std::numeric_limits<float>::lowest(),
                                        std::numeric_limits<float>::lowest(),
                                        std::numeric_limits<float>::lowest()};
            for (const RenderInstance& instance : snapshot.instances) {
                if ((instance.flags & required_flags) == 0) {
                    continue;
                }
                if (has_gpu_skinned_prototype(instance.mesh_id)) {
                    continue;
                }
                const std::shared_ptr<const RenderMesh> prototype = resolve_instance_prototype(
                    snapshot, instance.mesh_id, impl_->preview_prototype);
                const foundation::Vec3 local_extent =
                    prototype ? mesh_half_extents(*prototype) : foundation::Vec3{1.0F, 1.0F, 1.0F};
                const float cosine = std::abs(std::cos(instance.rotation_y));
                const float sine = std::abs(std::sin(instance.rotation_y));
                const foundation::Vec3 scaled{local_extent.x * instance.scale.x,
                                              local_extent.y * instance.scale.y,
                                              local_extent.z * instance.scale.z};
                const foundation::Vec3 extent{cosine * scaled.x + sine * scaled.z,
                                              scaled.y,
                                              sine * scaled.x + cosine * scaled.z};
                min_corner.x = std::min(min_corner.x, instance.position.x - extent.x);
                min_corner.y = std::min(min_corner.y, instance.position.y - extent.y);
                min_corner.z = std::min(min_corner.z, instance.position.z - extent.z);
                max_corner.x = std::max(max_corner.x, instance.position.x + extent.x);
                max_corner.y = std::max(max_corner.y, instance.position.y + extent.y);
                max_corner.z = std::max(max_corner.z, instance.position.z + extent.z);
            }
            const foundation::Vec3 center{(min_corner.x + max_corner.x) * 0.5F,
                                          (min_corner.y + max_corner.y) * 0.5F,
                                          (min_corner.z + max_corner.z) * 0.5F};
            const float extent = std::max({max_corner.x - min_corner.x,
                                           max_corner.y - min_corner.y,
                                           max_corner.z - min_corner.z, 1.0F});
            const foundation::Vec3 auto_eye{center.x + extent * 2.0F,
                                            center.y + extent * 1.35F,
                                            center.z + extent * 2.0F};
            const bool use_scene_camera = snapshot.camera.enabled && snapshot.camera.valid();
            const foundation::Vec3 eye = use_scene_camera ? snapshot.camera.position : auto_eye;
            const foundation::Vec3 target = use_scene_camera ? snapshot.camera.target : center;
            const foundation::Vec3 up = use_scene_camera ? snapshot.camera.up
                                                          : foundation::Vec3{0.0F, 1.0F, 0.0F};
            const float vertical_fov = use_scene_camera ? snapshot.camera.vertical_fov : 0.9F;
            const float near_plane = use_scene_camera
                                         ? snapshot.camera.near_plane
                                         : std::max(0.05F, extent * 0.01F);
            const float far_plane = use_scene_camera ? snapshot.camera.far_plane : extent * 8.0F;
            const Diligent::SwapChainDesc swap_chain_desc = impl_->swap_chain->GetDesc();
            const float aspect = static_cast<float>(swap_chain_desc.Width) /
                                 static_cast<float>(std::max(1U, swap_chain_desc.Height));
            const Mat4 view_projection = multiply(
                perspective(vertical_fov, aspect, near_plane, far_plane),
                look_at(eye, target, up));
            CameraConstants camera{};
            std::memcpy(camera.view_projection, view_projection.values,
                        sizeof(camera.view_projection));
            impl_->context->UpdateBuffer(
                impl_->terrain_camera_buffer, 0, sizeof(camera), &camera,
                Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            const auto split_id = [](foundation::StableId id, std::uint32_t& low,
                                     std::uint32_t& high) noexcept {
                low = static_cast<std::uint32_t>(id & 0xFFFF'FFFFull);
                high = static_cast<std::uint32_t>(id >> 32U);
            };
            auto* instances_variable = impl_->preview_srb->GetVariableByName(
                Diligent::SHADER_TYPE_VERTEX, "Instances");
            auto* indices_variable = impl_->preview_srb->GetVariableByName(
                Diligent::SHADER_TYPE_VERTEX, "InstanceIndices");
            if (instances_variable == nullptr || indices_variable == nullptr) {
                return RenderResult::failure(
                    {foundation::ErrorCode::Internal,
                     "Diligent preview instance bindings disappeared"});
            }
            instances_variable->Set(
                impl_->gpu_instance_buffer->GetDefaultView(Diligent::BUFFER_VIEW_SHADER_RESOURCE),
                Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            impl_->context->SetPipelineState(impl_->preview_pipeline);

            for (const InstanceBatch& batch : batches) {
                if (batch.slots.empty() ||
                    batch.slots.size() > std::numeric_limits<std::uint32_t>::max()) {
                    return RenderResult::failure(
                        {foundation::ErrorCode::OutOfRange,
                         "instance batch exceeds GPU draw limits"});
                }
                const std::shared_ptr<const RenderMesh> prototype_owner =
                    resolve_instance_prototype(snapshot, batch.mesh_id, impl_->preview_prototype);
                if (!prototype_owner) {
                    return RenderResult::failure(
                        {foundation::ErrorCode::NotFound,
                         "instance batch has no geometry prototype"});
                }
                const RenderMesh& prototype = *prototype_owner;
                Impl::InstancePrototypeGpu& prototype_gpu =
                    impl_->instance_prototypes_gpu[prototype.mesh_id];
                if (prototype.vertices.size() > std::numeric_limits<std::uint32_t>::max() ||
                    prototype.indices.size() > std::numeric_limits<std::uint32_t>::max() ||
                    prototype.indices.empty() || prototype.indices.size() % 3 != 0) {
                    return RenderResult::failure(
                        {foundation::ErrorCode::InvalidArgument,
                         "instance prototype exceeds GPU draw limits"});
                }
                const std::size_t prototype_vertex_bytes =
                    prototype.vertices.size() * sizeof(RenderMeshVertex);
                const std::size_t prototype_index_bytes =
                    prototype.indices.size() * sizeof(std::uint32_t);
                const bool prototype_changed =
                    prototype_gpu.uploaded_revision != prototype.revision;
                if (!prototype_gpu.vertex_buffer || prototype_changed ||
                    prototype_vertex_bytes > prototype_gpu.vertex_capacity) {
                    const std::size_t capacity = prototype_vertex_bytes;
                    Diligent::BufferDesc desc{};
                    desc.Name = "Genomes instance prototype vertex buffer";
                    desc.Size = static_cast<Diligent::Uint64>(capacity);
                    desc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
                    desc.Usage = Diligent::USAGE_DEFAULT;
                    desc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;
                    const Diligent::BufferData data{prototype.vertices.data(),
                                                    static_cast<Diligent::Uint64>(prototype_vertex_bytes)};
                    Diligent::IBuffer* buffer = nullptr;
                    impl_->device->CreateBuffer(desc, &data, &buffer);
                    if (buffer == nullptr) {
                        return RenderResult::failure(
                            {foundation::ErrorCode::Internal,
                             "Diligent could not create the instance prototype vertex buffer"});
                    }
                    prototype_gpu.vertex_buffer.Attach(buffer);
                    prototype_gpu.vertex_capacity = capacity;
                }
                if (!prototype_gpu.index_buffer || prototype_changed ||
                    prototype_index_bytes > prototype_gpu.index_capacity) {
                    const std::size_t capacity = prototype_index_bytes;
                    Diligent::BufferDesc desc{};
                    desc.Name = "Genomes instance prototype index buffer";
                    desc.Size = static_cast<Diligent::Uint64>(capacity);
                    desc.BindFlags = Diligent::BIND_INDEX_BUFFER;
                    desc.Usage = Diligent::USAGE_DEFAULT;
                    desc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;
                    const Diligent::BufferData data{prototype.indices.data(),
                                                    static_cast<Diligent::Uint64>(prototype_index_bytes)};
                    Diligent::IBuffer* buffer = nullptr;
                    impl_->device->CreateBuffer(desc, &data, &buffer);
                    if (buffer == nullptr) {
                        return RenderResult::failure(
                            {foundation::ErrorCode::Internal,
                             "Diligent could not create the instance prototype index buffer"});
                    }
                    prototype_gpu.index_buffer.Attach(buffer);
                    prototype_gpu.index_capacity = capacity;
                }
                prototype_gpu.owner = prototype_owner;
                prototype_gpu.uploaded_revision = prototype.revision;

                const std::size_t remap_bytes = batch.slots.size() * sizeof(std::uint32_t);
                if (remap_bytes > impl_->instance_remap_capacity) {
                    const std::size_t capacity =
                        std::max<std::size_t>(remap_bytes, 256U * sizeof(std::uint32_t));
                    Diligent::BufferDesc desc{};
                    desc.Name = "Genomes instance slot remap";
                    desc.Size = static_cast<Diligent::Uint64>(capacity);
                    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
                    desc.Usage = Diligent::USAGE_DEFAULT;
                    desc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;
                    desc.Mode = Diligent::BUFFER_MODE_STRUCTURED;
                    desc.ElementByteStride = static_cast<Diligent::Uint32>(sizeof(std::uint32_t));
                    Diligent::IBuffer* buffer = nullptr;
                    impl_->device->CreateBuffer(desc, nullptr, &buffer);
                    if (buffer == nullptr) {
                        return RenderResult::failure(
                            {foundation::ErrorCode::Internal,
                             "Diligent could not create the instance slot remap buffer"});
                    }
                    impl_->instance_remap_buffer.Attach(buffer);
                    impl_->instance_remap_capacity = capacity;
                }
                impl_->context->UpdateBuffer(
                    impl_->instance_remap_buffer, 0, remap_bytes, batch.slots.data(),
                    Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

                InstancePassConstants pass_constants{};
                pass_constants.required_flags = required_flags;
                pass_constants.match_material = 1;
                split_id(batch.mesh_id, pass_constants.required_mesh_low,
                         pass_constants.required_mesh_high);
                split_id(batch.material_id, pass_constants.required_material_low,
                         pass_constants.required_material_high);
                impl_->context->UpdateBuffer(
                    impl_->preview_pass_constants, 0, sizeof(pass_constants), &pass_constants,
                    Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                indices_variable->Set(impl_->instance_remap_buffer->GetDefaultView(
                    Diligent::BUFFER_VIEW_SHADER_RESOURCE),
                    Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
                impl_->context->CommitShaderResources(
                    impl_->preview_srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                Diligent::IBuffer* prototype_vertex_buffers[] = {prototype_gpu.vertex_buffer};
                const Diligent::Uint64 prototype_offsets[] = {0};
                impl_->context->SetVertexBuffers(
                    0, 1, prototype_vertex_buffers, prototype_offsets,
                    Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                    Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
                impl_->context->SetIndexBuffer(
                    prototype_gpu.index_buffer, 0,
                    Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                impl_->context->DrawIndexed(Diligent::DrawIndexedAttribs{
                    static_cast<Diligent::Uint32>(prototype.indices.size()), Diligent::VT_UINT32,
                    Diligent::DRAW_FLAG_NONE,
                    static_cast<Diligent::Uint32>(batch.slots.size())});
            }
            return RenderResult::success();
        }
    } catch (const std::exception&) {
        return RenderResult::failure(
            {foundation::ErrorCode::Internal, "could not synchronize the GPU scene"});
    }

    if (impl_->swap_chain && impl_->debug_pipeline && impl_->debug_srb &&
        impl_->debug_vertex_buffer && !snapshot.debug_lines.empty() &&
        snapshot.debug_lines.size() <= kMaxDebugVertices / 2U) {
        const Diligent::SwapChainDesc swap_chain_desc = impl_->swap_chain->GetDesc();
        const float aspect = static_cast<float>(swap_chain_desc.Width) /
                             static_cast<float>(std::max(1U, swap_chain_desc.Height));
        const foundation::Vec3 eye = snapshot.camera.enabled
            ? snapshot.camera.position : foundation::Vec3{0.0F, 2.0F, 5.0F};
        const foundation::Vec3 target = snapshot.camera.enabled
            ? snapshot.camera.target : foundation::Vec3{0.0F, 1.0F, 0.0F};
        const foundation::Vec3 up = snapshot.camera.enabled
            ? snapshot.camera.up : foundation::Vec3{0.0F, 1.0F, 0.0F};
        const float vertical_fov = snapshot.camera.enabled ? snapshot.camera.vertical_fov : 0.9F;
        const float near_plane = snapshot.camera.enabled ? snapshot.camera.near_plane : 0.05F;
        const float far_plane = snapshot.camera.enabled ? snapshot.camera.far_plane : 100.0F;
        const Mat4 view_projection = multiply(
            perspective(vertical_fov, aspect, near_plane, far_plane),
            look_at(eye, target, up));
        CameraConstants camera{};
        std::memcpy(camera.view_projection, view_projection.values,
                    sizeof(camera.view_projection));
        impl_->context->UpdateBuffer(
            impl_->terrain_camera_buffer, 0, sizeof(camera), &camera,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        impl_->debug_vertices.clear();
        impl_->debug_vertices.reserve(snapshot.debug_lines.size() * 2U);
        for (const DebugLine& line : snapshot.debug_lines) {
            impl_->debug_vertices.push_back({
                {line.start.x, line.start.y, line.start.z},
                {line.color.r, line.color.g, line.color.b, line.color.a}});
            impl_->debug_vertices.push_back({
                {line.end.x, line.end.y, line.end.z},
                {line.color.r, line.color.g, line.color.b, line.color.a}});
        }
        impl_->context->UpdateBuffer(
            impl_->debug_vertex_buffer, 0,
            static_cast<Diligent::Uint64>(impl_->debug_vertices.size() * sizeof(DebugVertex)),
            impl_->debug_vertices.data(), Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        impl_->context->SetPipelineState(impl_->debug_pipeline);
        impl_->context->CommitShaderResources(
            impl_->debug_srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        Diligent::IBuffer* debug_vertex_buffers[] = {impl_->debug_vertex_buffer};
        const Diligent::Uint64 debug_offsets[] = {0};
        impl_->context->SetVertexBuffers(
            0, 1, debug_vertex_buffers, debug_offsets,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
        impl_->context->Draw(Diligent::DrawAttribs{
            static_cast<Diligent::Uint32>(impl_->debug_vertices.size()),
            Diligent::DRAW_FLAG_NONE});
    }

    if (!impl_->swap_chain || !impl_->ui_pipeline || !impl_->ui_vertex_buffer) {
        return RenderResult::success();
    }
    // Once the real 3D world is available, the old 2D world-plan pass would
    // occlude it. Keep that pass for menu/pre-generation previews only.
    try {
        impl_->ui_vertices.clear();
        const Diligent::SwapChainDesc swap_chain_desc = impl_->swap_chain->GetDesc();
        append_world_instances(impl_->ui_vertices, snapshot, swap_chain_desc.Width,
                               swap_chain_desc.Height);
    } catch (const std::exception&) {
        return RenderResult::failure(
            {foundation::ErrorCode::Internal, "could not build the world vertex stream"});
    }

    if (impl_->ui_vertices.empty()) {
        return RenderResult::success();
    }
    if (impl_->ui_vertices.size() > kMaxUiVertices) {
        return RenderResult::failure(
            {foundation::ErrorCode::InvalidArgument,
             "world vertex stream exceeds its frame budget"});
    }

    impl_->context->UpdateBuffer(
        impl_->ui_vertex_buffer, 0,
        static_cast<Diligent::Uint64>(impl_->ui_vertices.size() * sizeof(UiVertex)),
        impl_->ui_vertices.data(), Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    impl_->context->SetPipelineState(impl_->ui_pipeline);
    const auto fallback = impl_->ui_textures.find(0);
    if (fallback != impl_->ui_textures.end()) {
        impl_->context->CommitShaderResources(
            fallback->second.srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    const Diligent::SwapChainDesc swap_chain_desc = impl_->swap_chain->GetDesc();
    const Diligent::Rect full_scissor{0, 0, static_cast<Diligent::Int32>(swap_chain_desc.Width),
                                     static_cast<Diligent::Int32>(swap_chain_desc.Height)};
    impl_->context->SetScissorRects(1, &full_scissor,
                                    swap_chain_desc.Width, swap_chain_desc.Height);
    Diligent::IBuffer* vertex_buffers[] = {impl_->ui_vertex_buffer};
    const Diligent::Uint64 offsets[] = {0};
    impl_->context->SetVertexBuffers(
        0, 1, vertex_buffers, offsets, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
    impl_->context->Draw(Diligent::DrawAttribs{
        static_cast<Diligent::Uint32>(impl_->ui_vertices.size()), Diligent::DRAW_FLAG_NONE});
    return RenderResult::success();
}

RenderResult DiligentBackend::draw_ui(const ui::UiRenderFrame& document) noexcept {
    if (impl_ == nullptr || !impl_->capabilities.initialized) {
        return RenderResult::failure(
            {foundation::ErrorCode::InvalidState, "Diligent backend is shut down"});
    }
    if (!impl_->frame_open) {
        return RenderResult::failure(
            {foundation::ErrorCode::InvalidState, "Diligent frame is not open"});
    }
    if (!impl_->swap_chain || !impl_->ui_pipeline || !impl_->ui_vertex_buffer) {
        return RenderResult::success();
    }

    const auto ensure_ui_texture = [this](std::uint64_t id, std::uint32_t width,
                                          std::uint32_t height,
                                          const std::uint8_t* rgba) -> bool {
        if (impl_->ui_textures.contains(id)) return true;
        if (width == 0 || height == 0 || rgba == nullptr) return false;
        Diligent::TextureDesc desc{};
        desc.Name = id == 0 ? "Genomes UI white texture" : "Genomes RmlUi texture";
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = width;
        desc.Height = height;
        desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        desc.Usage = Diligent::USAGE_IMMUTABLE;
        Diligent::TextureSubResData subresource{rgba,
                                                static_cast<Diligent::Uint64>(width) * 4U};
        Diligent::TextureData data{&subresource, 1U};
        Diligent::ITexture* texture = nullptr;
        impl_->device->CreateTexture(desc, &data, &texture);
        if (texture == nullptr) return false;

        Impl::UiTextureGpu gpu{};
        gpu.texture.Attach(texture);
        Diligent::IShaderResourceBinding* srb = nullptr;
        impl_->ui_pipeline->CreateShaderResourceBinding(&srb, true);
        if (srb == nullptr) return false;
        gpu.srb.Attach(srb);
        auto* variable = gpu.srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_Texture");
        auto* view = gpu.texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
        if (variable == nullptr || view == nullptr) return false;
        variable->Set(view);
        impl_->ui_textures.emplace(id, std::move(gpu));
        return true;
    };

    static constexpr std::array<std::uint8_t, 4> white_pixel{255, 255, 255, 255};
    if (!ensure_ui_texture(0, 1, 1, white_pixel.data())) {
        return RenderResult::failure(
            {foundation::ErrorCode::Internal, "could not create the UI fallback texture"});
    }
    for (const auto& texture : document.textures) {
        const std::size_t expected = static_cast<std::size_t>(texture.width) *
                                     static_cast<std::size_t>(texture.height) * 4U;
        if (texture.rgba.size() < expected ||
            !ensure_ui_texture(texture.id, texture.width, texture.height,
                               texture.rgba.data())) {
            return RenderResult::failure(
                {foundation::ErrorCode::Internal, "could not upload an RmlUi texture"});
        }
    }

    // RmlUi commands carry texture and scissor state per draw. Keep their
    // order intact; flattening them into one stream turns glyph quads into
    // solid rectangles because the font atlas binding is lost.
    if (document.widgets.empty() && !document.commands.empty()) {
        const Diligent::SwapChainDesc swap_chain_desc = impl_->swap_chain->GetDesc();
        impl_->context->SetPipelineState(impl_->ui_pipeline);
        Diligent::IBuffer* vertex_buffers[] = {impl_->ui_vertex_buffer};
        const Diligent::Uint64 offsets[] = {0};
        impl_->context->SetVertexBuffers(
            0, 1, vertex_buffers, offsets, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
        for (const auto& command : document.commands) {
            impl_->ui_vertices.clear();
            append_rml_geometry(impl_->ui_vertices, command, swap_chain_desc.Width,
                                swap_chain_desc.Height);
            if (impl_->ui_vertices.empty()) continue;
            if (impl_->ui_vertices.size() > kMaxUiVertices) {
                return RenderResult::failure(
                    {foundation::ErrorCode::InvalidArgument,
                     "RmlUi draw command exceeds its vertex budget"});
            }
            impl_->context->UpdateBuffer(
                impl_->ui_vertex_buffer, 0,
                static_cast<Diligent::Uint64>(impl_->ui_vertices.size() * sizeof(UiVertex)),
                impl_->ui_vertices.data(), Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            const auto texture = impl_->ui_textures.find(command.texture);
            const auto fallback = impl_->ui_textures.find(0);
            const auto& binding = texture != impl_->ui_textures.end() ? texture->second
                                                                      : fallback->second;
            impl_->context->CommitShaderResources(
                binding.srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            Diligent::Rect scissor{0, 0, static_cast<Diligent::Int32>(swap_chain_desc.Width),
                                   static_cast<Diligent::Int32>(swap_chain_desc.Height)};
            if (command.scissor_enabled) {
                const float left = std::clamp(command.scissor.x, 0.0F,
                                              static_cast<float>(swap_chain_desc.Width));
                const float top = std::clamp(command.scissor.y, 0.0F,
                                             static_cast<float>(swap_chain_desc.Height));
                const float right = std::clamp(command.scissor.x + command.scissor.width, left,
                                               static_cast<float>(swap_chain_desc.Width));
                const float bottom = std::clamp(command.scissor.y + command.scissor.height, top,
                                                static_cast<float>(swap_chain_desc.Height));
                scissor = {static_cast<Diligent::Int32>(left),
                           static_cast<Diligent::Int32>(top),
                           static_cast<Diligent::Int32>(right),
                           static_cast<Diligent::Int32>(bottom)};
            }
            impl_->context->SetScissorRects(1, &scissor, swap_chain_desc.Width,
                                            swap_chain_desc.Height);
            impl_->context->Draw(Diligent::DrawAttribs{
                static_cast<Diligent::Uint32>(impl_->ui_vertices.size()),
                Diligent::DRAW_FLAG_NONE});
        }
        return RenderResult::success();
    }

    try {
        impl_->ui_vertices.clear();
        const Diligent::SwapChainDesc swap_chain_desc = impl_->swap_chain->GetDesc();
        append_ui_frame(impl_->ui_vertices, document, swap_chain_desc.Width,
                           swap_chain_desc.Height);
    } catch (const std::exception&) {
        return RenderResult::failure(
            {foundation::ErrorCode::Internal, "could not build the UI vertex stream"});
    }

    if (impl_->ui_vertices.empty()) {
        return RenderResult::success();
    }
    if (impl_->ui_vertices.size() > kMaxUiVertices) {
        return RenderResult::failure(
            {foundation::ErrorCode::InvalidArgument, "UI vertex stream exceeds its frame budget"});
    }

    impl_->context->UpdateBuffer(
        impl_->ui_vertex_buffer, 0,
        static_cast<Diligent::Uint64>(impl_->ui_vertices.size() * sizeof(UiVertex)),
        impl_->ui_vertices.data(), Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    impl_->context->SetPipelineState(impl_->ui_pipeline);
    const auto fallback = impl_->ui_textures.find(0);
    if (fallback != impl_->ui_textures.end()) {
        impl_->context->CommitShaderResources(
            fallback->second.srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    const Diligent::SwapChainDesc swap_chain_desc = impl_->swap_chain->GetDesc();
    const Diligent::Rect full_scissor{0, 0, static_cast<Diligent::Int32>(swap_chain_desc.Width),
                                     static_cast<Diligent::Int32>(swap_chain_desc.Height)};
    impl_->context->SetScissorRects(1, &full_scissor,
                                    swap_chain_desc.Width, swap_chain_desc.Height);
    Diligent::IBuffer* vertex_buffers[] = {impl_->ui_vertex_buffer};
    const Diligent::Uint64 offsets[] = {0};
    impl_->context->SetVertexBuffers(
        0, 1, vertex_buffers, offsets, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
    impl_->context->Draw(Diligent::DrawAttribs{
        static_cast<Diligent::Uint32>(impl_->ui_vertices.size()), Diligent::DRAW_FLAG_NONE});
    return RenderResult::success();
}

RenderResult DiligentBackend::resize(std::uint32_t width, std::uint32_t height) noexcept {
    if (impl_ == nullptr || !impl_->capabilities.initialized) {
        return RenderResult::failure(
            {foundation::ErrorCode::InvalidState, "Diligent backend is shut down"});
    }
    RenderConfig resized = impl_->config;
    resized.width = width;
    resized.height = height;
    if (!resized.valid()) {
        return RenderResult::failure(
            {foundation::ErrorCode::InvalidArgument, "invalid resize dimensions"});
    }
    if (!impl_->swap_chain) {
        impl_->config = resized;
        return RenderResult::success();
    }
    if (impl_->frame_open) {
        return RenderResult::failure(
            {foundation::ErrorCode::InvalidState, "cannot resize during an open frame"});
    }
    impl_->context->WaitForIdle();
    impl_->swap_chain->Resize(width, height);
    impl_->config = resized;
    return recreate_depth_buffer();
}

RenderResult DiligentBackend::end_frame() noexcept {
    if (impl_ == nullptr || !impl_->capabilities.initialized) {
        return RenderResult::failure(
            {foundation::ErrorCode::InvalidState, "Diligent backend is shut down"});
    }
    if (!impl_->frame_open) {
        return RenderResult::failure(
            {foundation::ErrorCode::InvalidState, "Diligent frame is not open"});
    }
    impl_->context->Flush();
    if (impl_->swap_chain) {
        impl_->swap_chain->Present(1);
    } else {
        // FinishFrame expects the immediate context to have no pending command
        // list.  This is a no-op for an empty frame but makes the lifecycle
        // valid for the first real submission as well.
        impl_->context->FinishFrame();
    }
    impl_->frame_open = false;
    return RenderResult::success();
}

RenderResult DiligentBackend::wait_idle() noexcept {
    if (impl_ == nullptr || !impl_->capabilities.initialized) {
        return RenderResult::failure(
            {foundation::ErrorCode::InvalidState, "Diligent backend is shut down"});
    }
    impl_->context->WaitForIdle();
    return RenderResult::success();
}

void DiligentBackend::shutdown() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    if (impl_->capabilities.initialized && impl_->context) {
        impl_->context->WaitForIdle();
    }
    impl_->frame_open = false;
    impl_->ui_vertex_buffer.Release();
    impl_->ui_pipeline.Release();
    impl_->ui_pixel_shader.Release();
    impl_->ui_vertex_shader.Release();
    impl_->debug_vertex_buffer.Release();
    impl_->debug_srb.Release();
    impl_->debug_pipeline.Release();
    impl_->debug_pixel_shader.Release();
    impl_->debug_vertex_shader.Release();
    impl_->debug_vertices.clear();
    impl_->terrain_index_buffer.Release();
    impl_->terrain_vertex_buffer.Release();
    impl_->terrain_camera_buffer.Release();
    impl_->terrain_pipeline.Release();
    impl_->terrain_pixel_shader.Release();
    impl_->terrain_vertex_shader.Release();
    impl_->preview_srb.Release();
    impl_->preview_pass_constants.Release();
    impl_->preview_pipeline.Release();
    impl_->preview_pixel_shader.Release();
    impl_->preview_vertex_shader.Release();
    impl_->depth_view.Release();
    impl_->depth_texture.Release();
    impl_->terrain_vertex_capacity = 0;
    impl_->terrain_index_capacity = 0;
    impl_->uploaded_terrain_owner.reset();
    impl_->uploaded_terrain_revision = 0;
    impl_->infantry_index_buffer.Release();
    impl_->infantry_vertex_buffer.Release();
    impl_->infantry_vertex_capacity = 0;
    impl_->infantry_index_capacity = 0;
    impl_->uploaded_infantry_owner.reset();
    impl_->uploaded_infantry_revision = 0;
    impl_->world_index_buffer.Release();
    impl_->world_vertex_buffer.Release();
    impl_->world_vertex_capacity = 0;
    impl_->world_index_capacity = 0;
    impl_->uploaded_world_owner.reset();
    impl_->uploaded_world_revision = 0;
    impl_->instance_remap_buffer.Release();
    impl_->instance_remap_capacity = 0;
    impl_->instance_prototypes_gpu.clear();
    impl_->preview_prototype.reset();
    impl_->gpu_instance_buffer.Release();
    impl_->gpu_instance_capacity = 0;
    impl_->gpu_scene.clear();
    impl_->ui_vertices.clear();
    impl_->swap_chain.Release();
    impl_->context.Release();
    impl_->device.Release();
    impl_->capabilities.initialized = false;
}

} // namespace genomes::render
