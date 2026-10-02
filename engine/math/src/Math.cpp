#include <genomes/math/Bounds.hpp>
#include <genomes/math/Transform.hpp>
#include <algorithm>
#include <cmath>

namespace genomes::math {
bool Mat4::finite() const noexcept { for(float v:m) if(!std::isfinite(v)) return false; return true; }
Mat4 operator*(const Mat4&a,const Mat4&b) noexcept { Mat4 r{}; for(int c=0;c<4;++c)for(int row=0;row<4;++row)for(int k=0;k<4;++k)r(row,c)+=a(row,k)*b(k,c); return r; }
Vec4 operator*(const Mat4&a,Vec4 v) noexcept { return {a(0,0)*v.x+a(0,1)*v.y+a(0,2)*v.z+a(0,3)*v.w,a(1,0)*v.x+a(1,1)*v.y+a(1,2)*v.z+a(1,3)*v.w,a(2,0)*v.x+a(2,1)*v.y+a(2,2)*v.z+a(2,3)*v.w,a(3,0)*v.x+a(3,1)*v.y+a(3,2)*v.z+a(3,3)*v.w}; }
Vec3 transformPoint(const Mat4&m,Vec3 p) noexcept { const Vec4 v=m*Vec4{p.x,p.y,p.z,1}; return v.w!=0?Vec3{v.x/v.w,v.y/v.w,v.z/v.w}:Vec3{v.x,v.y,v.z}; }
Vec3 transformVector(const Mat4&m,Vec3 v) noexcept { const Vec4 r=m*Vec4{v.x,v.y,v.z,0}; return {r.x,r.y,r.z}; }
std::optional<Mat4> Mat4::inverse() const noexcept { Mat4 a=*this,r=identity(); for(int i=0;i<4;++i){int pivot=i; for(int j=i+1;j<4;++j)if(std::fabs(a(j,i))>std::fabs(a(pivot,i)))pivot=j; if(std::fabs(a(pivot,i))<1e-8F)return std::nullopt; if(pivot!=i)for(int c=0;c<4;++c){std::swap(a(i,c),a(pivot,c));std::swap(r(i,c),r(pivot,c));} const float d=a(i,i); for(int c=0;c<4;++c){a(i,c)/=d;r(i,c)/=d;} for(int row=0;row<4;++row)if(row!=i){const float f=a(row,i);for(int c=0;c<4;++c){a(row,c)-=f*a(i,c);r(row,c)-=f*r(i,c);}}} return r; }
std::optional<Mat4> Mat4::rigidInverse() const noexcept { if(std::fabs(m[3])>1e-6F||std::fabs(m[7])>1e-6F||std::fabs(m[11])>1e-6F||std::fabs(m[15]-1)>1e-6F)return std::nullopt; Mat4 r=identity(); for(int row=0;row<3;++row)for(int c=0;c<3;++c)r(row,c)=(*this)(c,row); const Vec3 t{(*this)(0,3),(*this)(1,3),(*this)(2,3)}; const Vec3 nt=transformVector(r,-t); r(0,3)=nt.x;r(1,3)=nt.y;r(2,3)=nt.z; return r; }
Mat4 lookAtRH(Vec3 eye,Vec3 target,Vec3 up) noexcept { Vec3 z=normalized(eye-target),x=normalized(cross(up,z)),y=cross(z,x); Mat4 r=Mat4::identity(); r(0,0)=x.x;r(0,1)=x.y;r(0,2)=x.z;r(1,0)=y.x;r(1,1)=y.y;r(1,2)=y.z;r(2,0)=z.x;r(2,1)=z.y;r(2,2)=z.z;r(0,3)=-dot(x,eye);r(1,3)=-dot(y,eye);r(2,3)=-dot(z,eye);return r; }
Mat4 perspectiveD3D(float fov,float aspect,float n,float f) noexcept { Mat4 r{}; const float q=1/std::tan(fov*0.5F); r(0,0)=q/aspect;r(1,1)=q;r(2,2)=f/(n-f);r(2,3)=(n*f)/(n-f);r(3,2)=-1;return r; }
Mat4 Transform::matrix() const noexcept { Mat4 r=Mat4::identity(); const Quat q=rotation; const float xx=q.x*q.x,yy=q.y*q.y,zz=q.z*q.z,xy=q.x*q.y,xz=q.x*q.z,yz=q.y*q.z,wx=q.w*q.x,wy=q.w*q.y,wz=q.w*q.z; r(0,0)=(1-2*(yy+zz))*scale.x;r(1,0)=2*(xy+wz)*scale.x;r(2,0)=2*(xz-wy)*scale.x;r(0,1)=2*(xy-wz)*scale.y;r(1,1)=(1-2*(xx+zz))*scale.y;r(2,1)=2*(yz+wx)*scale.y;r(0,2)=2*(xz+wy)*scale.z;r(1,2)=2*(yz-wx)*scale.z;r(2,2)=(1-2*(xx+yy))*scale.z;r(0,3)=translation.x;r(1,3)=translation.y;r(2,3)=translation.z;return r; }
std::optional<Transform> Transform::inverse() const noexcept { if(!valid())return std::nullopt; auto m=matrix().inverse(); if(!m)return std::nullopt; Transform r; r.translation=transformPoint(*m,{0,0,0}); r.scale={1/scale.x,1/scale.y,1/scale.z}; r.rotation=rotation.inverse(); return r; }
Transform operator*(const Transform&a,const Transform&b) noexcept { Transform r; r.translation=a.translation+a.rotation.rotate({a.scale.x*b.translation.x,a.scale.y*b.translation.y,a.scale.z*b.translation.z}); r.rotation=a.rotation*b.rotation;r.scale={a.scale.x*b.scale.x,a.scale.y*b.scale.y,a.scale.z*b.scale.z};return r; }
void Aabb::include(Vec3 p) noexcept { if(empty){min=max=p;empty=false;}else{min.x=std::min(min.x,p.x);min.y=std::min(min.y,p.y);min.z=std::min(min.z,p.z);max.x=std::max(max.x,p.x);max.y=std::max(max.y,p.y);max.z=std::max(max.z,p.z);} }
Aabb Aabb::transformed(const Mat4&m) const noexcept { Aabb r; if(empty)return r; for(int i=0;i<8;++i)r.include(transformPoint(m,{(i&1)?max.x:min.x,(i&2)?max.y:min.y,(i&4)?max.z:min.z}));return r; }
bool Frustum::contains(Vec3 p) const noexcept { for(const auto& plane:planes)if(plane.signedDistance(p)<0)return false;return true; }
}
