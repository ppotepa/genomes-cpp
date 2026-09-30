#pragma once
#include "DiligentBackend.hpp"
#include "DiligentGpuContracts.hpp"
#include <genomes/render/DrawMaterialPlan.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/TextureView.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace genomes::render::diligent_detail {
using V=foundation::Vec3;
template<class T> using Ptr=Diligent::RefCntAutoPtr<T>;
struct Mat4 { std::array<float,16U> v{}; };
inline Mat4 identity() { Mat4 m; m.v[0]=m.v[5]=m.v[10]=m.v[15]=1; return m; }
inline Mat4 multiply(const Mat4& a,const Mat4& b) {
    Mat4 r;
    for (std::size_t c=0;c<4;++c) for (std::size_t row=0;row<4;++row)
        for (std::size_t k=0;k<4;++k) r.v[c*4+row]+=a.v[k*4+row]*b.v[c*4+k];
    return r;
}
inline Mat4 fromMath(const math::Mat4& source) {
    Mat4 result{};
    result.v=source.m;
    return result;
}
inline Mat4 ortho(float radius,float near_z,float far_z) {
    Mat4 m=identity();m.v[0]=m.v[5]=1/radius;m.v[10]=1/(near_z-far_z);m.v[14]=near_z/(near_z-far_z);return m;
}
inline V transformPoint(V p,const RenderInstance& i) {
    const float c=std::cos(i.rotation_y),s=std::sin(i.rotation_y);
    p={p.x*i.scale.x,p.y*i.scale.y,p.z*i.scale.z};
    return {p.x*c-p.z*s+i.position.x,p.y+i.position.y,p.x*s+p.z*c+i.position.z};
}
inline void vec(float* d,V v,float w) { d[0]=v.x;d[1]=v.y;d[2]=v.z;d[3]=w; }
inline void color(float* d,foundation::Color c,float a) { d[0]=c.r;d[1]=c.g;d[2]=c.b;d[3]=a; }
inline RenderResult error(const char* message,foundation::ErrorCode code=foundation::ErrorCode::InvalidState) {
    return RenderResult::failure({code,message});
}
}

namespace genomes::render {
struct DiligentBackend::Impl final {
    template<class T> using Ptr=diligent_detail::Ptr<T>;
    struct Pipeline { Ptr<Diligent::IPipelineState> state; Ptr<Diligent::IShaderResourceBinding> resources; };
    struct Mesh {
        Ptr<Diligent::IBuffer> vertices,indices;
        std::shared_ptr<const void> owner;
        std::uint64_t revision{0},last_seen{0};
        std::size_t vertex_bytes{0},index_bytes{0};
        std::vector<MaterialDrawRange> ranges;
        foundation::Vec3 center{},half_extent{};
    };
    struct Item {
        Mesh* gpu{nullptr};
        RenderInstance instance{};
        const SkinnedMeshPrototype* skin{nullptr};
        const SkinnedBonePalette* pose{nullptr};
        bool direct{false};
    };
    struct UiTextureGpu {
        Ptr<Diligent::ITexture> texture;
        Ptr<Diligent::IShaderResourceBinding> resources;
        std::uint64_t revision{0};
        std::uint32_t width{0},height{0};
    };
    explicit Impl(RenderConfig c):config(c) {
        caps.backend=c.backend;caps.headless=c.headless;
        caps.presentation=!c.headless;caps.instanced_rendering=!c.headless;caps.gpu_skinning=!c.headless;
    }
    RenderConfig config;
    RenderCapabilities caps{};
    // Destruction order: caches and pipelines first, device/context last.
    Ptr<Diligent::IRenderDevice> device;
    Ptr<Diligent::IDeviceContext> context;
    Ptr<Diligent::ISwapChain> swap;
    Ptr<Diligent::ITexture> depth,shadow;
    Ptr<Diligent::ITextureView> depth_view,shadow_depth,shadow_view;
    Ptr<Diligent::IBuffer> scene_buffer,skin_buffer,material_buffer,ui_parameters;
    Ptr<Diligent::IBuffer> instance_buffer,debug_buffer,ui_vertices_buffer,ui_indices_buffer;
    std::size_t instance_capacity{0},debug_capacity{0},ui_vertex_capacity{0},ui_index_capacity{0};
    std::map<unsigned,Pipeline> pipelines;
    Pipeline debug_pipeline,ui_pipeline;
    std::unordered_map<foundation::StableId,Mesh> regular_cache,skin_cache;
    std::unordered_map<std::uint64_t,UiTextureGpu> ui_textures;
    std::vector<Item> items;
    std::vector<diligent_contract::InstanceGpuVertex> instance_scratch;
    std::shared_ptr<const RenderMesh> preview_fallback;
    RenderCamera camera{};
    camera::ResolvedCamera resolved_camera{};
    bool have_resolved_camera{false},open{false},prepared{false},drawn_meshes{false},drawn_instances{false};
    diligent_detail::Mat4 camera_matrix{},shadow_matrix{},shadow_uv_matrix{};
    CharacterLightRig lights{};
    RenderUploadTelemetry telemetry{};
    std::optional<std::filesystem::path> pending_capture;
    std::string capture_metadata;
    // Valid only within one pass. Never reuse transient allocations across frames.
    foundation::StableId last_skin_instance{0};
    static constexpr std::uint32_t shadow_size=2048U;

    RenderResult initializeResources();
    RenderResult resizeDepth(std::uint32_t,std::uint32_t);
    RenderResult ensureRegular(const std::shared_ptr<const RenderMesh>&,Mesh*&);
    RenderResult ensureSkin(const std::shared_ptr<const SkinnedMeshPrototype>&,Mesh*&);
    RenderResult prepare(const PresentationSnapshot&);
    RenderResult renderItems(bool direct,bool shadow_pass);
    RenderResult renderDebug(const PresentationSnapshot&);
    RenderResult renderUi(const ui::UiRenderFrame&);
    RenderResult writeCapture();
    RenderResult createPipeline(bool skinned,bool shadow_pass,bool double_sided,MaterialAlphaMode,Pipeline*&);
    RenderResult drawRange(std::span<const Item* const>,std::size_t range_index,bool shadow_pass);
    RenderResult setSceneConstants(bool shadow_pass);
    void viewport(const RenderCamera&);
    void restoreTargets();
    void prune();
    Ptr<Diligent::IBuffer> buffer(const char*,std::size_t,Diligent::BIND_FLAGS,bool,const void* data=nullptr);
    RenderResult mapCopy(Diligent::IBuffer*,const void*,std::size_t);
    RenderResult grow(Ptr<Diligent::IBuffer>&,std::size_t&,std::size_t,Diligent::BIND_FLAGS,const char*);
    bool compile(const char*,const char*,Diligent::SHADER_TYPE,Ptr<Diligent::IShader>&);
    void bindConstants(Pipeline&);
};
} // namespace genomes::render
