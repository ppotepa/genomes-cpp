#include <genomes/geometry/ParametricSurface.hpp>
#include <genomes/geometry/PrimitiveBuilder.hpp>
#include <genomes/geometry/ProfileSurface.hpp>
#include <genomes/geometry/MeshValidation.hpp>
#include <cassert>
#include <cmath>

int main() {
    using namespace genomes::geometry;
    const auto box=makeBoxResult({{1,2,3}}); assert(box && box.value().valid());
    assert(validateMesh(box.value()).valid());
    const auto plane_mesh=makePlaneResult({2.0F,3.0F,2U,3U});
    assert(plane_mesh && plane_mesh.value().valid() && validateMesh(plane_mesh.value()).valid());
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
    ParametricSurfaceSpec plane_surface{}; plane_surface.u_segments=2; plane_surface.v_segments=2; plane_surface.position=[](float u,float v){return genomes::foundation::Vec3{u,v,0};};
    const auto tessellated=tessellateParametric(plane_surface); assert(tessellated && tessellated.value().indices.size()==24U && validateMesh(tessellated.value()).valid());
    plane_surface.position=[](float, float){return genomes::foundation::Vec3{NAN, 0.0F, 0.0F};};
    assert(!tessellateParametric(plane_surface));
    return 0;
}
