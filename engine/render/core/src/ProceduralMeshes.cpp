#include <genomes/render/ProceduralMeshes.hpp>

#include <array>
#include <cstdint>

namespace genomes::render::procedural {

std::shared_ptr<const RenderMesh> make_box(foundation::StableId mesh_id,
                                           foundation::Vec3 half_extents,
                                           foundation::Color color) {
    auto mesh = std::make_shared<RenderMesh>();
    mesh->mesh_id = mesh_id;
    mesh->revision = 1;
    mesh->vertices.reserve(24);
    mesh->indices.reserve(36);

    const std::array<foundation::Vec3, 8> corners{{
        {-half_extents.x, -half_extents.y, -half_extents.z},
        {half_extents.x, -half_extents.y, -half_extents.z},
        {half_extents.x, half_extents.y, -half_extents.z},
        {-half_extents.x, half_extents.y, -half_extents.z},
        {-half_extents.x, -half_extents.y, half_extents.z},
        {half_extents.x, -half_extents.y, half_extents.z},
        {half_extents.x, half_extents.y, half_extents.z},
        {-half_extents.x, half_extents.y, half_extents.z},
    }};
    constexpr std::array<std::array<std::uint32_t, 4>, 6> faces{{
        {{0, 1, 2, 3}}, {{5, 4, 7, 6}}, {{4, 0, 3, 7}},
        {{1, 5, 6, 2}}, {{3, 2, 6, 7}}, {{4, 5, 1, 0}},
    }};
    constexpr std::array<foundation::Vec3, 6> normals{{
        {0.0F, 0.0F, -1.0F}, {0.0F, 0.0F, 1.0F}, { -1.0F, 0.0F, 0.0F},
        {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}, {0.0F, -1.0F, 0.0F},
    }};
    constexpr std::array<foundation::Vec2, 4> uv{{
        {0.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 1.0F},
    }};

    for (std::size_t face = 0; face < faces.size(); ++face) {
        const std::uint32_t base = static_cast<std::uint32_t>(mesh->vertices.size());
        for (std::size_t corner = 0; corner < faces[face].size(); ++corner) {
            mesh->vertices.push_back(
                {corners[faces[face][corner]], normals[face], uv[corner], color});
        }
        mesh->indices.insert(mesh->indices.end(),
                             {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    return mesh;
}

} // namespace genomes::render::procedural
