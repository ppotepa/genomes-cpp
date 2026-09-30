#include <genomes/geometry/PolygonOps.hpp>
#include <cassert>

int main() {
    using namespace genomes::geometry;
    Polygon2 square{{{0,0},{4,0},{4,4},{0,4}},{{{1,1},{1,2},{2,2},{2,1}}}};
    assert(validatePolygon(square));
    const auto normalized=normalizeWinding(square);
    assert(normalized && signedArea(normalized.value().outer)>0);
    assert(contains(square,{0.5F,0.5F}) && !contains(square,{1.5F,1.5F}));
    const auto triangles=triangulate(square);
    assert(triangles && triangles.value().size()%3U==0U);
    Polygon2 invalid{{{0,0},{1,1},{2,2}}, {}, 1e-6F};
    assert(!validatePolygon(invalid));
    return 0;
}
