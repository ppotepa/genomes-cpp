#include "DiligentBackendImpl.hpp"
#include <limits>

namespace genomes::render {
using namespace diligent_detail;
using namespace diligent_contract;
RenderResult DiligentBackend::Impl::drawRange(std::span<const Item* const> batch,std::size_t index,bool shadow_pass) {
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
    if (first.skin) {
        if (batch.size()!=1U||!first.pose) return error("skinned poses cannot share an instance draw");
        // One pose upload per consecutive instance within a pass, not per material.
        // renderItems resets the cache at each pass, so transient storage never crosses frames.
        if (last_skin_instance!=first.instance.object_id) {
            SkinnedPassConstants skin{};const auto& vp=shadow_pass?shadow_matrix:camera_matrix;
            std::copy(vp.v.begin(),vp.v.end(),skin.view_projection);
            vec(skin.object_position_scale,first.instance.position,static_cast<float>(first.pose->debug_weight_bone));
            vec(skin.object_scale_rotation,first.instance.scale,first.instance.rotation_y);vec(skin.camera_position,camera.position,1);
            std::copy(first.pose->morph_weights.begin(),first.pose->morph_weights.end(),skin.morph_weights);
            for (std::size_t k=0;k<kBoneCount;++k) std::copy(first.pose->matrices[k].begin(),first.pose->matrices[k].end(),skin.bone_palette[k]);
            if (auto r=mapCopy(skin_buffer,&skin,sizeof(skin));!r) return r;
            last_skin_instance=first.instance.object_id;++telemetry.palette_updates;
        }
    } else {
        instance_scratch.clear();instance_scratch.reserve(batch.size());
        for (const auto* item:batch) {
            InstanceGpuVertex v{};vec(v.position_rotation,item->instance.position,item->instance.rotation_y);
            vec(v.scale,item->instance.scale,0);color(v.tint,item->instance.tint,item->instance.tint.a);instance_scratch.push_back(v);
        }
        const auto bytes=instance_scratch.size()*sizeof(InstanceGpuVertex);
        if (auto r=grow(instance_buffer,instance_capacity,bytes,Diligent::BIND_VERTEX_BUFFER,"Genomes frame instance data");!r) return r;
        if (auto r=mapCopy(instance_buffer,instance_scratch.data(),bytes);!r) return r;
    }
    context->SetPipelineState(pipeline->state);
    context->CommitShaderResources(pipeline->resources,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Diligent::IBuffer* buffers[]{first.gpu->vertices,instance_buffer};const Diligent::Uint64 offsets[]{0,0};
    context->SetVertexBuffers(0,first.skin?1U:2U,buffers,offsets,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
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
