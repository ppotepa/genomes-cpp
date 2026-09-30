#pragma once

#include <genomes/assets/ImportedScene.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <cstddef>
#include <filesystem>

namespace genomes::assets {
struct ImportLimits final { std::size_t max_meshes{10000}; std::size_t max_vertices{10'000'000}; std::size_t max_indices{30'000'000}; std::size_t max_nodes{10000}; };
[[nodiscard]] foundation::Result<ImportedScene, foundation::Error> importStaticGltf(const std::filesystem::path&, ImportLimits limits = {});
} // namespace genomes::assets
