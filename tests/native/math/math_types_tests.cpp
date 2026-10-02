#include <genomes/math/Bounds.hpp>
#include <genomes/math/Transform.hpp>
#include <cassert>
#include <cmath>
#include <limits>

int main() {
    using namespace genomes::math;
    static_assert(sizeof(Vec2)==8 && sizeof(Vec3)==12 && sizeof(Vec4)==16);
    const Vec2 divided=Vec2{8.0F,-6.0F}/2.0F;
    assert(divided.x==4.0F && divided.y==-3.0F);
    Vec2 normalized_vec{3.0F,4.0F};
    assert(normalize(normalized_vec));
    assert(std::fabs(normalized_vec.x-0.6F)<1e-6F && std::fabs(normalized_vec.y-0.8F)<1e-6F);
    Vec2 zero2{}; assert(!normalize(zero2));
    Vec2 nan2{std::numeric_limits<float>::quiet_NaN(),1.0F}; assert(!normalize(nan2));
    Vec2 inf2{std::numeric_limits<float>::infinity(),1.0F}; assert(!normalize(inf2));
    Vec3 x{1,0,0}; Vec3 y{0,1,0}; assert(cross(x,y).z==1); assert(dot(x,y)==0);
    Vec3 zero{}; assert(!normalize(zero));
    Quat q{}; assert(Quat::axisAngle({0,1,0},3.14159265358979323846F,q));
    const Vec3 turned=q.rotate(x); assert(std::fabs(turned.x+1)<1e-4F);
    Quat scaled{0.0F, 2.0F, 0.0F, 0.0F};
    const Vec3 scaled_turn=scaled.rotate(x); assert(std::fabs(scaled_turn.x+1)<1e-4F);
    const Quat scaled_inverse=scaled.inverse();
    const Quat identity=scaled*scaled_inverse;
    assert(std::fabs(identity.w-1.0F)<1e-4F && std::fabs(identity.x)<1e-4F);
    Quat invalid_angle{}; assert(!Quat::axisAngle({0,1,0}, NAN, invalid_angle));
    const Mat4 view=lookAtRH({0,0,2},{0,0,0},{0,1,0});
    const Mat4 projection=perspectiveD3D(1.0F,1.0F,0.1F,100.0F);
    assert(view.finite() && projection.finite() && view.inverse().has_value());
    const Vec3 cameraSpace=transformPoint(view,{0,0,0}); assert(cameraSpace.z<0);
    const Vec3 tilted_eye{3.0F,5.0F,7.0F};
    const Vec3 tilted_target{-2.0F,1.0F,0.5F};
    const Mat4 tilted=lookAtRH(tilted_eye,tilted_target,{0,1,0});
    assert(length(transformPoint(tilted,tilted_eye))<1e-5F);
    const Vec3 tilted_target_view=transformPoint(tilted,tilted_target);
    assert(std::fabs(tilted_target_view.x)<1e-5F && std::fabs(tilted_target_view.y)<1e-5F &&
           tilted_target_view.z<0.0F);
    assert(transformVector(tilted,{1,0,0}).x>0.0F);
    assert(transformVector(tilted,{0,1,0}).y>0.0F);
    Aabb bounds; bounds.include({-1,-2,-3}); bounds.include({1,2,3});
    assert(!bounds.empty && bounds.center().x==0 && bounds.extent().y==2);
    const Aabb moved=bounds.transformed(Transform{{2,0,0},{},{1,1,1}}.matrix());
    assert(moved.center().x==2);
    return 0;
}
