#pragma once

#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/geometry/Polygon2.hpp>

#include <span>
#include <vector>

namespace genomes::geometry {

[[nodiscard]] float signedArea(std::span<const math::Vec2> ring) noexcept;
[[nodiscard]] foundation::Result<void, foundation::Error>
validatePolygon(const Polygon2&);
[[nodiscard]] foundation::Result<Polygon2, foundation::Error>
normalizeWinding(const Polygon2&, PolygonWinding outer_winding = PolygonWinding::CounterClockwise);
[[nodiscard]] bool contains(const Polygon2&, math::Vec2 point) noexcept;
[[nodiscard]] foundation::Result<std::vector<std::uint32_t>, foundation::Error>
triangulate(const Polygon2&);

} // namespace genomes::geometry
