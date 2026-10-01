#include <genomes/geometry/SolidOps.hpp>
#include <genomes/geometry/MeshOperations.hpp>

#if GENOMES_HAS_MANIFOLD
#include <manifold/manifold.h>
#include <manifold/mesh.h>
#endif

#include <algorithm>
#include <limits>
#include <utility>

namespace genomes::geometry {
namespace {
foundation::Error unsupported() { return {foundation::ErrorCode::Unsupported,"Manifold CSG is disabled"}; }
[[maybe_unused]] foundation::Error invalid() { return {foundation::ErrorCode::InvalidArgument,"CSG requires valid closed meshes"}; }
#if GENOMES_HAS_MANIFOLD
foundation::Result<manifold::Manifold, foundation::Error> toManifold(const MeshData& mesh, std::uint32_t material_offset) {
    if(!mesh.valid()||mesh.vertices.empty())return foundation::Result<manifold::Manifold,foundation::Error>::failure(invalid());
    if (mesh.vertices.size() > std::numeric_limits<std::uint32_t>::max() ||
        mesh.indices.size() > std::numeric_limits<std::uint32_t>::max())
        return foundation::Result<manifold::Manifold,foundation::Error>::failure(
            {foundation::ErrorCode::OutOfRange,"CSG mesh exceeds uint32 index limits"});
    manifold::MeshGL gl{}; gl.vertProperties.reserve(mesh.vertices.size()*3U); gl.triVerts=mesh.indices;
    for(const auto& vertex:mesh.vertices){gl.vertProperties.push_back(vertex.position.x);gl.vertProperties.push_back(vertex.position.y);gl.vertProperties.push_back(vertex.position.z);}
    if (mesh.submeshes.empty()) {
        gl.runIndex={0U,static_cast<std::uint32_t>(gl.triVerts.size())};
        gl.runOriginalID={material_offset};
    } else {
        for (const auto range : mesh.submeshes) {
            if (range.material_index > std::numeric_limits<std::uint32_t>::max() - material_offset)
                return foundation::Result<manifold::Manifold,foundation::Error>::failure(
                    {foundation::ErrorCode::OutOfRange,"CSG material provenance overflow"});
            gl.runIndex.push_back(range.first_index);
            gl.runOriginalID.push_back(material_offset + range.material_index);
        }
        gl.runIndex.push_back(static_cast<std::uint32_t>(gl.triVerts.size()));
    }
    manifold::Manifold result(gl); if(result.Status()!=manifold::Manifold::Error::NoError)return foundation::Result<manifold::Manifold,foundation::Error>::failure(invalid()); return foundation::Result<manifold::Manifold,foundation::Error>::success(std::move(result));
}
foundation::Result<MeshData, foundation::Error> fromManifold(const manifold::Manifold& solid) {
    const auto gl=solid.GetMeshGL(); MeshData result{}; result.vertices.reserve(gl.NumVert()); result.indices.assign(gl.triVerts.begin(),gl.triVerts.end());
    for(std::size_t i=0;i<gl.NumVert();++i)result.vertices.push_back({{gl.vertProperties[i*gl.numProp],gl.vertProperties[i*gl.numProp+1U],gl.vertProperties[i*gl.numProp+2U]},{0,1,0},{}});
    if (gl.runOriginalID.empty()) {
        result.submeshes.push_back({0U,static_cast<std::uint32_t>(result.indices.size()),0U});
    } else {
        for (std::size_t run = 0; run < gl.runOriginalID.size(); ++run) {
            const std::size_t begin = run < gl.runIndex.size() ? gl.runIndex[run] : 0U;
            const std::size_t end = run + 1U < gl.runIndex.size() ? gl.runIndex[run + 1U] : gl.triVerts.size();
            if (begin > end || end > gl.triVerts.size() || (begin % 3U) != 0U || (end % 3U) != 0U)
                return foundation::Result<MeshData, foundation::Error>::failure(invalid());
            result.submeshes.push_back({static_cast<std::uint32_t>(begin), static_cast<std::uint32_t>(end - begin), gl.runOriginalID[run]});
        }
    }
    result.rebuildStreams();
    const auto normals = recalculateNormals(result);
    if (!normals) return foundation::Result<MeshData, foundation::Error>::failure(normals.error());
    return foundation::Result<MeshData, foundation::Error>::success(std::move(normals.value()));
}
#endif
}
foundation::Result<MeshData, foundation::Error> booleanSolid(const MeshData& left,const MeshData& right,SolidBoolean operation) {
#if !GENOMES_HAS_MANIFOLD
    (void)left;(void)right;(void)operation;return foundation::Result<MeshData,foundation::Error>::failure(unsupported());
#else
    std::uint32_t right_material_offset = left.submeshes.empty() ? 1U : 0U;
    for (const auto range : left.submeshes) {
        if (range.material_index == std::numeric_limits<std::uint32_t>::max())
            return foundation::Result<MeshData,foundation::Error>::failure(
                {foundation::ErrorCode::OutOfRange,"CSG material provenance overflow"});
        right_material_offset = std::max(right_material_offset, range.material_index + 1U);
    }
    auto a=toManifold(left,0U);if(!a)return foundation::Result<MeshData,foundation::Error>::failure(a.error()); auto b=toManifold(right,right_material_offset);if(!b)return foundation::Result<MeshData,foundation::Error>::failure(b.error()); manifold::OpType op=manifold::OpType::Add;if(operation==SolidBoolean::Difference)op=manifold::OpType::Subtract;else if(operation==SolidBoolean::Intersection)op=manifold::OpType::Intersect; auto result=a.value().Boolean(b.value(),op); if(result.Status()!=manifold::Manifold::Error::NoError)return foundation::Result<MeshData,foundation::Error>::failure(invalid()); return fromManifold(result);
#endif
}
} // namespace genomes::geometry
