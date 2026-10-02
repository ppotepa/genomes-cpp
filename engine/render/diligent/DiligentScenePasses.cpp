#include "DiligentBackendImpl.hpp"
#include <genomes/render/ProceduralMeshes.hpp>
#include <locale>
#include <set>
#include <sstream>
#include <tuple>

namespace genomes::render {
using namespace diligent_detail;
using namespace diligent_contract;
namespace {
bool validLights(const CharacterLightRig& lights) {
    const auto valid_color=[](foundation::Color a) {
        return std::isfinite(a.r)&&std::isfinite(a.g)&&std::isfinite(a.b)&&a.r>=0&&a.g>=0&&a.b>=0;
    };
    for (const auto* light:{&lights.key,&lights.fill}) {
        if (!math::finite(light->direction)||math::dot(light->direction,light->direction)<1.0e-8F||
            !valid_color(light->color)||!std::isfinite(light->intensity)||light->intensity<0) return false;
    }
    return valid_color(lights.hemisphere.sky)&&valid_color(lights.hemisphere.ground)&&
        std::isfinite(lights.hemisphere.intensity)&&lights.hemisphere.intensity>=0;
}
}
void DiligentBackend::Impl::viewport(const camera::PixelViewport& requested) {
    const auto& desc=swap->GetDesc();Diligent::Viewport value{};
    value.TopLeftX=static_cast<float>(requested.x);
    value.TopLeftY=static_cast<float>(requested.y);
    value.Width=static_cast<float>(requested.width);
    value.Height=static_cast<float>(requested.height);value.MinDepth=0;value.MaxDepth=1;
    context->SetViewports(1U,&value,desc.Width,desc.Height);
}
void DiligentBackend::Impl::restoreTargets() {
    Diligent::ITextureView* target=swap->GetCurrentBackBufferRTV();
    context->SetRenderTargets(1U,&target,depth_view,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    viewport(resolved_camera.viewport);
}
RenderResult DiligentBackend::Impl::setSceneConstants(bool shadow_pass) {
    SceneConstants constants{};const auto& vp=shadow_pass?shadow_matrix:camera_matrix;
    std::copy(vp.v.begin(),vp.v.end(),constants.view_projection);
    std::copy(shadow_uv_matrix.v.begin(),shadow_uv_matrix.v.end(),constants.shadow_uv_projection);
    vec(constants.camera_position,camera.position,1);
    vec(constants.key_direction_intensity,lights.key.direction,lights.key.intensity);
    color(constants.key_color,lights.key.color,1);
    vec(constants.fill_direction_intensity,lights.fill.direction,lights.fill.intensity);
    color(constants.fill_color,lights.fill.color,1);
    color(constants.hemisphere_sky,lights.hemisphere.sky,lights.hemisphere.intensity);
    color(constants.hemisphere_ground,lights.hemisphere.ground,lights.hemisphere.intensity);
    constants.shadow_parameters[0]=0.0008F;
    constants.shadow_parameters[1]=1.0F/static_cast<float>(shadow_size);
    constants.shadow_parameters[2]=shadow_pass?0.0F:1.0F;
    constants.shadow_parameters[3]=shadow_pass?1.0F:0.0F;
    return mapCopy(scene_buffer,&constants,sizeof(constants));
}
RenderResult DiligentBackend::Impl::prepare(const PresentationSnapshot& snapshot) {
    if (prepared) return RenderResult::success();
    items.clear();items.reserve(snapshot.instances.size()+3U);capture_metadata.clear();lights=snapshot.character_lights;
    if (!validLights(lights)) return error("non-finite or invalid scene lighting");
    std::unordered_map<foundation::StableId,std::shared_ptr<const RenderMesh>> meshes;
    std::unordered_map<foundation::StableId,std::shared_ptr<const SkinnedMeshPrototype>> skins;
    std::unordered_map<foundation::StableId,const SkinnedBonePalette*> poses;
    meshes.reserve(snapshot.instance_prototypes.size()+1U);
    skins.reserve(snapshot.skinned_prototypes.size());poses.reserve(snapshot.skinned_palettes.size());
    for (const auto& mesh:snapshot.instance_prototypes) if (mesh) {
        const auto [it,inserted]=meshes.emplace(mesh->mesh_id,mesh);
        if (!inserted&&it->second->revision!=mesh->revision) return error("conflicting regular prototype revisions");
    }
    for (const auto& mesh:snapshot.skinned_prototypes) if (mesh) {
        const auto [it,inserted]=skins.emplace(mesh->mesh_id,mesh);
        if (!inserted&&it->second->revision!=mesh->revision) return error("conflicting skinned prototype revisions");
    }
    for (const auto& pose:snapshot.skinned_palettes) {
        if (!poses.emplace(pose.instance_id,&pose).second) return error("duplicate pose instance id");
    }
    if (snapshot.infantry_mesh) meshes.emplace(snapshot.infantry_mesh->mesh_id,snapshot.infantry_mesh);
    const auto direct=[&](const std::shared_ptr<const RenderMesh>& source,bool cast)->RenderResult {
        if (!source||source->vertices.empty()||source->indices.empty()) return RenderResult::success();
        Item item;item.direct=true;item.instance.mesh_id=source->mesh_id;item.instance.object_id=source->mesh_id;
        item.instance.flags=RenderInstanceFlagReceiveShadow|(cast?RenderInstanceFlagCastShadow:0U);
        if (auto result=ensureRegular(source,item.gpu);!result) return result;
        items.push_back(item);return RenderResult::success();
    };
    if (auto result=direct(snapshot.terrain_mesh,false);!result) return result;
    if (auto result=direct(snapshot.world_mesh,true);!result) return result;
    if (snapshot.infantry_mesh&&std::none_of(snapshot.instances.begin(),snapshot.instances.end(),
        [&](const auto& instance){return instance.mesh_id==snapshot.infantry_mesh->mesh_id;})) {
        if (auto result=direct(snapshot.infantry_mesh,true);!result) return result;
    }
    std::set<foundation::StableId> ids;
    for (const auto& instance:snapshot.instances) {
        if (!instance.object_id||!validInstance(instance)||!ids.insert(instance.object_id).second)
            return error("invalid or duplicate render instance");
        Item item;item.instance=instance;
        if (const auto skin_it=skins.find(instance.mesh_id);skin_it!=skins.end()) {
            const auto pose_it=poses.find(instance.object_id);
            if (pose_it==poses.end()||!validPalette(*skin_it->second,*pose_it->second))
                return error("invalid or missing skinned palette");
            item.skin=skin_it->second.get();item.pose=pose_it->second;
            if (auto result=ensureSkin(skin_it->second,item.gpu);!result) return result;
        } else {
            std::shared_ptr<const RenderMesh> source;
            if (const auto mesh_it=meshes.find(instance.mesh_id);mesh_it!=meshes.end()) source=mesh_it->second;
            else if ((instance.flags&RenderInstanceFlagPreview)!=0U) {
                if (!preview_fallback) preview_fallback=procedural::make_box(
                    foundation::stable_id("mesh.preview.cube"),{.5F,.5F,.5F},{1,1,1,1});
                source=preview_fallback;
            } else return error("render instance has no published prototype");
            if (auto result=ensureRegular(source,item.gpu);!result) return result;
        }
        items.push_back(item);
    }
    if (auto result=prepareSkinPalettes();!result) return result;
    camera=snapshot.camera;
    const auto& desc=swap->GetDesc();
    if (have_resolved_camera) {
        const auto& viewport_pixels=resolved_camera.viewport;
        if (viewport_pixels.width<=0 || viewport_pixels.height<=0 ||
            viewport_pixels.x<0 || viewport_pixels.y<0 ||
            viewport_pixels.x+viewport_pixels.width>static_cast<int>(desc.Width) ||
            viewport_pixels.y+viewport_pixels.height>static_cast<int>(desc.Height))
            return error("resolved camera viewport is outside the framebuffer");
        camera.viewport_left=static_cast<float>(viewport_pixels.x)/static_cast<float>(desc.Width);
        camera.viewport_top=static_cast<float>(viewport_pixels.y)/static_cast<float>(desc.Height);
        camera.viewport_width=static_cast<float>(viewport_pixels.width)/static_cast<float>(desc.Width);
        camera.viewport_height=static_cast<float>(viewport_pixels.height)/static_cast<float>(desc.Height);
    } else {
        camera.viewport_left=0.0F;
        camera.viewport_top=0.0F;
        camera.viewport_width=1.0F;
        camera.viewport_height=1.0F;
    }
    if (camera.enabled&&!camera.valid()) return error("invalid scene camera");
    if (!camera.enabled) {
        V lo{-1,0,-1},hi{1,2,1};
        for (const auto& item:items) {
            const auto extent=item.gpu->half_extent;
            for (float x:{-extent.x,extent.x}) for (float y:{-extent.y,extent.y}) for (float z:{-extent.z,extent.z}) {
                const auto point=transformPoint(item.gpu->center+V{x,y,z},item.instance);
                lo={std::min(lo.x,point.x),std::min(lo.y,point.y),std::min(lo.z,point.z)};
                hi={std::max(hi.x,point.x),std::max(hi.y,point.y),std::max(hi.z,point.z)};
            }
        }
        camera={};camera.enabled=true;camera.target=(lo+hi)*.5F;
        const float extent=std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z,2.0F});
        camera.position=camera.target+V{extent,extent*.8F,extent};camera.far_plane=extent*12.0F;
    }
    const float aspect=static_cast<float>(desc.Width)*camera.viewport_width/
        (static_cast<float>(desc.Height)*camera.viewport_height);
    if (have_resolved_camera) {
        camera_matrix.v=resolved_camera.view_projection.m;
    } else {
        const auto view=math::lookAtRH(camera.position,camera.target,camera.up);
        auto projection=math::perspectiveD3D(camera.vertical_fov,aspect,
                                             camera.near_plane,camera.far_plane);
        projection(0,2)=-camera.projection_offset_x;
        projection(1,2)=-camera.projection_offset_y;
        camera_matrix.v=(projection*view).m;
    }
    const auto offset=camera.position-camera.target;
    const float radius=std::clamp(math::length(offset)*.8F,3.0F,160.0F);
    const V direction=math::normalized(lights.key.direction),up=std::abs(direction.y)>.95F?V{0,0,1}:V{0,1,0};
    shadow_matrix=multiply(ortho(radius,.1F,radius*6.0F),
        fromMath(math::lookAtRH(camera.target+direction*(radius*3.0F),camera.target,up)));
    const auto& ndc=device->GetDeviceInfo().GetNDCAttribs();
    Mat4 uv=identity();uv.v[0]=.5F;uv.v[5]=ndc.YtoVScale;uv.v[10]=ndc.ZtoDepthScale;
    uv.v[12]=uv.v[13]=.5F;uv.v[14]=ndc.GetZtoDepthBias();shadow_uv_matrix=multiply(uv,shadow_matrix);
    context->SetRenderTargets(0U,nullptr,shadow_depth,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    context->ClearDepthStencil(shadow_depth,Diligent::CLEAR_DEPTH_FLAG,1.0F,0U,Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Diligent::Viewport shadow_viewport{};
    shadow_viewport.Width=shadow_viewport.Height=static_cast<float>(shadow_size);shadow_viewport.MaxDepth=1;
    context->SetViewports(1U,&shadow_viewport,shadow_size,shadow_size);
    if (auto result=setSceneConstants(true);!result) return result;
    if (auto result=renderItems(false,true);!result) return result;
    restoreTargets();
    if (auto result=setSceneConstants(false);!result) return result;
    if (pending_capture) {
        std::ostringstream meta;meta.imbue(std::locale::classic());meta.precision(9);
        meta<<"\"camera\":{\"position\":["<<camera.position.x<<','<<camera.position.y<<','<<camera.position.z
            <<"],\"target\":["<<camera.target.x<<','<<camera.target.y<<','<<camera.target.z<<"],\"fov\":"<<camera.vertical_fov
            <<",\"near\":"<<camera.near_plane<<",\"far\":"<<camera.far_plane<<",\"viewport\":["<<camera.viewport_left<<','
            <<camera.viewport_top<<','<<camera.viewport_width<<','<<camera.viewport_height<<"]},\"instances\":[";
        bool first=true;
        for (const auto& item:items) {
            if (!first) {meta<<',';}
            first=false;
            meta<<"{\"id\":\""<<item.instance.object_id<<"\",\"mesh\":\""<<item.instance.mesh_id
                <<"\",\"revision\":\""<<item.gpu->revision<<'"';
            if (item.pose) {
                meta<<",\"pose_revision\":\""<<item.pose->pose_revision<<"\",\"skeleton\":\""<<item.pose->skeleton_id
                    <<"\",\"morph_weights\":[";
                for (std::size_t k=0;k<kMorphCount;++k) {if (k) {meta<<',';}meta<<item.pose->morph_weights[k];}
                meta<<']';
            }
            meta<<'}';
        }
        meta<<']';capture_metadata=meta.str();
    }
    prepared=true;return RenderResult::success();
}
RenderResult DiligentBackend::Impl::prepareSkinPalettes() {
    skin_palette_scratch.clear();
    std::size_t skin_items=0U;
    for (auto& item:items) {
        if (!item.skin || !item.pose) continue;
        item.skin_palette_index=static_cast<std::uint32_t>(skin_items*kBoneCount);
        skin_palette_scratch.insert(skin_palette_scratch.end(),item.pose->matrices.begin(),
                                    item.pose->matrices.end());
        ++skin_items;
    }
    if (skin_items==0U) return RenderResult::success();
    const std::size_t required=skin_palette_scratch.size();
    if (required>skin_palette_capacity) {
        std::size_t next=std::max<std::size_t>(kBoneCount,skin_palette_capacity);
        while (next<required) {
            if (next>std::numeric_limits<std::size_t>::max()/2U)
                return error("skin palette buffer size overflow",foundation::ErrorCode::Internal);
            next*=2U;
        }
        auto candidate=buffer("Genomes skin palette",next*sizeof(std::array<float,16U>),
                              Diligent::BIND_SHADER_RESOURCE,false,nullptr,
                              static_cast<std::uint32_t>(sizeof(std::array<float,16U>)));
        if (!candidate) return error("could not allocate skin palette buffer",
                                     foundation::ErrorCode::Internal);
        skin_palette_buffer=std::move(candidate);
        skin_palette_view=skin_palette_buffer->GetDefaultView(
            Diligent::BUFFER_VIEW_SHADER_RESOURCE);
        if (!skin_palette_view) return error("skin palette buffer view is missing",
                                             foundation::ErrorCode::Internal);
        skin_palette_capacity=next;
        for (auto& entry:pipelines) bindSkinPaletteBuffer(entry.second);
    }
    if (auto result=mapCopy(skin_palette_buffer,skin_palette_scratch.data(),
                            required*sizeof(std::array<float,16U>));!result) return result;
    telemetry.palette_updates+=static_cast<std::uint32_t>(skin_items);
    return RenderResult::success();
}
RenderResult DiligentBackend::Impl::renderItems(bool /*direct*/,bool shadow_pass) {
    using Key=std::tuple<foundation::StableId,std::uint64_t,foundation::StableId,
                         std::size_t,bool>;
    std::map<Key,std::vector<const Item*>> rigid,skin;
    struct Draw {const Item* item;std::size_t range;float depth;};
    std::vector<Draw> transparent;
    for (const auto& item:items) {
        if (shadow_pass&&(item.instance.flags&RenderInstanceFlagCastShadow)==0U) continue;
        for (std::size_t k=0;k<item.gpu->ranges.size();++k) {
            const auto alpha=effectiveAlpha(item.gpu->ranges[k],item.instance.tint);
            const auto delta=transformPoint(item.gpu->center,item.instance)-camera.position;
            if (alpha==MaterialAlphaMode::Blend) {
                if (!shadow_pass) transparent.push_back({&item,k,math::dot(delta,delta)});
                continue;
            }
            auto& batches=item.skin?skin:rigid;
            batches[{item.instance.mesh_id,item.gpu->revision,item.instance.material_id,k,
                (item.instance.flags&RenderInstanceFlagReceiveShadow)!=0U}].push_back(&item);
        }
    }
    std::stable_sort(transparent.begin(),transparent.end(),[](const Draw& a,const Draw& b) {
        if (a.depth!=b.depth) return a.depth>b.depth;
        if (a.item->instance.object_id!=b.item->instance.object_id)
            return a.item->instance.object_id<b.item->instance.object_id;
        return a.range<b.range;
    });

    instance_scratch.clear();
    skin_instance_scratch.clear();
    std::size_t regular_count=0U,skin_count=0U;
    for (const auto& entry:rigid) regular_count+=entry.second.size();
    for (const auto& entry:skin) skin_count+=entry.second.size();
    for (const auto& draw:transparent) {
        if (draw.item->skin) ++skin_count;
        else ++regular_count;
    }
    instance_scratch.reserve(regular_count);
    skin_instance_scratch.reserve(skin_count);
    const auto append_regular=[this](std::span<const Item* const> batch) {
        const std::size_t offset=instance_scratch.size();
        for (const auto* item:batch) {
            InstanceGpuVertex vertex{};
            vec(vertex.position_rotation,item->instance.position,item->instance.rotation_y);
            vec(vertex.scale,item->instance.scale,0.0F);
            color(vertex.tint,item->instance.tint,item->instance.tint.a);
            instance_scratch.push_back(vertex);
        }
        return offset;
    };
    const auto append_skin=[this](std::span<const Item* const> batch) {
        const std::size_t offset=skin_instance_scratch.size();
        for (const auto* item:batch) {
            SkinnedInstanceGpuVertex vertex{};
            vec(vertex.position_rotation,item->instance.position,item->instance.rotation_y);
            vec(vertex.scale,item->instance.scale,0.0F);
            color(vertex.tint,item->instance.tint,item->instance.tint.a);
            if (item->pose) {
                std::copy(item->pose->morph_weights.begin(),item->pose->morph_weights.end(),
                          vertex.morph_weights);
                vertex.debug_weight_bone=item->pose->debug_weight_bone<0
                    ? std::numeric_limits<std::uint32_t>::max()
                    : static_cast<std::uint32_t>(item->pose->debug_weight_bone);
            }
            vertex.palette_index=item->skin_palette_index;
            skin_instance_scratch.push_back(vertex);
        }
        return offset;
    };
    std::vector<std::size_t> rigid_offsets;
    rigid_offsets.reserve(rigid.size());
    for (const auto& entry:rigid) rigid_offsets.push_back(append_regular(entry.second));
    std::vector<std::size_t> skin_offsets;
    skin_offsets.reserve(skin.size());
    for (const auto& entry:skin) skin_offsets.push_back(append_skin(entry.second));
    std::vector<std::size_t> transparent_regular_offsets,transparent_skin_offsets;
    transparent_regular_offsets.reserve(transparent.size());
    transparent_skin_offsets.reserve(transparent.size());
    for (const auto& draw:transparent) {
        const Item* one=draw.item;
        if (draw.item->skin) transparent_skin_offsets.push_back(append_skin({&one,1U}));
        else transparent_regular_offsets.push_back(append_regular({&one,1U}));
    }
    if (!instance_scratch.empty()) {
        const std::size_t bytes=instance_scratch.size()*sizeof(InstanceGpuVertex);
        if (auto result=grow(instance_buffer,instance_capacity,bytes,Diligent::BIND_VERTEX_BUFFER,
                             "Genomes frame instance data");!result) return result;
        if (auto result=mapCopy(instance_buffer,instance_scratch.data(),bytes);!result) return result;
    }
    if (!skin_instance_scratch.empty()) {
        const std::size_t bytes=skin_instance_scratch.size()*sizeof(SkinnedInstanceGpuVertex);
        if (auto result=grow(skin_instance_buffer,skin_instance_capacity,bytes,
                             Diligent::BIND_VERTEX_BUFFER,"Genomes frame skin instances");!result)
            return result;
        if (auto result=mapCopy(skin_instance_buffer,skin_instance_scratch.data(),bytes);!result)
            return result;
    }

    std::size_t rigid_index=0U;
    for (const auto& [key,batch]:rigid) {
        if (auto result=drawRange(batch,std::get<3>(key),shadow_pass,
                                  rigid_offsets[rigid_index++]);!result) return result;
    }
    std::size_t skin_index=0U;
    for (const auto& [key,batch]:skin) {
        if (auto result=drawRange(batch,std::get<3>(key),shadow_pass,
                                  skin_offsets[skin_index++]);!result) return result;
    }
    std::size_t transparent_regular_index=0U,transparent_skin_index=0U;
    for (const auto& draw:transparent) {
        const Item* one=draw.item;
        const std::size_t offset=draw.item->skin
            ? transparent_skin_offsets[transparent_skin_index++]
            : transparent_regular_offsets[transparent_regular_index++];
        if (auto result=drawRange({&one,1U},draw.range,false,offset);!result) return result;
    }
    return RenderResult::success();
}
void DiligentBackend::Impl::retireMesh(Mesh& mesh) noexcept {
    if (!mesh.vertices && !mesh.indices && !mesh.owner) return;
    retired_meshes.push_back({std::move(mesh.vertices),std::move(mesh.indices),
                              std::move(mesh.owner),active_fence != 0U ? active_fence : last_submitted_fence});
    mesh.revision=0U;mesh.vertex_bytes=mesh.index_bytes=0U;
}
void DiligentBackend::Impl::retireCompleted() noexcept {
    if (!frame_fence) return;
    const auto completed=frame_fence->GetCompletedValue();
    retired_meshes.erase(std::remove_if(retired_meshes.begin(),retired_meshes.end(),
        [completed](const RetiredMesh& mesh) { return mesh.fence <= completed; }),retired_meshes.end());
}
void DiligentBackend::Impl::prune() {
    const auto old=telemetry.frame>600U?telemetry.frame-600U:0U;items.clear();
    for (auto it=regular_cache.begin();it!=regular_cache.end();) {
        if (it->second.last_seen<old) { retireMesh(it->second);it=regular_cache.erase(it); } else ++it;
    }
    for (auto it=skin_cache.begin();it!=skin_cache.end();) {
        if (it->second.last_seen<old) { retireMesh(it->second);it=skin_cache.erase(it); } else ++it;
    }
}
} // namespace genomes::render
