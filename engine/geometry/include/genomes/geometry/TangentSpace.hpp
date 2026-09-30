#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/geometry/MeshData.hpp>

namespace genomes::geometry {
[[nodiscard]] foundation::Result<MeshData, foundation::Error>
generateTangents(const MeshData&);
} // namespace genomes::geometry
