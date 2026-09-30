#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/geometry/MeshData.hpp>

namespace genomes::geometry {

struct BoxSpec final {
    foundation::Vec3 size{1.0F, 1.0F, 1.0F};
};

[[nodiscard]] MeshData makeBox(const BoxSpec& spec = {});
[[nodiscard]] foundation::Result<MeshData, foundation::Error>
makeBoxResult(const BoxSpec& spec = {});

} // namespace genomes::geometry
