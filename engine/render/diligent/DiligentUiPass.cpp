#include "DiligentBackendImpl.hpp"
#include "DiligentUi.hpp"
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Sampler.h>
#include <limits>
#include <set>

namespace genomes::render {
using namespace diligent_detail;
using namespace diligent_contract;
namespace {
constexpr char ui_vs[]=R"HLSL(
struct In {float2 P:ATTRIB0;float2 UV:ATTRIB1;float4 Color:ATTRIB2;};
struct Out {float4 P:SV_POSITION;float2 UV:TEXCOORD0;float4 Color:COLOR0;};
Out main(In i){Out o;o.P=float4(i.P,0,1);o.UV=i.UV;o.Color=i.Color;return o;}
)HLSL";
constexpr char ui_ps[]=R"HLSL(
Texture2D g_UiTexture;SamplerState g_UiTexture_sampler;
cbuffer UiParameters {float4 Mode;};
struct In {float4 P:SV_POSITION;float2 UV:TEXCOORD0;float4 Color:COLOR0;};
float3 linearColor(float3 c){return float3(c.r<=.04045?c.r/12.92:pow((c.r+.055)/1.055,2.4),c.g<=.04045?c.g/12.92:pow((c.g+.055)/1.055,2.4),c.b<=.04045?c.b/12.92:pow((c.b+.055)/1.055,2.4));}
float4 main(In i):SV_TARGET {
    float4 v=i.Color;if(Mode.x<.5)v.rgb*=v.a;
    float4 c=v*g_UiTexture.Sample(g_UiTexture_sampler,i.UV);
    if(c.a<=1e-8)return 0;
    return float4(linearColor(saturate(c.rgb/c.a))*c.a,c.a);
}
)HLSL";
constexpr char debug_vs[]=R"HLSL(
cbuffer SceneConstants {column_major float4x4 SceneViewProjection;};
struct In {float3 P:ATTRIB0;float4 Color:ATTRIB1;};
struct Out {float4 P:SV_POSITION;float4 Color:COLOR0;};
Out main(In i){Out o;o.P=mul(SceneViewProjection,float4(i.P,1));o.Color=i.Color;return o;}
)HLSL";
constexpr char debug_ps[]=R"HLSL(
struct In {float4 P:SV_POSITION;float4 Color:COLOR0;};
float4 main(In i):SV_TARGET{return i.Color;}
)HLSL";
}
RenderResult DiligentBackend::Impl::renderDebug(const PresentationSnapshot& snapshot) {
    if (snapshot.debug_lines.empty()) return RenderResult::success();
    if (snapshot.debug_lines.size()>1'000'000U) return error("debug line budget exceeded");
    if (!debug_pipeline.state) {
        Ptr<Diligent::IShader> vs,ps;
        if (!compile(debug_vs,"Genomes debug VS",Diligent::SHADER_TYPE_VERTEX,vs)||!compile(debug_ps,"Genomes debug PS",Diligent::SHADER_TYPE_PIXEL,ps)) return error("debug shader creation failed");
        Diligent::GraphicsPipelineStateCreateInfo ci{};ci.PSODesc.Name="Genomes debug lines";ci.pVS=vs;ci.pPS=ps;
        ci.GraphicsPipeline.PrimitiveTopology=Diligent::PRIMITIVE_TOPOLOGY_LINE_LIST;
        ci.GraphicsPipeline.NumRenderTargets=1;ci.GraphicsPipeline.RTVFormats[0]=swap->GetDesc().ColorBufferFormat;ci.GraphicsPipeline.DSVFormat=Diligent::TEX_FORMAT_D32_FLOAT;
        ci.GraphicsPipeline.DepthStencilDesc.DepthEnable=Diligent::True;ci.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable=Diligent::False;
        ci.GraphicsPipeline.DepthStencilDesc.DepthFunc=Diligent::COMPARISON_FUNC_LESS_EQUAL;
        ci.GraphicsPipeline.RasterizerDesc.CullMode=Diligent::CULL_MODE_NONE;
        const Diligent::LayoutElement layout[]{{0,0,3,Diligent::VT_FLOAT32,Diligent::False,0,sizeof(DebugGpuVertex)},{1,0,4,Diligent::VT_FLOAT32,Diligent::False,12,sizeof(DebugGpuVertex)}};
        ci.GraphicsPipeline.InputLayout={layout,2};ci.PSODesc.ResourceLayout.DefaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
        Pipeline candidate;device->CreateGraphicsPipelineState(ci,&candidate.state);
        if (!candidate.state) return error("debug pipeline creation failed");
        candidate.state->CreateShaderResourceBinding(&candidate.resources,true);
        if (!candidate.resources) return error("debug binding creation failed");
        bindConstants(candidate);debug_pipeline=std::move(candidate);
    }
    std::vector<DebugGpuVertex> vertices;vertices.reserve(snapshot.debug_lines.size()*2U);
    for (const auto& line:snapshot.debug_lines) {
        if (!finite(line.start)||!finite(line.end)||!std::isfinite(line.color.r)||!std::isfinite(line.color.g)||!std::isfinite(line.color.b)||!std::isfinite(line.color.a)) return error("invalid debug vertex");
        for (auto point:{line.start,line.end}) {
            DebugGpuVertex v{};v.position[0]=point.x;v.position[1]=point.y;v.position[2]=point.z;color(v.color,line.color,line.color.a);vertices.push_back(v);
        }
    }
    const auto bytes=vertices.size()*sizeof(DebugGpuVertex);
    if (auto r=grow(debug_buffer,debug_capacity,bytes,Diligent::BIND_VERTEX_BUFFER,"Genomes debug stream");!r)return r;
    if (auto r=mapCopy(debug_buffer,vertices.data(),bytes);!r)return r;
    viewport(camera);context->SetPipelineState(debug_pipeline.state);context->CommitShaderResources(debug_pipeline.resources,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Diligent::IBuffer* vb=debug_buffer;const Diligent::Uint64 offset=0;
    context->SetVertexBuffers(0,1,&vb,&offset,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
    context->Draw(Diligent::DrawAttribs{static_cast<Diligent::Uint32>(vertices.size()),Diligent::DRAW_FLAG_NONE});++telemetry.draw_calls;
    return RenderResult::success();
}
RenderResult DiligentBackend::Impl::renderUi(const ui::UiRenderFrame& frame) {
    if (frame.commands.empty()&&frame.widgets.empty()) return RenderResult::success();
    if (!ui_pipeline.state) {
        Ptr<Diligent::IShader> vs,ps;
        if (!compile(ui_vs,"Genomes UI VS",Diligent::SHADER_TYPE_VERTEX,vs)||!compile(ui_ps,"Genomes UI PS",Diligent::SHADER_TYPE_PIXEL,ps))return error("UI shader creation failed");
        Diligent::GraphicsPipelineStateCreateInfo ci{};ci.PSODesc.Name="Genomes RmlUi";ci.pVS=vs;ci.pPS=ps;
        auto& g=ci.GraphicsPipeline;g.NumRenderTargets=1;g.RTVFormats[0]=swap->GetDesc().ColorBufferFormat;g.DSVFormat=Diligent::TEX_FORMAT_D32_FLOAT;
        g.PrimitiveTopology=Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;g.RasterizerDesc.CullMode=Diligent::CULL_MODE_NONE;g.RasterizerDesc.ScissorEnable=Diligent::True;
        g.DepthStencilDesc.DepthEnable=Diligent::False;g.DepthStencilDesc.DepthWriteEnable=Diligent::False;
        auto& b=g.BlendDesc.RenderTargets[0];b.BlendEnable=Diligent::True;b.SrcBlend=Diligent::BLEND_FACTOR_ONE;b.DestBlend=Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
        b.SrcBlendAlpha=Diligent::BLEND_FACTOR_ONE;b.DestBlendAlpha=Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
        const Diligent::LayoutElement layout[]{{0,0,2,Diligent::VT_FLOAT32,Diligent::False,0,sizeof(UiGpuVertex)},
            {1,0,2,Diligent::VT_FLOAT32,Diligent::False,8,sizeof(UiGpuVertex)},{2,0,4,Diligent::VT_FLOAT32,Diligent::False,16,sizeof(UiGpuVertex)}};
        g.InputLayout={layout,3};ci.PSODesc.ResourceLayout.DefaultVariableType=Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
        Diligent::SamplerDesc sampler{};sampler.MinFilter=sampler.MagFilter=sampler.MipFilter=Diligent::FILTER_TYPE_LINEAR;
        sampler.AddressU=sampler.AddressV=sampler.AddressW=Diligent::TEXTURE_ADDRESS_CLAMP;
        const Diligent::ImmutableSamplerDesc immutable{Diligent::SHADER_TYPE_PIXEL,"g_UiTexture",sampler};
        ci.PSODesc.ResourceLayout.ImmutableSamplers=&immutable;ci.PSODesc.ResourceLayout.NumImmutableSamplers=1U;
        device->CreateGraphicsPipelineState(ci,&ui_pipeline.state);
        if (!ui_pipeline.state)return error("UI pipeline creation failed");
    }
    const auto uploadTexture=[&](std::uint64_t id,std::uint64_t revision,std::uint32_t width,std::uint32_t height,const std::uint8_t* pixels)->RenderResult {
        auto it=ui_textures.find(id);
        if(it!=ui_textures.end()&&it->second.revision==revision&&it->second.width==width&&it->second.height==height)return RenderResult::success();
        Diligent::TextureDesc d{};d.Name="Genomes UI texture";d.Type=Diligent::RESOURCE_DIM_TEX_2D;
        d.Width=width;d.Height=height;d.Format=Diligent::TEX_FORMAT_RGBA8_UNORM;d.Usage=Diligent::USAGE_IMMUTABLE;d.BindFlags=Diligent::BIND_SHADER_RESOURCE;
        Diligent::TextureSubResData subresource{pixels,static_cast<Diligent::Uint64>(width)*4U};
        Diligent::TextureData data{&subresource,1U};UiTextureGpu candidate;
        device->CreateTexture(d,&data,&candidate.texture);if(!candidate.texture)return error("UI texture allocation failed");
        ui_pipeline.state->CreateShaderResourceBinding(&candidate.resources,true);if(!candidate.resources)return error("UI resource binding allocation failed");
        auto* texture=candidate.resources->GetVariableByName(Diligent::SHADER_TYPE_PIXEL,"g_UiTexture");
        auto* parameters=candidate.resources->GetVariableByName(Diligent::SHADER_TYPE_PIXEL,"UiParameters");
        if(!texture||!parameters)return error("UI shader resource reflection mismatch");
        texture->Set(candidate.texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));parameters->Set(ui_parameters);
        candidate.revision=revision;candidate.width=width;candidate.height=height;ui_textures[id]=std::move(candidate);
        ++telemetry.ui_texture_uploads;telemetry.ui_texture_upload_bytes+=static_cast<std::uint64_t>(width)*height*4U;
        return RenderResult::success();
    };
    const std::uint8_t white[]{255,255,255,255};if(auto r=uploadTexture(0U,1U,1U,1U,white);!r)return r;
    std::set<std::uint64_t> live{0U};
    for(const auto& texture:frame.textures) {
        const std::uint64_t bytes=static_cast<std::uint64_t>(texture.width)*texture.height*4U;
        if(!texture.id||!texture.width||!texture.height||texture.width>16384U||texture.height>16384U||bytes!=texture.rgba.size())return error("invalid UI texture payload");
        if(!live.insert(texture.id).second)return error("duplicate UI texture id");
        if(auto r=uploadTexture(texture.id,texture.content_revision,texture.width,texture.height,texture.rgba.data());!r)return r;
    }
    const auto& desc=swap->GetDesc();const float width=static_cast<float>(desc.Width),height=static_cast<float>(desc.Height);
    struct Draw {std::uint32_t first,count;std::uint64_t texture;Diligent::Rect scissor;bool premultiplied;};
    std::vector<UiGpuVertex> vertices;std::vector<std::uint32_t> indices;std::vector<Draw> draws;
    const Diligent::Rect full{0,0,static_cast<Diligent::Int32>(desc.Width),static_cast<Diligent::Int32>(desc.Height)};
    const auto vertex=[&](float x,float y,float u,float v,foundation::Color c) {
        UiGpuVertex out{};out.position[0]=x/width*2-1;out.position[1]=1-y/height*2;out.uv[0]=u;out.uv[1]=v;color(out.color,c,c.a);vertices.push_back(out);
    };
    const auto rectangle=[&](float x,float y,float w,float h,foundation::Color c) {
        if(w<=0||h<=0)return;
        const auto base=static_cast<std::uint32_t>(vertices.size());
        vertex(x,y,0,0,c);vertex(x+w,y,1,0,c);vertex(x+w,y+h,1,1,c);vertex(x,y+h,0,1,c);
        indices.insert(indices.end(),{base,base+1U,base+2U,base,base+2U,base+3U});
    };
    const auto text=[&](std::string_view value,float x,float y,float scale,foundation::Color c) {
        float cursor=x;
        for(char ch:value) {
            const auto glyph=diligent_ui::glyph(ch);
            for(std::size_t row=0;row<7U;++row)for(std::size_t col=0;col<5U;++col)
                if((glyph[row]&(1U<<(4U-col)))!=0U)rectangle(cursor+static_cast<float>(col)*scale,y+static_cast<float>(row)*scale,scale,scale,c);
            cursor+=6*scale;if(cursor>width)break;
        }
    };
    if(frame.commands.empty()) {
        float y=150;
        for(const auto& w:frame.widgets) {
            if(w.type==ui::UiWidgetType::Panel){rectangle(48,48,std::min(w.width,width-96),std::min(w.height,height-96),{.025F,.035F,.06F,1});text(w.text,65,65,2,{.85F,.9F,.95F,1});}
            else {if(w.type==ui::UiWidgetType::Button)rectangle(65,y,std::min(w.width,width-90),w.height,w.selected?foundation::Color{.1F,.38F,.67F,1}:foundation::Color{.07F,.11F,.18F,1});text(w.text,78,y+8,1.5F,{.85F,.9F,.95F,1});y+=std::max(w.height,30.0F)+10;}
        }
        if(!indices.empty())draws.push_back({0U,static_cast<std::uint32_t>(indices.size()),0U,full,false});
    } else for(const auto& command:frame.commands) {
        if(vertices.size()+command.vertices.size()>1'000'000U||indices.size()+command.indices.size()>6'000'000U)return error("UI geometry budget exceeded");
        const auto first=static_cast<std::uint32_t>(indices.size());
        bool premultiplied=false;
        if(!command.vertices.empty()) {
            const auto base=static_cast<std::uint32_t>(vertices.size());
            for(const auto& v:command.vertices) {
                if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.u)||!std::isfinite(v.v)||!std::isfinite(v.color.r)||!std::isfinite(v.color.g)||!std::isfinite(v.color.b)||!std::isfinite(v.color.a))return error("non-finite UI vertex");
                vertex(v.x,v.y,v.u,v.v,v.color);
            }
            if(command.indices.empty()) {if(command.vertices.size()%3U)return error("UI triangle stream is incomplete");for(std::uint32_t k=0;k<command.vertices.size();++k)indices.push_back(base+k);}
            else {if(command.indices.size()%3U)return error("UI index stream is incomplete");for(auto i:command.indices){if(i>=command.vertices.size())return error("UI index out of range");indices.push_back(base+i);}}
            // The pinned RmlUi Vertex contract is premultiplied sRGB; legacy rectangles below are straight.
            premultiplied=true;
        } else if(command.primitive==ui::UiDrawPrimitive::Text)text(command.text,command.rect.x,command.rect.y,1.5F,command.color);
        else rectangle(command.rect.x,command.rect.y,command.rect.width,command.rect.height,command.color);
        const auto count=static_cast<std::uint32_t>(indices.size())-first;if(!count)continue;
        Diligent::Rect clip=full;
        if(command.scissor_enabled) {
            const auto r=command.scissor;
            if(!std::isfinite(r.x)||!std::isfinite(r.y)||!std::isfinite(r.width)||!std::isfinite(r.height))return error("invalid UI scissor");
            clip.left=static_cast<Diligent::Int32>(std::floor(std::clamp(r.x,0.0F,width)));
            clip.top=static_cast<Diligent::Int32>(std::floor(std::clamp(r.y,0.0F,height)));
            clip.right=static_cast<Diligent::Int32>(std::ceil(std::clamp(r.x+r.width,0.0F,width)));
            clip.bottom=static_cast<Diligent::Int32>(std::ceil(std::clamp(r.y+r.height,0.0F,height)));
        }
        if(command.texture && !live.contains(command.texture))return error("UI draw references unavailable texture");
        if(clip.right>clip.left&&clip.bottom>clip.top)draws.push_back({first,count,command.texture,clip,premultiplied});
    }
    if(draws.empty())return RenderResult::success();
    const auto vb=vertices.size()*sizeof(UiGpuVertex),ib=indices.size()*sizeof(std::uint32_t);
    const auto old_vb=ui_vertex_capacity,old_ib=ui_index_capacity;
    if(auto r=grow(ui_vertices_buffer,ui_vertex_capacity,vb,Diligent::BIND_VERTEX_BUFFER,"Genomes UI vertices");!r)return r;
    if(auto r=grow(ui_indices_buffer,ui_index_capacity,ib,Diligent::BIND_INDEX_BUFFER,"Genomes UI indices");!r)return r;
    telemetry.ui_buffer_grows+=(old_vb!=ui_vertex_capacity?1U:0U)+(old_ib!=ui_index_capacity?1U:0U);
    if(auto r=mapCopy(ui_vertices_buffer,vertices.data(),vb);!r)return r;
    if(auto r=mapCopy(ui_indices_buffer,indices.data(),ib);!r)return r;
    viewport(RenderCamera{});context->SetPipelineState(ui_pipeline.state);
    Diligent::IBuffer* vertex_buffer=ui_vertices_buffer;const Diligent::Uint64 zero=0;
    context->SetVertexBuffers(0,1,&vertex_buffer,&zero,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
    context->SetIndexBuffer(ui_indices_buffer,0,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    for(const auto& draw:draws) {
        const float parameters[]{draw.premultiplied?1.0F:0.0F,0,0,0};if(auto r=mapCopy(ui_parameters,parameters,sizeof(parameters));!r)return r;
        context->CommitShaderResources(ui_textures.at(draw.texture).resources,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context->SetScissorRects(1U,&draw.scissor,desc.Width,desc.Height);
        Diligent::DrawIndexedAttribs attributes{draw.count,Diligent::VT_UINT32,Diligent::DRAW_FLAG_NONE};attributes.FirstIndexLocation=draw.first;
        context->DrawIndexed(attributes);++telemetry.ui_draw_calls;
    }
    for(auto it=ui_textures.begin();it!=ui_textures.end();) {if(!live.contains(it->first))it=ui_textures.erase(it);else ++it;}
    return RenderResult::success();
}
} // namespace genomes::render
