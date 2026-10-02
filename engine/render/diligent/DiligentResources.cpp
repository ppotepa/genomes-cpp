#include "DiligentBackendImpl.hpp"
#include <ShaderSources.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Sampler.h>
#include <limits>

namespace genomes::render {
using namespace diligent_detail;
using namespace diligent_contract;

DiligentBackend::Impl::Ptr<Diligent::IBuffer> DiligentBackend::Impl::buffer(
    const char* name,std::size_t size,Diligent::BIND_FLAGS bind,bool dynamic,
    const void* data,std::uint32_t structured_stride) {
    (void)dynamic;
    // Keep Genomes uploads in default D3D12 resources. Diligent's dynamic
    // buffer path suballocates an over-aligned context array; the pinned
    // release backend can issue an unaligned AVX store while constructing it.
    // UpdateBuffer below provides the same per-frame upload contract without
    // relying on that allocator path.
    Diligent::BufferDesc d{};d.Name=name;d.Size=static_cast<Diligent::Uint64>(size);d.BindFlags=bind;
    if (structured_stride != 0U) {
        d.Mode=Diligent::BUFFER_MODE_STRUCTURED;
        d.ElementByteStride=structured_stride;
    }
    d.Usage=Diligent::USAGE_DEFAULT;
    d.CPUAccessFlags=Diligent::CPU_ACCESS_NONE;
    Diligent::BufferData initial{data,static_cast<Diligent::Uint64>(size)};
    Ptr<Diligent::IBuffer> out;
    device->CreateBuffer(d,data?&initial:nullptr,&out);
    return out;
}
RenderResult DiligentBackend::Impl::mapCopy(Diligent::IBuffer* target,const void* source,std::size_t size) {
    if (!target || !source || !size || size>target->GetDesc().Size) return error("invalid GPU buffer write");
    if (target->GetDesc().Usage == Diligent::USAGE_DEFAULT) {
        context->UpdateBuffer(target,0,static_cast<Diligent::Uint64>(size),source,
                              Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        return RenderResult::success();
    }
    void* mapped=nullptr;context->MapBuffer(target,Diligent::MAP_WRITE,Diligent::MAP_FLAG_DISCARD,mapped);
    if (!mapped) return error("could not map transient GPU data",foundation::ErrorCode::Internal);
    std::memcpy(mapped,source,size);context->UnmapBuffer(target,Diligent::MAP_WRITE);return RenderResult::success();
}
RenderResult DiligentBackend::Impl::grow(Ptr<Diligent::IBuffer>& target,std::size_t& capacity,
                                        std::size_t size,Diligent::BIND_FLAGS bind,const char* name) {
    if (target && size<=capacity) return RenderResult::success();
    std::size_t next=std::max<std::size_t>(4096U,capacity);
    while (next<size) { if (next>std::numeric_limits<std::size_t>::max()/2U) return error("buffer size overflow"); next*=2U; }
    auto candidate=buffer(name,next,bind,true);
    if (!candidate) return error("could not allocate transient buffer",foundation::ErrorCode::Internal);
    target=std::move(candidate);capacity=next;return RenderResult::success();
}
bool DiligentBackend::Impl::compile(const char* source,const char* name,Diligent::SHADER_TYPE stage,Ptr<Diligent::IShader>& out) {
    Diligent::ShaderCreateInfo ci{};ci.Source=source;ci.EntryPoint="main";
    ci.SourceLanguage=Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    ci.Desc.Name=name;ci.Desc.ShaderType=stage;ci.Desc.UseCombinedTextureSamplers=true;
    device->CreateShader(ci,&out);return static_cast<bool>(out);
}
void DiligentBackend::Impl::bindConstants(Pipeline& p) {
    for (const auto stage:{Diligent::SHADER_TYPE_VERTEX,Diligent::SHADER_TYPE_PIXEL}) {
        if (auto* v=p.resources->GetVariableByName(stage,"SceneConstants")) v->Set(scene_buffer);
        if (auto* v=p.resources->GetVariableByName(stage,"SkinnedPassConstants")) v->Set(skin_buffer);
        if (auto* v=p.resources->GetVariableByName(stage,"MaterialConstants")) v->Set(material_buffer);
    }
    if (auto* v=p.resources->GetVariableByName(Diligent::SHADER_TYPE_PIXEL,"g_ShadowMap")) v->Set(shadow_view);
    bindSkinPaletteBuffer(p);
}
void DiligentBackend::Impl::bindSkinPaletteBuffer(Pipeline& p) {
    if (auto* v=p.resources->GetVariableByName(Diligent::SHADER_TYPE_VERTEX,
                                               "BonePaletteBuffer")) {
        v->Set(skin_palette_view);
    }
}
RenderResult DiligentBackend::Impl::createPipeline(bool skin,bool shadow_pass,bool two_sided,MaterialAlphaMode alpha,Pipeline*& output) {
    const unsigned key=(skin?1U:0U)|(shadow_pass?2U:0U)|(two_sided?4U:0U)|(static_cast<unsigned>(alpha)<<3U);
    if (auto it=pipelines.find(key);it!=pipelines.end()) { output=&it->second;return RenderResult::success(); }
    Ptr<Diligent::IShader> vs,ps;
    if (!compile(skin?diligent_shaders::skin_vs:diligent_shaders::regular_vs,"Genomes surface VS",Diligent::SHADER_TYPE_VERTEX,vs)||
        !compile(shadow_pass?diligent_shaders::shadow_ps:diligent_shaders::surface_ps,"Genomes surface PS",Diligent::SHADER_TYPE_PIXEL,ps))
        return error("could not compile surface shaders",foundation::ErrorCode::Internal);
    std::vector<Diligent::LayoutElement> layout;
    const auto f=Diligent::VT_FLOAT32;const auto no=Diligent::False;
    if (skin) {
        const auto stride=static_cast<Diligent::Uint32>(sizeof(SkinnedGpuVertex));
        layout={{0,0,3,f,no,0,stride},{1,0,3,f,no,12,stride},{2,0,2,f,no,24,stride},
                {3,0,4,f,no,32,stride},{4,0,4,f,no,48,stride},{5,0,4,f,no,64,stride}};
        for (std::uint32_t k=0;k<kMorphCount;++k) layout.push_back({6U+k,0,3,f,no,80U+12U*k,stride});
        for (std::uint32_t k=0;k<kMorphCount;++k) layout.push_back({10U+k,0,3,f,no,128U+12U*k,stride});
        layout.push_back({14,0,1,Diligent::VT_UINT32,no,176,stride});
        const auto skin_instance_stride=static_cast<Diligent::Uint32>(
            sizeof(SkinnedInstanceGpuVertex));
        layout.push_back({15,1,4,f,no,0,skin_instance_stride,
                          Diligent::INPUT_ELEMENT_FREQUENCY_PER_INSTANCE,1});
        layout.push_back({16,1,4,f,no,16,skin_instance_stride,
                          Diligent::INPUT_ELEMENT_FREQUENCY_PER_INSTANCE,1});
        layout.push_back({17,1,4,f,no,32,skin_instance_stride,
                          Diligent::INPUT_ELEMENT_FREQUENCY_PER_INSTANCE,1});
        layout.push_back({18,1,4,f,no,48,skin_instance_stride,
                          Diligent::INPUT_ELEMENT_FREQUENCY_PER_INSTANCE,1});
        layout.push_back({19,1,1,Diligent::VT_UINT32,no,64,skin_instance_stride,
                          Diligent::INPUT_ELEMENT_FREQUENCY_PER_INSTANCE,1});
        layout.push_back({20,1,1,Diligent::VT_UINT32,no,68,skin_instance_stride,
                          Diligent::INPUT_ELEMENT_FREQUENCY_PER_INSTANCE,1});
    } else {
        const auto stride=static_cast<Diligent::Uint32>(sizeof(RenderMeshVertex));
        layout={{0,0,3,f,no,static_cast<Diligent::Uint32>(offsetof(RenderMeshVertex,position)),stride},
                {1,0,3,f,no,static_cast<Diligent::Uint32>(offsetof(RenderMeshVertex,normal)),stride},
                {2,0,2,f,no,static_cast<Diligent::Uint32>(offsetof(RenderMeshVertex,uv)),stride},
                {3,0,4,f,no,static_cast<Diligent::Uint32>(offsetof(RenderMeshVertex,color)),stride}};
        for (std::uint32_t k=0;k<3U;++k) layout.push_back({4U+k,1,4,f,no,16U*k,sizeof(InstanceGpuVertex),Diligent::INPUT_ELEMENT_FREQUENCY_PER_INSTANCE,1});
    }
    Diligent::GraphicsPipelineStateCreateInfo ci{};ci.PSODesc.Name="Genomes material surface";
    ci.PSODesc.PipelineType=Diligent::PIPELINE_TYPE_GRAPHICS;ci.pVS=vs;ci.pPS=ps;
    auto& g=ci.GraphicsPipeline;
    g.InputLayout={layout.data(),static_cast<Diligent::Uint32>(layout.size())};
    g.PrimitiveTopology=Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    g.NumRenderTargets=shadow_pass?0U:1U;
    if (!shadow_pass) g.RTVFormats[0]=swap->GetDesc().ColorBufferFormat;
    g.DSVFormat=Diligent::TEX_FORMAT_D32_FLOAT;
    g.RasterizerDesc.CullMode=two_sided?Diligent::CULL_MODE_NONE:Diligent::CULL_MODE_BACK;
    g.RasterizerDesc.FrontCounterClockwise=Diligent::True;
    g.DepthStencilDesc.DepthEnable=Diligent::True;
    g.DepthStencilDesc.DepthWriteEnable=(shadow_pass||alpha!=MaterialAlphaMode::Blend)?Diligent::True:Diligent::False;
    if (!shadow_pass && alpha==MaterialAlphaMode::Blend) {
        auto& blend=g.BlendDesc.RenderTargets[0];blend.BlendEnable=Diligent::True;
        blend.SrcBlend=Diligent::BLEND_FACTOR_SRC_ALPHA;blend.DestBlend=Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
        blend.SrcBlendAlpha=Diligent::BLEND_FACTOR_ONE;blend.DestBlendAlpha=Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
    }
    ci.PSODesc.ResourceLayout.DefaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
    Diligent::SamplerDesc sampler{};
    sampler.MinFilter=sampler.MagFilter=sampler.MipFilter=Diligent::FILTER_TYPE_COMPARISON_LINEAR;
    sampler.ComparisonFunc=Diligent::COMPARISON_FUNC_LESS_EQUAL;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=Diligent::TEXTURE_ADDRESS_CLAMP;
    const Diligent::ImmutableSamplerDesc immutable{Diligent::SHADER_TYPE_PIXEL,"g_ShadowMap",sampler};
    if (!shadow_pass) { ci.PSODesc.ResourceLayout.ImmutableSamplers=&immutable;ci.PSODesc.ResourceLayout.NumImmutableSamplers=1U; }
    Pipeline candidate;device->CreateGraphicsPipelineState(ci,&candidate.state);
    if (!candidate.state) return error("could not create material pipeline",foundation::ErrorCode::Internal);
    candidate.state->CreateShaderResourceBinding(&candidate.resources,true);
    if (!candidate.resources) return error("could not create material bindings",foundation::ErrorCode::Internal);
    bindConstants(candidate);
    output=&pipelines.emplace(key,std::move(candidate)).first->second;
    return RenderResult::success();
}
RenderResult DiligentBackend::Impl::resizeDepth(std::uint32_t w,std::uint32_t h) {
    Diligent::TextureDesc d{};d.Name="Genomes depth";d.Type=Diligent::RESOURCE_DIM_TEX_2D;
    d.Width=w;d.Height=h;d.Format=Diligent::TEX_FORMAT_D32_FLOAT;d.BindFlags=Diligent::BIND_DEPTH_STENCIL;
    Ptr<Diligent::ITexture> candidate;device->CreateTexture(d,nullptr,&candidate);
    if (!candidate) return error("could not allocate depth buffer",foundation::ErrorCode::Internal);
    auto* view=candidate->GetDefaultView(Diligent::TEXTURE_VIEW_DEPTH_STENCIL);
    if (!view) return error("depth view is missing",foundation::ErrorCode::Internal);
    depth_view=view;depth=std::move(candidate);return RenderResult::success();
}
RenderResult DiligentBackend::Impl::initializeResources() {
    Diligent::FenceDesc fence_desc{};
    fence_desc.Name = "Genomes presentation fence";
    fence_desc.Type = Diligent::FENCE_TYPE_GENERAL;
    device->CreateFence(fence_desc, &frame_fence);
    if (!frame_fence) return error("could not allocate presentation fence",foundation::ErrorCode::Internal);
    scene_buffer=buffer("Genomes scene constants",sizeof(SceneConstants),Diligent::BIND_UNIFORM_BUFFER,true);
    skin_buffer=buffer("Genomes skin constants",(sizeof(SkinnedPassConstants)+255U)&~std::size_t{255U},Diligent::BIND_UNIFORM_BUFFER,true);
    skin_palette_buffer=buffer("Genomes skin palette",sizeof(float)*16U*69U,
                                Diligent::BIND_SHADER_RESOURCE,false,nullptr,
                                static_cast<std::uint32_t>(sizeof(float)*16U));
    if (skin_palette_buffer) {
        skin_palette_view=skin_palette_buffer->GetDefaultView(
            Diligent::BUFFER_VIEW_SHADER_RESOURCE);
    }
    material_buffer=buffer("Genomes material constants",256U,Diligent::BIND_UNIFORM_BUFFER,true);
    ui_parameters=buffer("Genomes UI parameters",256U,Diligent::BIND_UNIFORM_BUFFER,true);
    skin_palette_capacity=69U;
    if (!scene_buffer||!skin_buffer||!skin_palette_buffer||!skin_palette_view||
        !material_buffer||!ui_parameters) return error("could not allocate draw constants",foundation::ErrorCode::Internal);
    if (auto r=resizeDepth(config.width,config.height);!r) return r;
    Diligent::TextureDesc d{};d.Name="Genomes key shadow";d.Type=Diligent::RESOURCE_DIM_TEX_2D;
    d.Width=d.Height=shadow_size;d.Format=Diligent::TEX_FORMAT_D32_FLOAT;
    d.BindFlags=Diligent::BIND_DEPTH_STENCIL|Diligent::BIND_SHADER_RESOURCE;
    device->CreateTexture(d,nullptr,&shadow);
    if (!shadow) return error("could not allocate shadow map",foundation::ErrorCode::Internal);
    shadow_depth=shadow->GetDefaultView(Diligent::TEXTURE_VIEW_DEPTH_STENCIL);
    shadow_view=shadow->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    if (!shadow_depth||!shadow_view) return error("shadow views are missing",foundation::ErrorCode::Internal);
    // Compile required variants now, so startup reports missing shader support instead of failing mid-frame.
    for (bool skin:{false,true}) for (bool shadow_pass:{false,true}) {
        Pipeline* pipeline=nullptr;
        if (auto r=createPipeline(skin,shadow_pass,false,MaterialAlphaMode::Opaque,pipeline);!r) return r;
    }
    return RenderResult::success();
}
} // namespace genomes::render
