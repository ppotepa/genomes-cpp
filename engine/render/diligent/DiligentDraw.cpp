#include "DiligentBackendImpl.hpp"
#include <limits>

namespace genomes::render {
using namespace diligent_detail;
using namespace diligent_contract;
RenderResult DiligentBackend::Impl::drawRange(std::span<const Item* const> batch,std::size_t index,
                                               bool shadow_pass,std::size_t instance_offset) {
    if (batch.empty()||batch.size()>std::numeric_limits<std::uint32_t>::max()) return error("invalid draw batch");
    const auto& first=*batch.front();const auto& range=first.gpu->ranges[index];
    const auto alpha=effectiveAlpha(range,first.instance.tint);Pipeline* pipeline=nullptr;
    if (auto r=createPipeline(first.skin!=nullptr,shadow_pass,range.material.double_sided,alpha,pipeline);!r) return r;
    MaterialConstants material{};color(material.base_color,range.material.base_color,range.material.base_color.a);
    material.factors[0]=range.material.roughness;material.factors[1]=range.material.metalness;
    material.factors[2]=range.material.opacity;material.factors[3]=range.material.alpha_cutoff;
    color(material.tint,first.instance.tint,first.instance.tint.a);
    material.flags[0]=range.material.vertex_color?1U:0U;material.flags[1]=range.material.instance_tint?1U:0U;
    material.flags[2]=static_cast<std::uint32_t>(alpha);material.flags[3]=(first.instance.flags&RenderInstanceFlagReceiveShadow)!=0U?1U:0U;
    if (auto r=mapCopy(material_buffer,&material,sizeof(material));!r) return r;
    context->SetPipelineState(pipeline->state);
    context->CommitShaderResources(pipeline->resources,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Diligent::IBuffer* buffers[]{first.gpu->vertices,
                                 first.skin?skin_instance_buffer:instance_buffer};
    const auto instance_stride=first.skin?sizeof(SkinnedInstanceGpuVertex)
                                         :sizeof(InstanceGpuVertex);
    const Diligent::Uint64 offsets[]{0,static_cast<Diligent::Uint64>(
        instance_offset*instance_stride)};
    context->SetVertexBuffers(0,2U,buffers,offsets,
                              Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                              Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
    context->SetIndexBuffer(first.gpu->indices,0,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Diligent::DrawIndexedAttribs draw{
        range.index_count,Diligent::VT_UINT32,
        config.validation?Diligent::DRAW_FLAG_VERIFY_ALL:Diligent::DRAW_FLAG_NONE,
        static_cast<Diligent::Uint32>(batch.size())};
    draw.FirstIndexLocation=range.first_index;
    context->DrawIndexed(draw);++telemetry.draw_calls;
    return RenderResult::success();
}
} // namespace genomes::render
