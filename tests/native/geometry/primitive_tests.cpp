#include <genomes/geometry/ParametricSurface.hpp>
#include <genomes/geometry/PrimitiveBuilder.hpp>
#include <genomes/geometry/ProfileSurface.hpp>
#include <genomes/geometry/MeshValidation.hpp>
#include <cassert>

int main() {
    using namespace genomes::geometry;
    const auto box=makeBoxResult({{1,2,3}}); assert(box && box.value().valid());
    assert(validateMesh(box.value()).valid());
    const auto plane=makePlaneResult({2.0F,3.0F,2U,3U});
    assert(plane && plane.value().valid() && validateMesh(plane.value()).valid());
    const auto grid=makeGridResult({2.0F,3.0F,3U,2U});
    assert(grid && grid.value().valid() && validateMesh(grid.value()).valid());
    const auto sphere=makeUvSphereResult({1.0F,12U,6U});
    assert(sphere && sphere.value().valid() && validateMesh(sphere.value()).valid());
    const auto cylinder=makeCylinderResult({0.5F,2.0F,12U,true});
    assert(cylinder && cylinder.value().valid() && validateMesh(cylinder.value()).valid());
    const auto cone=makeConeResult({0.5F,2.0F,12U,true});
    assert(cone && cone.value().valid() && validateMesh(cone.value()).valid());
    const auto capsule=makeCapsuleResult({0.25F,1.0F,12U,4U,2U});
    assert(capsule && capsule.value().valid() && validateMesh(capsule.value()).valid());
    assert(!makePlaneResult({1.0F, 1.0F, 0U, 1U}));
    assert(!makeUvSphereResult({1.0F, 2U, 2U}));
    assert(!makeCylinderResult({1.0F, 1.0F, 2U, true}));
    SweepSpec sweep{{{0,0,0},{0,1,0},{0,2,0}},{{0.5F,0.5F},{0.5F,0.5F},{0.5F,0.5F}},8U,false,false};
    const auto swept=sweepProfile(sweep); assert(swept && swept.value().valid());
    sweep.cap = true;
    const auto capped_sweep=sweepProfile(sweep);
    assert(capped_sweep && capped_sweep.value().valid() &&
           validateMesh(capped_sweep.value()).valid());
    SweepSpec degenerate_sweep{{{0,0,0},{0,0,0}},{{0.5F,0.5F},{0.5F,0.5F}},8U,false,false};
    assert(!sweepProfile(degenerate_sweep));
    ParametricSurfaceSpec plane{}; plane.u_segments=2; plane.v_segments=2; plane.position=[](float u,float v){return genomes::foundation::Vec3{u,v,0};};
    const auto tessellated=tessellateParametric(plane); assert(tessellated && tessellated.value().indices.size()==24U);
    return 0;
}
