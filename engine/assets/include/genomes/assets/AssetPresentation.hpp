#pragma once

#include <genomes/assets/ImportedScene.hpp>
#include <genomes/math/Bounds.hpp>

#include <cstdint>
#include <vector>

namespace genomes::assets {
struct PresentedAsset final {
    std::vector<geometry::MeshData> meshes;
    std::vector<MaterialDescriptor> materials;
    math::Aabb bounds{};
    std::uint64_t revision{0};
};
[[nodiscard]] PresentedAsset present(const ImportedScene&);
} // namespace genomes::assets
