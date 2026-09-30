#include <genomes/geometry/ParametricSurface.hpp>
#include <genomes/geometry/PrimitiveBuilder.hpp>
#include <genomes/geometry/ProfileSurface.hpp>
#include <cassert>

int main() {
    using namespace genomes::geometry;
    const auto box=makeBoxResult({{1,2,3}}); assert(box && box.value().valid());
    SweepSpec sweep{{{0,0,0},{0,1,0},{0,2,0}},{{0.5F,0.5F},{0.5F,0.5F},{0.5F,0.5F}},8U,false,false};
    const auto swept=sweepProfile(sweep); assert(swept && swept.value().valid());
    ParametricSurfaceSpec plane{}; plane.u_segments=2; plane.v_segments=2; plane.position=[](float u,float v){return genomes::foundation::Vec3{u,v,0};};
    const auto tessellated=tessellateParametric(plane); assert(tessellated && tessellated.value().indices.size()==24U);
    return 0;
}
