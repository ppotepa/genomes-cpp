#include <genomes/geometry/MeshOperations.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace genomes::geometry {
namespace {
foundation::Error invalid(const char* message) { return {foundation::ErrorCode::InvalidArgument, message}; }
foundation::Vec3 transformNormal(const math::Mat4& inverse, foundation::Vec3 normal) noexcept {
    // Normals use the inverse transpose of the linear transform. Mat4 is
    // column-major, so each output component reads one column of M^-1.
    const foundation::Vec3 transformed{
        inverse(0, 0) * normal.x + inverse(1, 0) * normal.y + inverse(2, 0) * normal.z,
        inverse(0, 1) * normal.x + inverse(1, 1) * normal.y + inverse(2, 1) * normal.z,
        inverse(0, 2) * normal.x + inverse(1, 2) * normal.y + inverse(2, 2) * normal.z};
    return math::normalized(transformed);
}

void refreshBounds(MeshData& mesh) noexcept {
    mesh.bounds = {};
    if (!mesh.positions.empty()) {
        for (const auto position : mesh.positions) mesh.bounds.include(position);
    } else {
        for (const auto& vertex : mesh.vertices) mesh.bounds.include(vertex.position);
    }
}

void refreshDefaultSubmesh(MeshData& mesh) {
    if (mesh.submeshes.empty())
        mesh.submeshes.push_back({0U, static_cast<std::uint32_t>(mesh.indices.size()), 0U});
}

void reverseTriangleWinding(MeshData& mesh) noexcept {
    for (std::size_t index = 0; index + 2U < mesh.indices.size(); index += 3U)
        std::swap(mesh.indices[index + 1U], mesh.indices[index + 2U]);
}
}

foundation::Result<MeshData, foundation::Error> combine(std::span<const MeshData> meshes) {
    MeshData result{};
    if (meshes.empty())
        return foundation::Result<MeshData, foundation::Error>::failure(
            invalid("cannot combine an empty mesh span"));
    bool has_tangents = false;
    bool has_colors = false;
    bool initialized_stream_policy = false;
    for (const auto& source : meshes) {
        if (!source.valid()) return foundation::Result<MeshData, foundation::Error>::failure(invalid("cannot combine invalid mesh"));
        const std::size_t source_vertex_count = source.positions.empty() ? source.vertices.size() : source.positions.size();
        const bool source_has_tangents = !source.tangents.empty();
        const bool source_has_colors = !source.colors.empty();
        if (!initialized_stream_policy) {
            has_tangents = source_has_tangents;
            has_colors = source_has_colors;
            initialized_stream_policy = true;
        } else if (source_has_tangents != has_tangents || source_has_colors != has_colors) {
            return foundation::Result<MeshData, foundation::Error>::failure(
                invalid("combined meshes must agree on optional stream presence"));
        }
        if (source_vertex_count > std::numeric_limits<std::uint32_t>::max() ||
            source.indices.size() > std::numeric_limits<std::uint32_t>::max() ||
            result.vertices.size() > std::numeric_limits<std::uint32_t>::max()-source_vertex_count ||
            result.indices.size() > std::numeric_limits<std::uint32_t>::max()-source.indices.size())
            return foundation::Result<MeshData, foundation::Error>::failure(
                {foundation::ErrorCode::OutOfRange,"combined mesh exceeds 32-bit limits"});
        const auto vertex_base=static_cast<std::uint32_t>(result.vertices.size());
        const auto index_base=static_cast<std::uint32_t>(result.indices.size());
        result.vertices.reserve(result.vertices.size()+source_vertex_count);
        for (std::size_t vertex_index = 0; vertex_index < source_vertex_count; ++vertex_index) {
            if (!source.vertices.empty()) {
                result.vertices.push_back(source.vertices[vertex_index]);
            } else {
                result.vertices.push_back({source.positions[vertex_index], source.normals[vertex_index],
                                          source.uvs[vertex_index]});
            }
        }
        if (has_tangents)
            result.tangents.insert(result.tangents.end(), source.tangents.begin(), source.tangents.end());
        if (has_colors)
            result.colors.insert(result.colors.end(), source.colors.begin(), source.colors.end());
        result.indices.reserve(result.indices.size()+source.indices.size());
        for(const auto index:source.indices) result.indices.push_back(vertex_base+index);
        if (source.submeshes.empty()) {
            result.submeshes.push_back({index_base, static_cast<std::uint32_t>(source.indices.size()), 0U});
        } else {
            for(const auto range:source.submeshes)
                result.submeshes.push_back({index_base+range.first_index,range.index_count,range.material_index});
        }
    }
    if(result.vertices.empty()||result.indices.empty()) return foundation::Result<MeshData, foundation::Error>::failure(invalid("cannot combine empty mesh span"));
    result.rebuildStreams();
    return foundation::Result<MeshData, foundation::Error>::success(std::move(result));
}

