#pragma once

#include <genomes/geometry/MeshData.hpp>
#include <genomes/math/Transform.hpp>

#include <string>
#include <vector>

namespace genomes::assets {
struct MaterialDescriptor final { math::Vec4 base_color{1,1,1,1}; float metallic{0}; float roughness{1}; };
struct ImportedNode final { std::string name; std::size_t mesh_index{0}; math::Transform local{}; math::Transform world{}; };
struct ImportedScene final { std::vector<geometry::MeshData> meshes; std::vector<MaterialDescriptor> materials; std::vector<ImportedNode> nodes; std::vector<std::string> diagnostics; };
} // namespace genomes::assets
