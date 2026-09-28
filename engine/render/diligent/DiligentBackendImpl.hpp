#pragma once

// Private implementation shared only by the backend translation unit.
#include "DiligentBackend.hpp"
#include "DiligentBuiltinShaders.hpp"
#include "DiligentUi.hpp"
#include <ShaderSources.hpp>
#include <genomes/render/ProceduralMeshes.hpp>
#include <genomes/render/gpu_scene/GpuScene.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace genomes::render::diligent_detail {
using foundation::Vec3;
template<class T> using Ptr = Diligent::RefCntAutoPtr<T>;
inline float dot(Vec3 a, Vec3 b) noexcept { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec3 sub(Vec3 a, Vec3 b) noexcept { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Vec3 cross(Vec3 a, Vec3 b) noexcept { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline bool finite(Vec3 a) noexcept { return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z); }
inline Vec3 normal(Vec3 a) noexcept {
    const float d=dot(a,a);
    if (!(d>1.0e-12F) || !std::isfinite(d)) return {0,1,0};
    const float r=1.0F/std::sqrt(d); return {a.x*r,a.y*r,a.z*r};
}
struct Mat4 { float v[16]{}; };
inline Mat4 multiply(const Mat4& a, const Mat4& b) noexcept {
    Mat4 c;
    for (std::size_t col=0;col<4;++col) for (std::size_t row=0;row<4;++row)
        for (std::size_t k=0;k<4;++k) c.v[col*4+row]+=a.v[k*4+row]*b.v[col*4+k];
    return c;
}
inline Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up) noexcept {
    const Vec3 f=normal(sub(target,eye)), r=normal(cross(f,up)), u=cross(r,f);
    Mat4 m;
    m.v[0]=r.x; m.v[1]=u.x; m.v[2]=-f.x;
    m.v[4]=r.y; m.v[5]=u.y; m.v[6]=-f.y;
    m.v[8]=r.z; m.v[9]=u.z; m.v[10]=-f.z;
    m.v[12]=-dot(r,eye); m.v[13]=-dot(u,eye); m.v[14]=dot(f,eye); m.v[15]=1;
    return m;
}
inline Mat4 perspective(float fov,float aspect,float near_z,float far_z) noexcept {
    Mat4 m; const float f=1.0F/std::tan(fov*0.5F);
    m.v[0]=f/aspect; m.v[5]=f; m.v[10]=far_z/(near_z-far_z);
    m.v[11]=-1; m.v[14]=far_z*near_z/(near_z-far_z); return m;
}
struct Bounds {
    Vec3 minimum{std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()};
    Vec3 maximum{std::numeric_limits<float>::lowest(),std::numeric_limits<float>::lowest(),std::numeric_limits<float>::lowest()};
    bool empty{true};
    void add(Vec3 p) noexcept {
        if (!finite(p)) return;
        empty=false;
        minimum={std::min(minimum.x,p.x),std::min(minimum.y,p.y),std::min(minimum.z,p.z)};
        maximum={std::max(maximum.x,p.x),std::max(maximum.y,p.y),std::max(maximum.z,p.z)};
    }
};
inline Vec3 instancePoint(Vec3 v,const RenderInstance& i) noexcept {
    const float x=v.x*i.scale.x, y=v.y*i.scale.y, z=v.z*i.scale.z;
    const float c=std::cos(i.rotation_y), s=std::sin(i.rotation_y);
    return {x*c-z*s+i.position.x,y+i.position.y,x*s+z*c+i.position.z};
}
inline bool validInstance(const RenderInstance& i) noexcept {
    return finite(i.position)&&finite(i.scale)&&std::isfinite(i.rotation_y)&&
        i.scale.x>1.0e-6F&&i.scale.y>1.0e-6F&&i.scale.z>1.0e-6F;
}
struct CameraConstants { float view_projection[16]{}; };
struct InstancePassConstants {
    std::uint32_t options[4]{};
    std::uint32_t mesh[4]{};
    std::uint32_t material[4]{};
};
inline constexpr std::size_t kBoneCount=69U;
struct SkinnedPassConstants {
    float view_projection[16]{};
    float object_position_scale[4]{};
    float object_scale_rotation[4]{};
    float camera_position[4]{};
    float key_direction_intensity[4]{};
    float key_color[4]{};
    float fill_direction_intensity[4]{};
    float fill_color[4]{};
    float hemisphere_sky[4]{};
    float hemisphere_ground[4]{};
    float morph_weights[4]{};
    float bone_palette[kBoneCount][16]{};
};
struct SkinnedGpuVertex {
    float position[3]{};
    float normal[3]{};
    float uv[2]{};
    float color[4]{};
    float bone_indices[4]{};
    float bone_weights[4]{};
    float morph_position[4][3]{};
    float morph_normal[4][3]{};
    std::uint32_t material_region{0};
};
struct DebugVertex { float position[3]{}; float color[4]{}; };
static_assert(std::is_standard_layout_v<RenderMeshVertex>);
static_assert(offsetof(RenderMeshVertex,position)==0U && offsetof(RenderMeshVertex,color)==32U);
static_assert(offsetof(RenderMeshVertex,material_region)==48U);
static_assert(sizeof(RenderMeshVertex)==52U);
static_assert(sizeof(SkinnedGpuVertex)==180U && offsetof(SkinnedGpuVertex,material_region)==176U);
static_assert(sizeof(InstancePassConstants)==48U);
static_assert(offsetof(SkinnedPassConstants,bone_palette)==224U);
static_assert(sizeof(SkinnedPassConstants)==4640U);
inline void vec(float* out,Vec3 v,float w) noexcept { out[0]=v.x;out[1]=v.y;out[2]=v.z;out[3]=w; }
inline void color(float* out,foundation::Color c,float w) noexcept { out[0]=c.r;out[1]=c.g;out[2]=c.b;out[3]=w; }
inline SkinnedGpuVertex gpuVertex(const SkinnedMeshPrototype& m,std::size_t index) {
    const auto& v=m.vertices[index]; SkinnedGpuVertex out;
    out.position[0]=v.position.x;out.position[1]=v.position.y;out.position[2]=v.position.z;
    out.normal[0]=v.normal.x;out.normal[1]=v.normal.y;out.normal[2]=v.normal.z;
    out.uv[0]=v.uv.x;out.uv[1]=v.uv.y;
    color(out.color,v.color,v.color.a);out.material_region=v.material_region;
    for (std::size_t k=0;k<4;++k) {
        out.bone_indices[k]=static_cast<float>(v.bone_indices[k]);out.bone_weights[k]=v.bone_weights[k];
        if (k>=m.morph_target_count) continue;
        if (index<m.morphs[k].position_deltas.size()) {
            const auto p=m.morphs[k].position_deltas[index];
            out.morph_position[k][0]=p.x;out.morph_position[k][1]=p.y;out.morph_position[k][2]=p.z;
        }
        if (index<m.morphs[k].normal_deltas.size()) {
            const auto n=m.morphs[k].normal_deltas[index];
            out.morph_normal[k][0]=n.x;out.morph_normal[k][1]=n.y;out.morph_normal[k][2]=n.z;
        }
    }
    return out;
}
template<class Mesh> inline bool validMesh(const Mesh& mesh) noexcept {
    if (mesh.vertices.empty()||mesh.indices.empty()||mesh.indices.size()%3U!=0||
        mesh.indices.size()>std::numeric_limits<std::uint32_t>::max()||
        mesh.vertices.size()>std::numeric_limits<std::uint32_t>::max()) return false;
    for (const auto& v:mesh.vertices) {
        if (!finite(v.position)||!finite(v.normal)||!std::isfinite(v.uv.x)||!std::isfinite(v.uv.y)||
            !std::isfinite(v.color.r)||!std::isfinite(v.color.g)||!std::isfinite(v.color.b)||
            !std::isfinite(v.color.a)) return false;
    }
    for (auto index:mesh.indices) if (index>=mesh.vertices.size()) return false;
    return true;
}
inline bool validSkin(const SkinnedMeshPrototype& mesh) noexcept {
    if (!validMesh(mesh)||mesh.morph_target_count>4U) return false;
    for (const auto& v:mesh.vertices) {
        float sum=0;
        for (std::size_t k=0;k<4;++k) {
            const float w=v.bone_weights[k];
            if (!std::isfinite(w)||w<0||(w>0&&v.bone_indices[k]>=kBoneCount)) return false;
            sum+=w;
        }
        if (std::abs(sum-1.0F)>1.0e-4F) return false;
    }
    for (std::size_t k=0;k<mesh.morph_target_count;++k) {
        const auto& m=mesh.morphs[k];
        if (m.position_deltas.size()!=mesh.vertices.size()||m.normal_deltas.size()!=mesh.vertices.size()) return false;
        for (std::size_t v=0;v<mesh.vertices.size();++v)
            if (!finite(m.position_deltas[v])||!finite(m.normal_deltas[v])) return false;
    }
    return true;
}
inline RenderResult error(const char* message,foundation::ErrorCode code=foundation::ErrorCode::InvalidState) {
    return RenderResult::failure({code,message});
}
} // namespace genomes::render::diligent_detail