foundation::Result<MeshData, foundation::Error> transform(const MeshData& source,const math::Transform& operation) {
    if(!operation.valid()) return foundation::Result<MeshData, foundation::Error>::failure(invalid("mesh transform is invalid"));
    if(!source.valid()) return foundation::Result<MeshData, foundation::Error>::failure(invalid("cannot transform invalid mesh"));
    const auto matrix=operation.matrix(); const auto inverse=matrix.inverse();
    if(!inverse) return foundation::Result<MeshData, foundation::Error>::failure({foundation::ErrorCode::InvalidArgument,"mesh transform is singular"});
    MeshData result=source;
    if (!result.vertices.empty()) {
        for (auto& vertex : result.vertices) {
            vertex.position = math::transformPoint(matrix, vertex.position);
            vertex.normal = transformNormal(*inverse, vertex.normal);
        }
    }
    for (auto& position : result.positions) position = math::transformPoint(matrix, position);
    for (auto& normal : result.normals) normal = transformNormal(*inverse, normal);
    for (auto& tangent : result.tangents) {
        const auto xyz = transformNormal(*inverse, {tangent.x, tangent.y, tangent.z});
        tangent.x = xyz.x; tangent.y = xyz.y; tangent.z = xyz.z;
    }
    if (operation.scale.x * operation.scale.y * operation.scale.z < 0.0F) {
        reverseTriangleWinding(result);
        for (auto& tangent : result.tangents) tangent.w = -tangent.w;
    }
    refreshBounds(result);
    refreshDefaultSubmesh(result);
    return foundation::Result<MeshData, foundation::Error>::success(std::move(result));
}

foundation::Result<MeshData, foundation::Error> recalculateNormals(const MeshData& source) {
    if(!source.valid()||source.vertices.empty()) return foundation::Result<MeshData, foundation::Error>::failure({foundation::ErrorCode::Unsupported,"stream-only normal generation requires an attribute adapter"});
    MeshData result=source; const auto submeshes=result.submeshes; for(auto& vertex:result.vertices)vertex.normal={0,0,0};
    for(std::size_t i=0;i+2<result.indices.size();i+=3){auto& a=result.vertices[result.indices[i]];auto& b=result.vertices[result.indices[i+1]];auto& c=result.vertices[result.indices[i+2]];const auto n=math::cross(b.position-a.position,c.position-a.position);a.normal=a.normal+n;b.normal=b.normal+n;c.normal=c.normal+n;}
    for(auto& vertex:result.vertices)vertex.normal=math::normalized(vertex.normal); refreshBounds(result); result.submeshes=submeshes; if(result.submeshes.empty())result.submeshes.push_back({0U,static_cast<std::uint32_t>(result.indices.size()),0U}); return foundation::Result<MeshData, foundation::Error>::success(std::move(result));
}

foundation::Result<MeshData, foundation::Error> convertIndexFormat(const MeshData& source,IndexFormat format) {
    if(!source.valid()) return foundation::Result<MeshData, foundation::Error>::failure(invalid("cannot convert invalid mesh"));
    if(format==IndexFormat::UInt16){for(const auto index:source.indices)if(index>std::numeric_limits<std::uint16_t>::max())return foundation::Result<MeshData, foundation::Error>::failure({foundation::ErrorCode::OutOfRange,"index does not fit UInt16"});}
    MeshData result=source; result.index_format=format; return foundation::Result<MeshData, foundation::Error>::success(std::move(result));
}
}
