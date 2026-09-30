#include "DiligentBackendImpl.hpp"
#include <limits>

namespace genomes::render {
using namespace diligent_detail;
using namespace diligent_contract;
namespace {
template<class MeshType> bool validVertices(const MeshType& mesh) {
    if (!mesh.mesh_id || mesh.vertices.empty() || mesh.indices.empty() || mesh.indices.size()%3U ||
        mesh.vertices.size()>std::numeric_limits<std::uint32_t>::max() ||
        mesh.indices.size()>std::numeric_limits<std::uint32_t>::max()) return false;
    for (const auto& v:mesh.vertices) if (!finite(v.position)||!finite(v.normal)||
        !std::isfinite(v.uv.x)||!std::isfinite(v.uv.y)||!std::isfinite(v.color.r)||
        !std::isfinite(v.color.g)||!std::isfinite(v.color.b)||!std::isfinite(v.color.a)) return false;
    for (auto i:mesh.indices) if (i>=mesh.vertices.size()) return false;
    return true;
}
SkinnedGpuVertex packVertex(const SkinnedMeshPrototype& mesh,std::size_t index) {
    const auto& v=mesh.vertices[index];SkinnedGpuVertex p{};
    p.position[0]=v.position.x;p.position[1]=v.position.y;p.position[2]=v.position.z;
    p.normal[0]=v.normal.x;p.normal[1]=v.normal.y;p.normal[2]=v.normal.z;
    p.uv[0]=v.uv.x;p.uv[1]=v.uv.y;color(p.color,v.color,v.color.a);p.material_region=v.material_region;
    for (std::size_t k=0;k<4U;++k) {
        p.bone_indices[k]=static_cast<float>(v.bone_indices[k]);p.bone_weights[k]=v.bone_weights[k];
        if (k<mesh.morph_target_count) {
            const auto d=mesh.morphs[k].position_deltas[index],n=mesh.morphs[k].normal_deltas[index];
            p.morph_position[k][0]=d.x;p.morph_position[k][1]=d.y;p.morph_position[k][2]=d.z;
            p.morph_normal[k][0]=n.x;p.morph_normal[k][1]=n.y;p.morph_normal[k][2]=n.z;
        }
    }
    return p;
}
template<class Source,class Resident> void setBounds(const Source& source,Resident& gpu) {
    V lo=source.vertices.front().position,hi=lo;
    for (const auto& v:source.vertices) {
        lo={std::min(lo.x,v.position.x),std::min(lo.y,v.position.y),std::min(lo.z,v.position.z)};
        hi={std::max(hi.x,v.position.x),std::max(hi.y,v.position.y),std::max(hi.z,v.position.z)};
    }
    gpu.center=scale(add(lo,hi),.5F);gpu.half_extent=scale(sub(hi,lo),.5F);
}
}
RenderResult DiligentBackend::Impl::ensureRegular(const std::shared_ptr<const RenderMesh>& source,Mesh*& out) {
    if (!source) return error("missing regular prototype");
    if (source->vertices.size()>std::numeric_limits<std::uint32_t>::max() || source->indices.size()>std::numeric_limits<std::uint32_t>::max()) return error("mesh draw limits exceeded");
    auto& gpu=regular_cache[source->mesh_id];
    const auto vb=source->vertices.size()*sizeof(RenderMeshVertex),ib=source->indices.size()*sizeof(std::uint32_t);
    const bool changed=!gpu.vertices||!gpu.indices||gpu.revision!=source->revision||gpu.vertex_bytes!=vb||gpu.index_bytes!=ib||
        (source->revision==0U&&gpu.owner.get()!=source.get());
    if (changed || gpu.owner.get()!=source.get()) {
        if (!validVertices(*source)) return error("invalid regular mesh data",foundation::ErrorCode::InvalidArgument);
        auto plan=buildMaterialDrawPlan(*source);
        if (!plan) return RenderResult::failure(plan.error());
        if (changed) {
            auto v=buffer("Genomes persistent mesh vertices",vb,Diligent::BIND_VERTEX_BUFFER,false,source->vertices.data());
            if (!v) return error("could not allocate regular VB",foundation::ErrorCode::Internal);
            auto i=buffer("Genomes persistent mesh indices",ib,Diligent::BIND_INDEX_BUFFER,false,source->indices.data());
            if (!i) return error("could not allocate regular IB",foundation::ErrorCode::Internal);
            // Both candidates exist before any resident handle or key changes.
            gpu.vertices=std::move(v);gpu.indices=std::move(i);gpu.vertex_bytes=vb;gpu.index_bytes=ib;gpu.revision=source->revision;
            ++telemetry.mesh_uploads;++telemetry.total_mesh_uploads;
            telemetry.mesh_upload_bytes+=vb+ib;telemetry.total_mesh_upload_bytes+=vb+ib;
        }
        gpu.ranges=std::move(plan.value());gpu.owner=source;setBounds(*source,gpu);
    }
    gpu.last_seen=telemetry.frame;out=&gpu;return RenderResult::success();
}
RenderResult DiligentBackend::Impl::ensureSkin(const std::shared_ptr<const SkinnedMeshPrototype>& source,Mesh*& out) {
    if (!source) return error("missing skinned prototype");
    if (source->vertices.size()>std::numeric_limits<std::uint32_t>::max() || source->indices.size()>std::numeric_limits<std::uint32_t>::max()) return error("skin draw limits exceeded");
    auto& gpu=skin_cache[source->mesh_id];
    const auto vb=source->vertices.size()*sizeof(SkinnedGpuVertex),ib=source->indices.size()*sizeof(std::uint32_t);
    const bool changed=!gpu.vertices||!gpu.indices||gpu.revision!=source->revision||gpu.vertex_bytes!=vb||gpu.index_bytes!=ib||
        (source->revision==0U&&gpu.owner.get()!=source.get());
    if (changed || gpu.owner.get()!=source.get()) {
        if (!validVertices(*source)||source->morph_target_count>4U) return error("invalid skinned mesh data");
        for (const auto& v:source->vertices) {
            float sum=0;
            for (std::size_t k=0;k<4U;++k) {
                if (!std::isfinite(v.bone_weights[k])||v.bone_weights[k]<0||
                    (v.bone_weights[k]>0&&v.bone_indices[k]>=kBoneCount)) return error("invalid skin weights");
                sum+=v.bone_weights[k];
            }
            if (std::abs(sum-1)>1.0e-4F) return error("skin weights are not normalized");
        }
        for (std::size_t m=0;m<source->morph_target_count;++m) {
            const auto& morph=source->morphs[m];
            if (morph.position_deltas.size()!=source->vertices.size()||morph.normal_deltas.size()!=source->vertices.size()) return error("morph stream size mismatch");
            for (const auto d:morph.position_deltas) if (!finite(d)) return error("invalid morph position");
            for (const auto d:morph.normal_deltas) if (!finite(d)) return error("invalid morph normal");
        }
        auto plan=buildMaterialDrawPlan(*source);
        if (!plan) return RenderResult::failure(plan.error());
        if (changed) {
            std::vector<SkinnedGpuVertex> packed;packed.reserve(source->vertices.size());
            for (std::size_t k=0;k<source->vertices.size();++k) packed.push_back(packVertex(*source,k));
            auto v=buffer("Genomes persistent skin vertices",vb,Diligent::BIND_VERTEX_BUFFER,false,packed.data());
            if (!v) return error("could not allocate skinned VB",foundation::ErrorCode::Internal);
            auto i=buffer("Genomes persistent skin indices",ib,Diligent::BIND_INDEX_BUFFER,false,source->indices.data());
            if (!i) return error("could not allocate skinned IB",foundation::ErrorCode::Internal);
            gpu.vertices=std::move(v);gpu.indices=std::move(i);gpu.vertex_bytes=vb;gpu.index_bytes=ib;gpu.revision=source->revision;
            ++telemetry.mesh_uploads;++telemetry.total_mesh_uploads;
            telemetry.mesh_upload_bytes+=vb+ib;telemetry.total_mesh_upload_bytes+=vb+ib;
        }
        gpu.ranges=std::move(plan.value());gpu.owner=source;setBounds(*source,gpu);
    }
    gpu.last_seen=telemetry.frame;out=&gpu;return RenderResult::success();
}
} // namespace genomes::render
