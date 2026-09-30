#include <genomes/geometry/PrimitiveBuilder.hpp>

#include <array>
#include <cmath>
#include <utility>

namespace genomes::geometry {

foundation::Result<MeshData, foundation::Error> makeBoxResult(const BoxSpec& spec) {
    MeshData mesh;
    if (!std::isfinite(spec.size.x) || !std::isfinite(spec.size.y) ||
        !std::isfinite(spec.size.z) || spec.size.x <= 0.0F ||
        spec.size.y <= 0.0F || spec.size.z <= 0.0F) {
        return foundation::Result<MeshData, foundation::Error>::failure(
            {foundation::ErrorCode::InvalidArgument, "box dimensions must be finite and positive"});
    }
    const foundation::Vec3 h{spec.size.x*0.5F, spec.size.y*0.5F, spec.size.z*0.5F};
    const std::array<foundation::Vec3,8> corners{{
        {-h.x,-h.y,-h.z},{h.x,-h.y,-h.z},{h.x,h.y,-h.z},{-h.x,h.y,-h.z},
        {-h.x,-h.y,h.z},{h.x,-h.y,h.z},{h.x,h.y,h.z},{-h.x,h.y,h.z}}};
    constexpr std::array<std::array<std::uint32_t,4>,6> faces{{
        {{0,1,2,3}},{{5,4,7,6}},{{4,0,3,7}},
        {{1,5,6,2}},{{3,2,6,7}},{{4,5,1,0}}}};
    constexpr std::array<foundation::Vec3,6> normals{{
        {0,0,-1},{0,0,1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}}};
    constexpr std::array<foundation::Vec2,4> uvs{{
        {0,0},{1,0},{1,1},{0,1}}};
    mesh.vertices.reserve(24U);
    mesh.indices.reserve(36U);
    for (std::size_t face=0; face<faces.size(); ++face) {
        const auto base=static_cast<std::uint32_t>(mesh.vertices.size());
        for (std::size_t corner=0; corner<4U; ++corner)
            mesh.vertices.push_back({corners[faces[face][corner]],normals[face],uvs[corner]});
        mesh.indices.insert(mesh.indices.end(),
            {base,base+1U,base+2U,base,base+2U,base+3U});
    }
    mesh.rebuildStreams();
    mesh.submeshes.push_back({0U, static_cast<std::uint32_t>(mesh.indices.size()), 0U});
    return foundation::Result<MeshData, foundation::Error>::success(std::move(mesh));
}

MeshData makeBox(const BoxSpec& spec) {
    auto result = makeBoxResult(spec);
    return result ? std::move(result).value() : MeshData{};
}

} // namespace genomes::geometry
