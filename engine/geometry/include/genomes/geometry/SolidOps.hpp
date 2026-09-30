#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/geometry/MeshData.hpp>

namespace genomes::geometry {

enum class SolidBoolean : std::uint8_t { Union, Difference, Intersection };
[[nodiscard]] foundation::Result<MeshData, foundation::Error>
booleanSolid(const MeshData&, const MeshData&, SolidBoolean);

} // namespace genomes::geometry
