#pragma once

#include <genomes/math/Vec.hpp>

#include <cstdint>
#include <vector>

namespace genomes::geometry {

enum class PolygonWinding : std::uint8_t { Clockwise, CounterClockwise };
struct Polygon2 final {
    std::vector<math::Vec2> outer;
    std::vector<std::vector<math::Vec2>> holes;
    float epsilon{1.0e-6F};
};

} // namespace genomes::geometry
