#include <genomes/geometry/PolygonOps.hpp>
#include <cassert>
#include <cmath>
#include <vector>

int main() {
    using namespace genomes::geometry;
    Polygon2 square{{{0,0},{4,0},{4,4},{0,4}},{{{1,1},{1,2},{2,2},{2,1}}}};
    assert(validatePolygon(square));
    const auto normalized=normalizeWinding(square);
    assert(normalized && signedArea(normalized.value().outer)>0);
    assert(contains(square,{0.5F,0.5F}) && !contains(square,{1.5F,1.5F}));
    const auto triangles=triangulate(square);
    assert(triangles && triangles.value().size()%3U==0U);
    const std::vector<math::Vec2> flattened{{0,0},{4,0},{4,4},{0,4},{1,1},{2,1},{2,2},{1,2}};
    float triangulated_area=0.0F;
    for(std::size_t i=0;i<triangles.value().size();i+=3U){
        const auto a=flattened[triangles.value()[i]],b=flattened[triangles.value()[i+1U]],c=flattened[triangles.value()[i+2U]];
        triangulated_area+=std::fabs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))*0.5F;
    }
    assert(std::fabs(triangulated_area-15.0F)<1.0e-4F);
    Polygon2 crossing_hole{{{0,0},{4,0},{4,4},{0,4}},{{{-1,1},{2,1},{2,2},{-1,2}}}};
    assert(!validatePolygon(crossing_hole));
    Polygon2 overlapping_holes{{{0,0},{4,0},{4,4},{0,4}},
                               {{{1,1},{3,1},{3,3},{1,3}}, {{2,2},{3.5F,2},{3.5F,3.5F},{2,3.5F}}}};
    assert(!validatePolygon(overlapping_holes));
    Polygon2 invalid{{{0,0},{1,1},{2,2}}, {}, 1e-6F};
    assert(!validatePolygon(invalid));
    return 0;
}