namespace genomes::render {
struct DiligentBackend::Impl final {
    template<class T> using Ptr=diligent_detail::Ptr<T>;
    struct Pipeline {
        Ptr<Diligent::IShader> vs,ps;
        Ptr<Diligent::IPipelineState> state;
        Ptr<Diligent::IShaderResourceBinding> resources;
    };
    struct GpuMesh {
        Ptr<Diligent::IBuffer> vertices,indices;
        std::shared_ptr<const void> owner;
        std::uint64_t revision{0},last_seen{0};
        std::size_t vertex_bytes{0},index_bytes{0};
    };
    explicit Impl(RenderConfig c):config(c),capabilities{c.backend,false,c.headless,true,!c.headless} {
        capabilities.instanced_rendering=!c.headless;
        capabilities.gpu_skinning=!c.headless;
    }
    RenderConfig config;
    RenderCapabilities capabilities;
    // Declared first: native device/context outlive all resource owners.
    Ptr<Diligent::IRenderDevice> device;
    Ptr<Diligent::IDeviceContext> context;
    Ptr<Diligent::ISwapChain> swap_chain;
    Pipeline ui,terrain,debug,instances,skinned;
    Ptr<Diligent::IBuffer> camera_buffer,instance_constants,skinned_constants;
    Ptr<Diligent::IBuffer> instance_buffer,remap_buffer,ui_buffer,debug_buffer;
    std::size_t instance_capacity{0},remap_capacity{0},ui_capacity{0},debug_capacity{0};
    Ptr<Diligent::ITexture> depth;
    Ptr<Diligent::ITextureView> depth_view;
    std::unordered_map<foundation::StableId,GpuMesh> mesh_cache,skin_cache;
    std::shared_ptr<const RenderMesh> fallback;
    gpu_scene::GpuScene gpu_scene;
    RenderExtractor extractor;
    RenderUploadTelemetry telemetry{};
    diligent_detail::Mat4 view_projection{};
    RenderCamera camera{};
    bool frame_open{false};
    std::vector<diligent_ui::Vertex> ui_vertices;
    std::vector<diligent_detail::DebugVertex> debug_vertices;

