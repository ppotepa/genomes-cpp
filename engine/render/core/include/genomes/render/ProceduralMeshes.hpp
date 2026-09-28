#pragma once

#include <genomes/render/RenderTypes.hpp>

#include <memory>

namespace genomes::render::procedural {

// Creates an immutable, centered box prototype. Instance transforms remain
// in RenderInstance, so this helper is suitable for menu previews as well as
// later world-prototype catalogs.
[[nodiscard]] std::shared_ptr<const RenderMesh> make_box(
    foundation::StableId mesh_id,
    foundation::Vec3 half_extents,
    foundation::Color color = {1.0F, 1.0F, 1.0F, 1.0F});

} // namespace genomes::render::procedural
