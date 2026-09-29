#pragma once

#include <genomes/foundation/Types.hpp>
#include <genomes/geometry/MeshData.hpp>

namespace genomes::geometry {

struct BoxSpec final {
    foundation::Vec3 size{1.0F, 1.0F, 1.0F};
};

[[nodiscard]] MeshData makeBox(const BoxSpec& spec = {});

} // namespace genomes::geometry
