#include <genomes/geometry/MeshRepair.hpp>
#include <genomes/geometry/GeometryConstants.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace genomes::geometry {
namespace {
foundation::Vec3 sub(foundation::Vec3 a, foundation::Vec3 b) {
    return {a.x-b.x,a.y-b.y,a.z-b.z};
}
foundation::Vec3 cross(foundation::Vec3 a, foundation::Vec3 b) {
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
float dot(foundation::Vec3 a, foundation::Vec3 b) {
    return a.x*b.x+a.y*b.y+a.z*b.z;
}
bool finite(foundation::Vec3 v) {
    return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);
}
foundation::Vec3 normalized(foundation::Vec3 v) {
    const float length=std::sqrt(dot(v,v));
    return length>kNormalLengthEpsilon
        ? foundation::Vec3{v.x/length,v.y/length,v.z/length}
        : foundation::Vec3{0.0F,1.0F,0.0F};
}
}

foundation::Result<MeshRepairResult, foundation::Error> repairTriangleMesh(
    std::span<const foundation::Vec3> positions,
    std::span<const foundation::Vec3> source_normals,
    std::span<const std::uint32_t> source_indices,
    std::span<const TriangleGroup> source_groups) {
    using Result=foundation::Result<MeshRepairResult,foundation::Error>;
    if (positions.empty() || source_normals.size()!=positions.size() ||
        source_indices.empty() || source_indices.size()%3U!=0U) {
        return Result::failure({foundation::ErrorCode::InvalidArgument,
                                "invalid indexed triangle streams"});
    }
    for (const auto p:positions) if(!finite(p))
        return Result::failure({foundation::ErrorCode::InvalidArgument,
                                "non-finite mesh position"});
    for (const auto i:source_indices) if(i>=positions.size())
        return Result::failure({foundation::ErrorCode::OutOfRange,
                                "mesh index out of range"});

    std::vector<TriangleGroup> groups;
    if (source_groups.empty()) {
        groups.push_back({0U,static_cast<std::uint32_t>(source_indices.size()),0U});
    } else {
        std::uint32_t cursor=0U;
        for(const auto& group:source_groups) {
            if(group.start!=cursor || group.count==0U || group.count%3U!=0U ||
               static_cast<std::size_t>(group.start)+group.count>source_indices.size())
                return Result::failure({foundation::ErrorCode::InvalidArgument,
                                        "mesh groups do not partition the index stream"});
            groups.push_back(group);
            cursor += group.count;
        }
        if(cursor!=source_indices.size())
            return Result::failure({foundation::ErrorCode::InvalidArgument,
                                    "mesh groups do not cover all indices"});
    }

    MeshRepairResult out;
    out.normals.assign(source_normals.begin(),source_normals.end());
    out.indices.reserve(source_indices.size());
    std::vector<foundation::Vec3> fallback(positions.size());

    for(const auto& group:groups) {
        const auto start=static_cast<std::uint32_t>(out.indices.size());
        for(std::uint32_t offset=group.start; offset<group.start+group.count; offset+=3U) {
            const auto i0=source_indices[offset],i1=source_indices[offset+1U],i2=source_indices[offset+2U];
            const auto face=cross(sub(positions[i1],positions[i0]),sub(positions[i2],positions[i0]));
            const float area2=dot(face,face);
            if(!std::isfinite(area2) ||
               area2<=kTriangleAreaEpsilon*kTriangleAreaEpsilon) {
                ++out.stats.removed_degenerate_triangles;
                continue;
            }
            out.indices.insert(out.indices.end(),{i0,i1,i2});
            for(const auto index:{i0,i1,i2})
                if(dot(fallback[index],fallback[index])<=kNormalLengthEpsilon*kNormalLengthEpsilon)
                    fallback[index]=face;
        }
        const auto count=static_cast<std::uint32_t>(out.indices.size())-start;
        if(count!=0U) out.groups.push_back({start,count,group.material});
    }
    if(out.indices.empty())
        return Result::failure({foundation::ErrorCode::InvalidState,
                                "triangle repair removed the complete mesh"});

    for(std::size_t index=0; index<out.normals.size(); ++index) {
        const float length2=dot(out.normals[index],out.normals[index]);
        if(!finite(out.normals[index]) ||
           !(length2>kNormalLengthEpsilon*kNormalLengthEpsilon)) {
            out.normals[index]=normalized(fallback[index]);
            ++out.stats.repaired_normals;
        }
    }
    for(std::size_t offset=0; offset<out.indices.size(); offset+=3U) {
        const auto i0=out.indices[offset],i1=out.indices[offset+1U],i2=out.indices[offset+2U];
        const auto face=cross(sub(positions[i1],positions[i0]),sub(positions[i2],positions[i0]));
        const foundation::Vec3 average{
            (out.normals[i0].x+out.normals[i1].x+out.normals[i2].x)/3.0F,
            (out.normals[i0].y+out.normals[i1].y+out.normals[i2].y)/3.0F,
            (out.normals[i0].z+out.normals[i1].z+out.normals[i2].z)/3.0F};
        if(dot(face,average)<-1.0e-6F) {
            std::swap(out.indices[offset+1U],out.indices[offset+2U]);
            ++out.stats.flipped_triangles;
        }
    }
    return Result::success(std::move(out));
}

foundation::Result<MeshRepairMeshResult, foundation::Error> repairMesh(const MeshData& source) {
    using Result=foundation::Result<MeshRepairMeshResult,foundation::Error>;
    if(!source.valid()||source.vertices.empty())return Result::failure({foundation::ErrorCode::InvalidArgument,"mesh repair requires a valid indexed vertex mesh"});
    MeshData normalized=source;if(normalized.positions.empty())normalized.rebuildStreams();
    std::vector<TriangleGroup> groups;groups.reserve(normalized.submeshes.size());for(const auto& range:normalized.submeshes)groups.push_back({range.first_index,range.index_count,static_cast<std::uint16_t>(range.material_index)});
    auto repaired=repairTriangleMesh(normalized.positions,normalized.normals,normalized.indices,groups);if(!repaired)return Result::failure(repaired.error()); MeshRepairMeshResult result{};result.mesh=std::move(normalized);result.mesh.indices=std::move(repaired.value().indices);result.mesh.normals=std::move(repaired.value().normals);for(std::size_t i=0;i<result.mesh.vertices.size();++i)result.mesh.vertices[i].normal=result.mesh.normals[i];result.mesh.submeshes.clear();for(const auto group:repaired.value().groups)result.mesh.submeshes.push_back({group.start,group.count,group.material});result.mesh.rebuildStreams();result.stats=repaired.value().stats;return Result::success(std::move(result));
}

} // namespace genomes::geometry