    bool shader(const char* source,const char* name,Diligent::SHADER_TYPE type,Ptr<Diligent::IShader>& out) {
        Diligent::ShaderCreateInfo info{};
        info.Source=source;info.SourceLength=std::strlen(source);info.EntryPoint="main";
        info.SourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
        info.Desc=Diligent::ShaderDesc{name,type};
        Diligent::IShader* result=nullptr;
        device->CreateShader(info,&result);
        out.Attach(result);return result!=nullptr;
    }
    bool pipeline(Pipeline& p,const char* name,const char* vs,const char* ps,
                  std::span<const Diligent::LayoutElement> layout,bool depth_enabled,
                  bool lines=false,bool cull=false) {
        if (!shader(vs,name,Diligent::SHADER_TYPE_VERTEX,p.vs)||
            !shader(ps,name,Diligent::SHADER_TYPE_PIXEL,p.ps)) return false;
        Diligent::GraphicsPipelineStateCreateInfo info{name};
        info.pVS=p.vs;info.pPS=p.ps;
        info.GraphicsPipeline.InputLayout={layout.data(),static_cast<Diligent::Uint32>(layout.size())};
        info.GraphicsPipeline.PrimitiveTopology=lines?Diligent::PRIMITIVE_TOPOLOGY_LINE_LIST:Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        info.GraphicsPipeline.NumRenderTargets=1;
        info.GraphicsPipeline.RTVFormats[0]=swap_chain->GetDesc().ColorBufferFormat;
        info.GraphicsPipeline.DSVFormat=Diligent::TEX_FORMAT_D32_FLOAT;
        info.GraphicsPipeline.DepthStencilDesc.DepthEnable=depth_enabled?Diligent::True:Diligent::False;
        info.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable=depth_enabled&&!lines?Diligent::True:Diligent::False;
        info.GraphicsPipeline.RasterizerDesc.CullMode=cull?Diligent::CULL_MODE_BACK:Diligent::CULL_MODE_NONE;
        info.PSODesc.ResourceLayout.DefaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
        Diligent::IPipelineState* result=nullptr;
        device->CreateGraphicsPipelineState(info,&result);
        if (!result) return false;
        p.state.Attach(result);
        Diligent::IShaderResourceBinding* binding=nullptr;
        p.state->CreateShaderResourceBinding(&binding,true);
        p.resources.Attach(binding);
        return true;
    }
    Ptr<Diligent::IBuffer> buffer(const char* name,std::size_t bytes,Diligent::BIND_FLAGS bind,
                                  const void* data=nullptr,std::uint32_t element_stride=0) {
        Diligent::BufferDesc desc{};
        desc.Name=name;desc.Size=static_cast<Diligent::Uint64>(bytes);desc.BindFlags=bind;
        desc.Usage=Diligent::USAGE_DEFAULT;desc.CPUAccessFlags=Diligent::CPU_ACCESS_NONE;
        if (element_stride!=0) { desc.Mode=Diligent::BUFFER_MODE_STRUCTURED;desc.ElementByteStride=element_stride; }
        const Diligent::BufferData initial{data,static_cast<Diligent::Uint64>(bytes)};
        Diligent::IBuffer* raw=nullptr;
        device->CreateBuffer(desc,data?&initial:nullptr,&raw);
        Ptr<Diligent::IBuffer> out;out.Attach(raw);return out;
    }
    Ptr<Diligent::IBuffer> constants(const char* name,std::size_t bytes) {
        return buffer(name,(bytes+255U)&~std::size_t{255U},Diligent::BIND_UNIFORM_BUFFER);
    }
    bool bindConstant(Pipeline& p,const char* name,Diligent::IBuffer* resource) {
        if (!p.resources) return false;
        bool found=false;
        for (auto stage:{Diligent::SHADER_TYPE_VERTEX,Diligent::SHADER_TYPE_PIXEL}) {
            if (auto* variable=p.resources->GetVariableByName(stage,name)) {
                variable->Set(resource);found=true;
            }
        }
        return found;
    }
    bool initializePipelines() {
        using Diligent::LayoutElement;
        constexpr auto f=Diligent::VT_FLOAT32;
        constexpr auto no=Diligent::False;
        const auto stride=static_cast<Diligent::Uint32>(sizeof(RenderMeshVertex));
        const LayoutElement regular[]{
            {0,0,3,f,no,static_cast<Diligent::Uint32>(offsetof(RenderMeshVertex,position)),stride},
            {1,0,3,f,no,static_cast<Diligent::Uint32>(offsetof(RenderMeshVertex,normal)),stride},
            {2,0,2,f,no,static_cast<Diligent::Uint32>(offsetof(RenderMeshVertex,uv)),stride},
            {3,0,4,f,no,static_cast<Diligent::Uint32>(offsetof(RenderMeshVertex,color)),stride},
            {4,0,1,Diligent::VT_UINT16,no,static_cast<Diligent::Uint32>(offsetof(RenderMeshVertex,material_region)),stride}};
        const LayoutElement ui_layout[]{{0,0,2,f,no,0U,sizeof(diligent_ui::Vertex)},
                                       {1,0,4,f,no,8U,sizeof(diligent_ui::Vertex)}};
        const LayoutElement debug_layout[]{{0,0,3,f,no,0U,sizeof(diligent_detail::DebugVertex)},
                                          {1,0,4,f,no,12U,sizeof(diligent_detail::DebugVertex)}};
        using V=diligent_detail::SkinnedGpuVertex;
        const auto ss=static_cast<Diligent::Uint32>(sizeof(V));
        std::array<LayoutElement,15U> skin_layout{{
            {0,0,3,f,no,offsetof(V,position),ss}, {1,0,3,f,no,offsetof(V,normal),ss},
            {2,0,2,f,no,offsetof(V,uv),ss}, {3,0,4,f,no,offsetof(V,color),ss},
            {4,0,4,f,no,offsetof(V,bone_indices),ss}, {5,0,4,f,no,offsetof(V,bone_weights),ss}}};
        for (std::uint32_t k=0;k<4U;++k) {
            skin_layout[6U+k]={6U+k,0,3,f,no,static_cast<Diligent::Uint32>(offsetof(V,morph_position))+12U*k,ss};
            skin_layout[10U+k]={10U+k,0,3,f,no,static_cast<Diligent::Uint32>(offsetof(V,morph_normal))+12U*k,ss};
        }
        skin_layout[14]={14,0,1,Diligent::VT_UINT32,no,offsetof(V,material_region),ss};
        if (!pipeline(ui,"Genomes UI",diligent_builtin::ui_vs,diligent_builtin::color_ps,ui_layout,false)||
            !pipeline(terrain,"Genomes terrain",diligent_builtin::terrain_vs,diligent_builtin::terrain_ps,{regular,4U},true)||
            !pipeline(debug,"Genomes debug",diligent_builtin::debug_vs,diligent_builtin::color_ps,debug_layout,true,true)||
            !pipeline(instances,"Genomes instances",diligent_builtin::instance_vs,diligent_builtin::instance_ps,regular,true,false,true)||
            !pipeline(skinned,"Genomes character",diligent_shaders::kSkinnedVertexShader,diligent_shaders::kSkinnedPixelShader,skin_layout,true,false,true)) return false;
        camera_buffer=constants("Genomes camera",sizeof(diligent_detail::CameraConstants));
        instance_constants=constants("Genomes instance batch",sizeof(diligent_detail::InstancePassConstants));
        skinned_constants=constants("Genomes character pass",sizeof(diligent_detail::SkinnedPassConstants));
        if (!camera_buffer||!instance_constants||!skinned_constants) return false;
        return bindConstant(terrain,"CameraConstants",camera_buffer)&&
            bindConstant(debug,"CameraConstants",camera_buffer)&&
            bindConstant(instances,"CameraConstants",camera_buffer)&&
            bindConstant(instances,"InstancePassConstants",instance_constants)&&
            bindConstant(skinned,"SkinnedPassConstants",skinned_constants);
    }
    bool changed(const GpuMesh& m,std::uint64_t revision,std::size_t vb,std::size_t ib,const void* owner) const noexcept {
        return !m.vertices||!m.indices||m.revision!=revision||m.vertex_bytes!=vb||m.index_bytes!=ib||
            (revision==0&&m.owner.get()!=owner);
    }
    bool upload(GpuMesh& m,const void* vertices,std::size_t vb,const std::uint32_t* indices,
                std::size_t ib,std::uint64_t revision,std::shared_ptr<const void> owner) {
        // Both resources own persistent storage. USAGE_DYNAMIC allocations are
        // frame-local on D3D12/Vulkan and cannot implement a revision cache.
        auto new_vb=buffer("Genomes persistent mesh vertices",vb,Diligent::BIND_VERTEX_BUFFER,vertices);
        if (!new_vb) return false;
        auto new_ib=buffer("Genomes persistent mesh indices",ib,Diligent::BIND_INDEX_BUFFER,indices);
        if (!new_ib) return false;
        // Publish the pair atomically; failure above leaves the old revision intact.
        m.vertices=std::move(new_vb);m.indices=std::move(new_ib);m.owner=std::move(owner);
        m.revision=revision;m.vertex_bytes=vb;m.index_bytes=ib;m.last_seen=telemetry.frame;
        ++telemetry.mesh_uploads;++telemetry.total_mesh_uploads;
        telemetry.mesh_upload_bytes+=vb+ib;telemetry.total_mesh_upload_bytes+=vb+ib;
        return true;
    }
    RenderResult ensureMesh(const std::shared_ptr<const RenderMesh>& mesh,GpuMesh*& output) {
        if (!mesh) return diligent_detail::error("missing mesh");
        auto& gpu=mesh_cache[mesh->mesh_id];
        const auto vb=mesh->vertices.size()*sizeof(RenderMeshVertex), ib=mesh->indices.size()*sizeof(std::uint32_t);
        if (changed(gpu,mesh->revision,vb,ib,mesh.get())) {
            if (!diligent_detail::validMesh(*mesh)) return diligent_detail::error("invalid mesh vertex/index data");
            if (!upload(gpu,mesh->vertices.data(),vb,mesh->indices.data(),ib,mesh->revision,mesh))
                return diligent_detail::error("could not allocate persistent mesh buffers",foundation::ErrorCode::Internal);
        }
        gpu.last_seen=telemetry.frame;output=&gpu;return RenderResult::success();
    }
    RenderResult ensureSkin(const std::shared_ptr<const SkinnedMeshPrototype>& mesh,GpuMesh*& output) {
        if (!mesh) return diligent_detail::error("missing skinned mesh");
        auto& gpu=skin_cache[mesh->mesh_id];
        const auto vb=mesh->vertices.size()*sizeof(diligent_detail::SkinnedGpuVertex), ib=mesh->indices.size()*sizeof(std::uint32_t);
        if (changed(gpu,mesh->revision,vb,ib,mesh.get())) {
            if (!diligent_detail::validSkin(*mesh)) return diligent_detail::error("invalid skin, morph or index data");
            std::vector<diligent_detail::SkinnedGpuVertex> packed;packed.reserve(mesh->vertices.size());
            for (std::size_t k=0;k<mesh->vertices.size();++k) packed.push_back(diligent_detail::gpuVertex(*mesh,k));
            if (!upload(gpu,packed.data(),vb,mesh->indices.data(),ib,mesh->revision,mesh))
                return diligent_detail::error("could not allocate persistent skinned buffers",foundation::ErrorCode::Internal);
        }
        gpu.last_seen=telemetry.frame;output=&gpu;return RenderResult::success();
    }
    void bindDraw(Pipeline& pipeline,GpuMesh& mesh,std::size_t index_count,std::uint32_t instance_count=1U) {
        context->SetPipelineState(pipeline.state);
        if (pipeline.resources) context->CommitShaderResources(pipeline.resources,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        Diligent::IBuffer* buffers[]{mesh.vertices};const Diligent::Uint64 offsets[]{0};
        context->SetVertexBuffers(0,1,buffers,offsets,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
        context->SetIndexBuffer(mesh.indices,0,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context->DrawIndexed(Diligent::DrawIndexedAttribs{static_cast<Diligent::Uint32>(index_count),Diligent::VT_UINT32,Diligent::DRAW_FLAG_NONE,instance_count});
        ++telemetry.draw_calls;
    }
    void viewport(const RenderCamera& c) {
        const auto desc=swap_chain->GetDesc();Diligent::Viewport vp{};
        vp.TopLeftX=c.viewport_left*static_cast<float>(desc.Width);
        vp.TopLeftY=c.viewport_top*static_cast<float>(desc.Height);
        vp.Width=c.viewport_width*static_cast<float>(desc.Width);
        vp.Height=c.viewport_height*static_cast<float>(desc.Height);vp.MinDepth=0;vp.MaxDepth=1;
        context->SetViewports(1,&vp,desc.Width,desc.Height);
    }
    RenderResult prepareCamera(const PresentationSnapshot& snapshot);
    RenderResult drawMeshes(const PresentationSnapshot& snapshot);
    RenderResult drawInstances(const PresentationSnapshot& snapshot);
    RenderResult drawDebug(const PresentationSnapshot& snapshot);
    RenderResult drawUi(const ui::UiDocument& document);
    RenderResult uploadUi();
};
} // namespace genomes::render
