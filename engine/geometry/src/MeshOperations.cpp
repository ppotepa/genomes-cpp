#include <genomes/geometry/MeshOperations.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace genomes::geometry {
namespace {
foundation::Error invalid(const char* message) { return {foundation::ErrorCode::InvalidArgument, message}; }
void refresh(MeshData& mesh) { mesh.rebuildStreams(); mesh.submeshes.clear(); mesh.submeshes.push_back({0U,static_cast<std::uint32_t>(mesh.indices.size()),0U}); }
}

foundation::Result<MeshData, foundation::Error> combine(std::span<const MeshData> meshes) {
    MeshData result{};
    for (const auto& source : meshes) {
        if (!source.valid()) return foundation::Result<MeshData, foundation::Error>::failure(invalid("cannot combine invalid mesh"));
        if (source.vertices.empty()) return foundation::Result<MeshData, foundation::Error>::failure({foundation::ErrorCode::Unsupported,"stream-only combine requires an attribute adapter"});
        if (result.vertices.size() > std::numeric_limits<std::uint32_t>::max()-source.vertices.size() || result.indices.size() > std::numeric_limits<std::uint32_t>::max()-source.indices.size()) return foundation::Result<MeshData, foundation::Error>::failure({foundation::ErrorCode::OutOfRange,"combined mesh exceeds 32-bit limits"});
        const auto vertex_base=static_cast<std::uint32_t>(result.vertices.size());
        const auto index_base=static_cast<std::uint32_t>(result.indices.size());
        result.vertices.insert(result.vertices.end(),source.vertices.begin(),source.vertices.end());
        result.indices.reserve(result.indices.size()+source.indices.size());
        for(const auto index:source.indices) result.indices.push_back(vertex_base+index);
        for(const auto range:source.submeshes) result.submeshes.push_back({index_base+range.first_index,range.index_count,range.material_index});
    }
    if(result.vertices.empty()||result.indices.empty()) return foundation::Result<MeshData, foundation::Error>::failure(invalid("cannot combine empty mesh span"));
    result.rebuildStreams(); if(result.submeshes.empty()) result.submeshes.push_back({0U,static_cast<std::uint32_t>(result.indices.size()),0U});
    return foundation::Result<MeshData, foundation::Error>::success(std::move(result));
}

foundation::Result<MeshData, foundation::Error> transform(const MeshData& source,const math::Transform& operation) {
    if(!operation.valid()) return foundation::Result<MeshData, foundation::Error>::failure(invalid("mesh transform is invalid"));
    if(!source.valid()||source.vertices.empty()) return foundation::Result<MeshData, foundation::Error>::failure({foundation::ErrorCode::Unsupported,"stream-only transform requires an attribute adapter"});
    MeshData result=source; const auto matrix=operation.matrix(); const auto inverse=matrix.inverse(); if(!inverse) return foundation::Result<MeshData, foundation::Error>::failure({foundation::ErrorCode::InvalidArgument,"mesh transform is singular"});
    for(auto& vertex:result.vertices){ vertex.position=math::transformPoint(matrix,vertex.position); const math::Vec3 n{(*inverse)(0,0)*vertex.normal.x+(*inverse)(1,0)*vertex.normal.y+(*inverse)(2,0)*vertex.normal.z,(*inverse)(0,1)*vertex.normal.x+(*inverse)(1,1)*vertex.normal.y+(*inverse)(2,1)*vertex.normal.z,(*inverse)(0,2)*vertex.normal.x+(*inverse)(1,2)*vertex.normal.y+(*inverse)(2,2)*vertex.normal.z}; vertex.normal=math::normalized(n); }
    refresh(result); return foundation::Result<MeshData, foundation::Error>::success(std::move(result));
}

foundation::Result<MeshData, foundation::Error> recalculateNormals(const MeshData& source) {
    if(!source.valid()||source.vertices.empty()) return foundation::Result<MeshData, foundation::Error>::failure({foundation::ErrorCode::Unsupported,"stream-only normal generation requires an attribute adapter"});
    MeshData result=source; for(auto& vertex:result.vertices)vertex.normal={0,0,0};
    for(std::size_t i=0;i+2<result.indices.size();i+=3){auto& a=result.vertices[result.indices[i]];auto& b=result.vertices[result.indices[i+1]];auto& c=result.vertices[result.indices[i+2]];const auto n=math::cross(b.position-a.position,c.position-a.position);a.normal=a.normal+n;b.normal=b.normal+n;c.normal=c.normal+n;}
    for(auto& vertex:result.vertices)vertex.normal=math::normalized(vertex.normal); refresh(result); return foundation::Result<MeshData, foundation::Error>::success(std::move(result));
}

foundation::Result<MeshData, foundation::Error> convertIndexFormat(const MeshData& source,IndexFormat format) {
    if(!source.valid()) return foundation::Result<MeshData, foundation::Error>::failure(invalid("cannot convert invalid mesh"));
    if(format==IndexFormat::UInt16){for(const auto index:source.indices)if(index>std::numeric_limits<std::uint16_t>::max())return foundation::Result<MeshData, foundation::Error>::failure({foundation::ErrorCode::OutOfRange,"index does not fit UInt16"});}
    MeshData result=source; result.index_format=format; return foundation::Result<MeshData, foundation::Error>::success(std::move(result));
}
}
