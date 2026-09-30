#pragma once

#include <genomes/render/RenderTypes.hpp>
#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>

#include <memory>

namespace genomes::render::procedural {

using MeshResult = foundation::Result<std::shared_ptr<const RenderMesh>, foundation::Error>;

[[nodiscard]] foundation::Result<void, foundation::Error> append_box(
    RenderMesh& destination,
    foundation::Vec3 center,
    foundation::Vec3 extent,
    foundation::Color color = {1.0F, 1.0F, 1.0F, 1.0F},
    float rotation_y = 0.0F);

// Typed construction path. Geometry validation failures remain observable to
// callers; the compatibility helper below is only for legacy scene call-sites.
[[nodiscard]] MeshResult make_box_result(
    foundation::StableId mesh_id,
    foundation::Vec3 half_extents,
    foundation::Color color = {1.0F, 1.0F, 1.0F, 1.0F});

// Creates an immutable, centered box prototype. Instance transforms remain
// in RenderInstance, so this helper is suitable for menu previews as well as
// later world-prototype catalogs.
[[nodiscard]] std::shared_ptr<const RenderMesh> make_box(
    foundation::StableId mesh_id,
    foundation::Vec3 half_extents,
    foundation::Color color = {1.0F, 1.0F, 1.0F, 1.0F});

} // namespace genomes::render::procedural
