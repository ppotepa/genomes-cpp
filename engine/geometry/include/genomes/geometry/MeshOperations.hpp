#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/geometry/MeshData.hpp>
#include <genomes/math/Transform.hpp>

#include <span>

namespace genomes::geometry {

[[nodiscard]] foundation::Result<MeshData, foundation::Error>
combine(std::span<const MeshData> meshes);
[[nodiscard]] foundation::Result<MeshData, foundation::Error>
transform(const MeshData&, const math::Transform&);
[[nodiscard]] foundation::Result<MeshData, foundation::Error>
recalculateNormals(const MeshData&);
[[nodiscard]] foundation::Result<MeshData, foundation::Error>
convertIndexFormat(const MeshData&, IndexFormat);

} // namespace genomes::geometry
