#pragma once
#include "DiligentBackendImpl.hpp"

namespace genomes::render {

inline RenderResult DiligentBackend::Impl::prepareCamera(const PresentationSnapshot& snapshot) {
    using namespace diligent_detail;
    camera=snapshot.camera;
    if (camera.enabled) {
        if (!camera.valid()) return error("invalid scene camera or viewport");
    } else {
        Bounds bounds;
        for (const auto* mesh:{snapshot.terrain_mesh.get(),snapshot.world_mesh.get(),snapshot.infantry_mesh.get()})
            if (mesh) for (const auto& vertex:mesh->vertices) bounds.add(vertex.position);
        for (const auto& instance:snapshot.instances) {
            bool found=false;
            for (const auto& mesh:snapshot.instance_prototypes) if (mesh&&mesh->mesh_id==instance.mesh_id) {
                for (const auto& vertex:mesh->vertices) bounds.add(instancePoint(vertex.position,instance));
                found=true;break;
            }
            if (!found) for (const auto& mesh:snapshot.skinned_prototypes) if (mesh&&mesh->mesh_id==instance.mesh_id) {
                for (const auto& vertex:mesh->vertices) bounds.add(instancePoint(vertex.position,instance));
                found=true;break;
            }
            if (!found) { bounds.add(instancePoint({-0.5F,-0.5F,-0.5F},instance));bounds.add(instancePoint({0.5F,0.5F,0.5F},instance)); }
        }
        if (bounds.empty) { bounds.add({-0.5F,0,-0.5F});bounds.add({0.5F,2,0.5F}); }
        const Vec3 center{(bounds.minimum.x+bounds.maximum.x)*0.5F,
                          (bounds.minimum.y+bounds.maximum.y)*0.5F,
                          (bounds.minimum.z+bounds.maximum.z)*0.5F};
        const float extent=std::max({bounds.maximum.x-bounds.minimum.x,bounds.maximum.y-bounds.minimum.y,
                                     bounds.maximum.z-bounds.minimum.z,1.0F});
        camera={};camera.enabled=true;camera.target=center;
        camera.position={center.x+extent*1.8F,center.y+extent*1.2F,center.z+extent*1.8F};
        camera.near_plane=std::max(0.02F,extent*0.001F);camera.far_plane=extent*12.0F;
    }
    const auto desc=swap_chain->GetDesc();
    const float aspect=static_cast<float>(desc.Width)*camera.viewport_width/
        std::max(1.0F,static_cast<float>(desc.Height)*camera.viewport_height);
    view_projection=multiply(perspective(camera.vertical_fov,aspect,camera.near_plane,camera.far_plane),
                             lookAt(camera.position,camera.target,camera.up));
    CameraConstants value{};std::memcpy(value.view_projection,view_projection.v,sizeof(value));
    context->UpdateBuffer(camera_buffer,0,sizeof(value),&value,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    viewport(camera);
    return RenderResult::success();
}

inline RenderResult DiligentBackend::Impl::drawMeshes(const PresentationSnapshot& snapshot) {
    if (auto result=prepareCamera(snapshot);!result) return result;
    const bool dynamic=std::any_of(snapshot.instances.begin(),snapshot.instances.end(),[](const auto& i) {
        return (i.flags&RenderInstanceFlagDynamic)!=0;
    });
    const std::array<std::shared_ptr<const RenderMesh>,3U> meshes{
        snapshot.terrain_mesh,snapshot.world_mesh,dynamic?nullptr:snapshot.infantry_mesh};
    for (const auto& mesh:meshes) {
        if (!mesh||mesh->vertices.empty()||mesh->indices.empty()) continue;
        GpuMesh* gpu=nullptr;
        if (auto result=ensureMesh(mesh,gpu);!result) return result;
        bindDraw(terrain,*gpu,mesh->indices.size());
    }
    return RenderResult::success();
}

inline RenderResult DiligentBackend::Impl::drawInstances(const PresentationSnapshot& snapshot) {
    using namespace diligent_detail;
    if (auto result=prepareCamera(snapshot);!result) return result;
    const auto extraction=extractor.extract(snapshot);
    if (!extraction) return RenderResult::failure(extraction.error());
    const auto upload_batch=gpu_scene.apply(extraction.value().changes);
    if (!upload_batch) return RenderResult::failure(upload_batch.error());
    auto staged=upload_batch.value();
    if (gpu_scene.capacity()>instance_capacity) {
        const auto count=gpu_scene.capacity();
        if (count>std::numeric_limits<std::size_t>::max()/sizeof(gpu_scene::GpuInstanceRecord))
            return error("GPU instance table overflow");
        auto candidate=buffer("Genomes persistent instance table",count*sizeof(gpu_scene::GpuInstanceRecord),
                              Diligent::BIND_SHADER_RESOURCE,nullptr,sizeof(gpu_scene::GpuInstanceRecord));
        if (!candidate) return error("could not allocate GPU instance table",foundation::ErrorCode::Internal);
        instance_buffer=std::move(candidate);instance_capacity=count;staged=gpu_scene.fullUpload();
    }
    for (const auto& range:staged.ranges) {
        if (range.count==0) continue;
        if (!instance_buffer||range.payload_offset>staged.payload.size()||
            range.count>staged.payload.size()-range.payload_offset||range.first_slot>instance_capacity||
            range.count>instance_capacity-range.first_slot) return error("invalid instance upload range");
        context->UpdateBuffer(instance_buffer,range.first_slot*sizeof(gpu_scene::GpuInstanceRecord),
                              range.count*sizeof(gpu_scene::GpuInstanceRecord),staged.payload.data()+range.payload_offset,
                              Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    const auto findPalette=[&snapshot](foundation::StableId id)->const SkinnedBonePalette* {
        for (const auto& palette:snapshot.skinned_palettes) if (palette.instance_id==id) return &palette;
        return nullptr;
    };
    std::unordered_set<foundation::StableId> skinned_instances;
    std::unordered_map<foundation::StableId,std::uint64_t> seen_prototypes;
    for (const auto& mesh:snapshot.skinned_prototypes) {
        if (!mesh||mesh->vertices.empty()||mesh->indices.empty()) continue;
        const auto [seen,inserted]=seen_prototypes.emplace(mesh->mesh_id,mesh->revision);
        if (!inserted) {
            if (seen->second!=mesh->revision) return error("conflicting revisions of one skinned mesh id");
            continue;
        }
        GpuMesh* gpu=nullptr;
        for (const auto& instance:snapshot.instances) {
            if (instance.mesh_id!=mesh->mesh_id) continue;
            if (!validInstance(instance)) return error("invalid skinned instance transform");
            const auto* palette=findPalette(instance.object_id);
            if (!palette||palette->matrices.size()!=kBoneCount) return error("incomplete skinned palette");
            for (const auto& matrix:palette->matrices) {
                for (float value:matrix) if (!std::isfinite(value)) return error("nonfinite bone matrix");
                if (std::abs(matrix[3])+std::abs(matrix[7])+std::abs(matrix[11])>1.0e-4F||
                    std::abs(matrix[15]-1.0F)>1.0e-4F) return error("bone palette must be affine");
            }
            for (float value:palette->morph_weights) if (!std::isfinite(value)) return error("nonfinite morph weight");
            if (!gpu) { if (auto result=ensureSkin(mesh,gpu);!result) return result; }
            SkinnedPassConstants pass{};
            std::memcpy(pass.view_projection,view_projection.v,sizeof(pass.view_projection));
            vec(pass.object_position_scale,instance.position,static_cast<float>(palette->debug_weight_bone));
            vec(pass.object_scale_rotation,instance.scale,instance.rotation_y);
            vec(pass.camera_position,camera.position,1.0F);
            const auto& light=snapshot.character_lights;
            vec(pass.key_direction_intensity,light.key.direction,light.key.intensity);
            color(pass.key_color,light.key.color,light.key.color.a);
            vec(pass.fill_direction_intensity,light.fill.direction,light.fill.intensity);
            color(pass.fill_color,light.fill.color,light.fill.color.a);
            color(pass.hemisphere_sky,light.hemisphere.sky,light.hemisphere.intensity);
            color(pass.hemisphere_ground,light.hemisphere.ground,light.hemisphere.intensity);
            std::copy(palette->morph_weights.begin(),palette->morph_weights.end(),pass.morph_weights);
            for (std::size_t bone=0;bone<kBoneCount;++bone)
                std::memcpy(pass.bone_palette[bone],palette->matrices[bone].data(),sizeof(pass.bone_palette[bone]));
            context->UpdateBuffer(skinned_constants,0,sizeof(pass),&pass,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            ++telemetry.palette_updates;
            bindDraw(skinned,*gpu,mesh->indices.size());
            skinned_instances.insert(instance.object_id);
        }
        // A published resource without a visible instance is legal (Surface OFF).
    }
    constexpr std::uint32_t required=RenderInstanceFlagPreview|RenderInstanceFlagDynamic;
    using BatchKey=std::pair<foundation::StableId,foundation::StableId>;
    std::map<BatchKey,std::vector<std::uint32_t>> batches;
    for (const auto& instance:snapshot.instances) {
        if ((instance.flags&required)==0||skinned_instances.contains(instance.object_id)) continue;
        if (!validInstance(instance)) return error("invalid ordinary instance transform");
        const auto handle=gpu_scene.find(instance.object_id);
        if (!handle.isValid()) return error("missing GPU instance slot");
        batches[{instance.mesh_id,instance.material_id}].push_back(handle.index);
    }
    if (!batches.empty()) {
        if (!fallback) fallback=procedural::make_box(foundation::stable_id("mesh.preview.cube"),{0.5F,0.5F,0.5F},{1,1,1,1});
        auto* instance_variable=instances.resources?instances.resources->GetVariableByName(Diligent::SHADER_TYPE_VERTEX,"Instances"):nullptr;
        auto* remap_variable=instances.resources?instances.resources->GetVariableByName(Diligent::SHADER_TYPE_VERTEX,"InstanceIndices"):nullptr;
        if (!instance_variable||!remap_variable||!instance_buffer) return error("missing instance shader bindings");
        instance_variable->Set(instance_buffer->GetDefaultView(Diligent::BUFFER_VIEW_SHADER_RESOURCE),Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
        for (const auto& [key,slots]:batches) {
            if (slots.size()>std::numeric_limits<std::uint32_t>::max()) return error("instance batch too large");
            auto mesh=fallback;
            for (const auto& candidate:snapshot.instance_prototypes)
                if (candidate&&candidate->mesh_id==key.first) { mesh=candidate;break; }
            if (mesh==fallback&&snapshot.infantry_mesh&&snapshot.infantry_mesh->mesh_id==key.first) mesh=snapshot.infantry_mesh;
            GpuMesh* gpu=nullptr;
            if (auto result=ensureMesh(mesh,gpu);!result) return result;
            const std::size_t bytes=slots.size()*sizeof(std::uint32_t);
            if (!remap_buffer||bytes>remap_capacity) {
                const auto capacity=std::max<std::size_t>(bytes,1024U);
                auto candidate=buffer("Genomes persistent slot remap",capacity,Diligent::BIND_SHADER_RESOURCE,nullptr,sizeof(std::uint32_t));
                if (!candidate) return error("could not allocate slot remap",foundation::ErrorCode::Internal);
                remap_buffer=std::move(candidate);remap_capacity=capacity;
            }
            context->UpdateBuffer(remap_buffer,0,bytes,slots.data(),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            InstancePassConstants pass{};pass.options[0]=required;pass.options[1]=1;
            pass.mesh[0]=static_cast<std::uint32_t>(key.first);pass.mesh[1]=static_cast<std::uint32_t>(key.first>>32U);
            pass.material[0]=static_cast<std::uint32_t>(key.second);pass.material[1]=static_cast<std::uint32_t>(key.second>>32U);
            context->UpdateBuffer(instance_constants,0,sizeof(pass),&pass,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            remap_variable->Set(remap_buffer->GetDefaultView(Diligent::BUFFER_VIEW_SHADER_RESOURCE),Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            bindDraw(instances,*gpu,mesh->indices.size(),static_cast<std::uint32_t>(slots.size()));
        }
    }
    if (!snapshot.terrain_mesh&&!snapshot.world_mesh&&batches.empty()&&skinned_instances.empty()) {
        ui_vertices.clear();const auto d=swap_chain->GetDesc();
        diligent_ui::worldPlan(ui_vertices,snapshot,static_cast<float>(d.Width),static_cast<float>(d.Height));
        if (auto result=uploadUi();!result) return result;
    }
    return drawDebug(snapshot);
}

inline RenderResult DiligentBackend::Impl::drawDebug(const PresentationSnapshot& snapshot) {
    using namespace diligent_detail;
    if (snapshot.debug_lines.empty()) return RenderResult::success();
    debug_vertices.clear();
    const auto count=std::min<std::size_t>(snapshot.debug_lines.size(),32'768U);
    debug_vertices.reserve(count*2U);
    for (std::size_t k=0;k<count;++k) {
        const auto& line=snapshot.debug_lines[k];
        if (!finite(line.start)||!finite(line.end)) continue;
        for (const auto p:{line.start,line.end}) {
            DebugVertex v{};v.position[0]=p.x;v.position[1]=p.y;v.position[2]=p.z;
            color(v.color,line.color,line.color.a);debug_vertices.push_back(v);
        }
    }
    if (debug_vertices.empty()) return RenderResult::success();
    const auto bytes=debug_vertices.size()*sizeof(DebugVertex);
    if (!debug_buffer||bytes>debug_capacity) {
        auto candidate=buffer("Genomes debug line stream",bytes,Diligent::BIND_VERTEX_BUFFER);
        if (!candidate) return error("could not allocate debug buffer",foundation::ErrorCode::Internal);
        debug_buffer=std::move(candidate);debug_capacity=bytes;
    }
    context->UpdateBuffer(debug_buffer,0,bytes,debug_vertices.data(),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    viewport(camera);context->SetPipelineState(debug.state);
    context->CommitShaderResources(debug.resources,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Diligent::IBuffer* buffers[]{debug_buffer};const Diligent::Uint64 offsets[]{0};
    context->SetVertexBuffers(0,1,buffers,offsets,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
    context->Draw(Diligent::DrawAttribs{static_cast<Diligent::Uint32>(debug_vertices.size()),Diligent::DRAW_FLAG_NONE});
    ++telemetry.draw_calls;return RenderResult::success();
}

inline RenderResult DiligentBackend::Impl::uploadUi() {
    if (ui_vertices.empty()) return RenderResult::success();
    const auto bytes=ui_vertices.size()*sizeof(diligent_ui::Vertex);
    if (!ui_buffer||bytes>ui_capacity) {
        const auto capacity=std::max<std::size_t>(bytes,64U*1024U);
        auto candidate=buffer("Genomes UI stream",capacity,Diligent::BIND_VERTEX_BUFFER);
        if (!candidate) return diligent_detail::error("could not allocate UI stream",foundation::ErrorCode::Internal);
        ui_buffer=std::move(candidate);ui_capacity=capacity;
    }
    viewport(RenderCamera{});
    context->UpdateBuffer(ui_buffer,0,bytes,ui_vertices.data(),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    context->SetPipelineState(ui.state);
    Diligent::IBuffer* buffers[]{ui_buffer};const Diligent::Uint64 offsets[]{0};
    context->SetVertexBuffers(0,1,buffers,offsets,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
    context->Draw(Diligent::DrawAttribs{static_cast<Diligent::Uint32>(ui_vertices.size()),Diligent::DRAW_FLAG_NONE});
    ++telemetry.draw_calls;return RenderResult::success();
}

inline RenderResult DiligentBackend::Impl::drawUi(const ui::UiDocument& document) {
    ui_vertices.clear();const auto d=swap_chain->GetDesc();
    diligent_ui::document(ui_vertices,document,static_cast<float>(d.Width),static_cast<float>(d.Height));
    return uploadUi();
}
} // namespace genomes::render
