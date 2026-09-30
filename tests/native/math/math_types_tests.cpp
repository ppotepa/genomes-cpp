#include <genomes/math/Bounds.hpp>
#include <genomes/math/Transform.hpp>
#include <cassert>
#include <cmath>

int main() {
    using namespace genomes::math;
    static_assert(sizeof(Vec2)==8 && sizeof(Vec3)==12 && sizeof(Vec4)==16);
    Vec3 x{1,0,0}; Vec3 y{0,1,0}; assert(cross(x,y).z==1); assert(dot(x,y)==0);
    Vec3 zero{}; assert(!normalize(zero));
    Quat q{}; assert(Quat::axisAngle({0,1,0},3.14159265358979323846F,q));
    const Vec3 turned=q.rotate(x); assert(std::fabs(turned.x+1)<1e-4F);
    const Mat4 view=lookAtRH({0,0,2},{0,0,0},{0,1,0});
    const Mat4 projection=perspectiveD3D(1.0F,1.0F,0.1F,100.0F);
    assert(view.finite() && projection.finite() && view.inverse().has_value());
    const Vec3 cameraSpace=transformPoint(view,{0,0,0}); assert(cameraSpace.z<0);
    Aabb bounds; bounds.include({-1,-2,-3}); bounds.include({1,2,3});
    assert(!bounds.empty && bounds.center().x==0 && bounds.extent().y==2);
    const Aabb moved=bounds.transformed(Transform{{2,0,0},{},{1,1,1}}.matrix());
    assert(moved.center().x==2);
    return 0;
}
