// TEST-ONLY mathematical compatibility layer. Not Three.js and not a WebGL renderer.
const noop=()=>{};
class Vector3 {
 constructor(x=0,y=0,z=0){this.x=x;this.y=y;this.z=z;}
 set(x,y,z){this.x=x;this.y=y;this.z=z;return this;}setScalar(s){return this.set(s,s,s);}
 copy(v){return this.set(v.x,v.y,v.z);}clone(){return new Vector3(this.x,this.y,this.z);}
 fromArray(a,o=0){return this.set(a[o],a[o+1],a[o+2]);}toArray(a=[],o=0){a[o]=this.x;a[o+1]=this.y;a[o+2]=this.z;return a;}
 add(v){this.x+=v.x;this.y+=v.y;this.z+=v.z;return this;}sub(v){this.x-=v.x;this.y-=v.y;this.z-=v.z;return this;}
 addVectors(a,b){return this.copy(a).add(b);}subVectors(a,b){return this.copy(a).sub(b);}addScaledVector(v,s){this.x+=v.x*s;this.y+=v.y*s;this.z+=v.z*s;return this;}
 multiplyScalar(s){this.x*=s;this.y*=s;this.z*=s;return this;}divideScalar(s){return this.multiplyScalar(1/s);}
 dot(v){return this.x*v.x+this.y*v.y+this.z*v.z;}lengthSq(){return this.dot(this);}length(){return Math.sqrt(this.lengthSq());}normalize(){return this.divideScalar(this.length()||1);}
 cross(v){return this.crossVectors(this.clone(),v);}crossVectors(a,b){return this.set(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
 distanceToSquared(v){const x=this.x-v.x,y=this.y-v.y,z=this.z-v.z;return x*x+y*y+z*z;}distanceTo(v){return Math.sqrt(this.distanceToSquared(v));}lerp(v,t){this.x+=(v.x-this.x)*t;this.y+=(v.y-this.y)*t;this.z+=(v.z-this.z)*t;return this;}
 applyQuaternion(q){const x=this.x,y=this.y,z=this.z,qx=q.x,qy=q.y,qz=q.z,qw=q.w,ix=qw*x+qy*z-qz*y,iy=qw*y+qz*x-qx*z,iz=qw*z+qx*y-qy*x,iw=-qx*x-qy*y-qz*z;return this.set(ix*qw+iw*-qx+iy*-qz-iz*-qy,iy*qw+iw*-qy+iz*-qx-ix*-qz,iz*qw+iw*-qz+ix*-qy-iy*-qx);}
 applyMatrix4(m){const e=m.elements,x=this.x,y=this.y,z=this.z,w=1/(e[3]*x+e[7]*y+e[11]*z+e[15]);return this.set((e[0]*x+e[4]*y+e[8]*z+e[12])*w,(e[1]*x+e[5]*y+e[9]*z+e[13])*w,(e[2]*x+e[6]*y+e[10]*z+e[14])*w);}
 transformDirection(m){const e=m.elements,x=this.x,y=this.y,z=this.z;return this.set(e[0]*x+e[4]*y+e[8]*z,e[1]*x+e[5]*y+e[9]*z,e[2]*x+e[6]*y+e[10]*z).normalize();}
}
class Vector2{constructor(x=0,y=0){this.x=x;this.y=y;}}
class Quaternion{
 constructor(x=0,y=0,z=0,w=1){this.x=x;this.y=y;this.z=z;this.w=w;this._onChange=noop;}
 set(x,y,z,w){this.x=x;this.y=y;this.z=z;this.w=w;this._onChange();return this;}identity(){return this.set(0,0,0,1);}clone(){return new Quaternion(this.x,this.y,this.z,this.w);}copy(q){return this.set(q.x,q.y,q.z,q.w);}
 fromArray(a,o=0){return this.set(a[o],a[o+1],a[o+2],a[o+3]);}toArray(a=[],o=0){a[o]=this.x;a[o+1]=this.y;a[o+2]=this.z;a[o+3]=this.w;return a;}
 normalize(){const l=Math.hypot(this.x,this.y,this.z,this.w);return l?this.set(this.x/l,this.y/l,this.z/l,this.w/l):this.identity();}
 invert(){return this.set(-this.x,-this.y,-this.z,this.w);}multiply(q){return this.multiplyQuaternions(this.clone(),q);}premultiply(q){return this.multiplyQuaternions(q,this.clone());}
 multiplyQuaternions(a,b){return this.set(a.x*b.w+a.w*b.x+a.y*b.z-a.z*b.y,a.y*b.w+a.w*b.y+a.z*b.x-a.x*b.z,a.z*b.w+a.w*b.z+a.x*b.y-a.y*b.x,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z);}
 setFromEuler(e){const c1=Math.cos(e.x/2),c2=Math.cos(e.y/2),c3=Math.cos(e.z/2),s1=Math.sin(e.x/2),s2=Math.sin(e.y/2),s3=Math.sin(e.z/2);if(e.order!=='XYZ')throw Error('Euler order');return this.set(s1*c2*c3+c1*s2*s3,c1*s2*c3-s1*c2*s3,c1*c2*s3+s1*s2*c3,c1*c2*c3-s1*s2*s3);}
 setFromAxisAngle(v,a){const s=Math.sin(a/2);return this.set(v.x*s,v.y*s,v.z*s,Math.cos(a/2));}
 setFromUnitVectors(a,b){let r=a.dot(b)+1;if(r<Number.EPSILON){r=0;if(Math.abs(a.x)>Math.abs(a.z))this.set(-a.y,a.x,0,r);else this.set(0,-a.z,a.y,r);}else this.set(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x,r);return this.normalize();}
 setFromRotationMatrix(m){const e=m.elements,m11=e[0],m12=e[4],m13=e[8],m21=e[1],m22=e[5],m23=e[9],m31=e[2],m32=e[6],m33=e[10],tr=m11+m22+m33;let s;if(tr>0){s=.5/Math.sqrt(tr+1);return this.set((m32-m23)*s,(m13-m31)*s,(m21-m12)*s,.25/s);}if(m11>m22&&m11>m33){s=2*Math.sqrt(1+m11-m22-m33);return this.set(.25*s,(m12+m21)/s,(m13+m31)/s,(m32-m23)/s);}if(m22>m33){s=2*Math.sqrt(1+m22-m11-m33);return this.set((m12+m21)/s,.25*s,(m23+m32)/s,(m13-m31)/s);}s=2*Math.sqrt(1+m33-m11-m22);return this.set((m13+m31)/s,(m23+m32)/s,.25*s,(m21-m12)/s);}
 slerp(q,t){let dot=this.x*q.x+this.y*q.y+this.z*q.z+this.w*q.w,s=dot<0?-1:1;dot=Math.abs(dot);if(dot>.9995)return this.set(this.x+(q.x*s-this.x)*t,this.y+(q.y*s-this.y)*t,this.z+(q.z*s-this.z)*t,this.w+(q.w*s-this.w)*t).normalize();const a=Math.acos(Math.min(1,dot)),ss=Math.sin(a),a0=Math.sin((1-t)*a)/ss,a1=Math.sin(t*a)/ss*s;return this.set(a0*this.x+a1*q.x,a0*this.y+a1*q.y,a0*this.z+a1*q.z,a0*this.w+a1*q.w);}
}
class Euler{
 constructor(x=0,y=0,z=0,order='XYZ'){this._x=x;this._y=y;this._z=z;this.order=order;this._onChange=noop;}
 get x(){return this._x;}set x(v){this._x=v;this._onChange();}get y(){return this._y;}set y(v){this._y=v;this._onChange();}get z(){return this._z;}set z(v){this._z=v;this._onChange();}
 set(x,y,z,order='XYZ'){this._x=x;this._y=y;this._z=z;this.order=order;this._onChange();return this;}
}
class Matrix4{
 constructor(){this.elements=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1];}identity(){this.elements=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1];return this;}
 copy(m){this.elements=m.elements.slice();return this;}clone(){return new Matrix4().copy(this);}toArray(a=[],o=0){this.elements.forEach((v,i)=>a[o+i]=v);return a;}
 multiply(m){return this.multiplyMatrices(this.clone(),m);}premultiply(m){return this.multiplyMatrices(m,this.clone());}
 multiplyMatrices(a,b){const ae=a.elements,be=b.elements,o=Array(16).fill(0);for(let c=0;c<4;c++)for(let r=0;r<4;r++)for(let k=0;k<4;k++)o[c*4+r]+=ae[k*4+r]*be[c*4+k];this.elements=o;return this;}
 makeTranslation(x,y,z){this.identity();this.elements[12]=x;this.elements[13]=y;this.elements[14]=z;return this;}
 makeBasis(x,y,z){this.elements=[x.x,x.y,x.z,0,y.x,y.y,y.z,0,z.x,z.y,z.z,0,0,0,0,1];return this;}
 compose(p,q,s){const x=q.x,y=q.y,z=q.z,w=q.w,x2=x+x,y2=y+y,z2=z+z,xx=x*x2,xy=x*y2,xz=x*z2,yy=y*y2,yz=y*z2,zz=z*z2,wx=w*x2,wy=w*y2,wz=w*z2;this.elements=[(1-(yy+zz))*s.x,(xy+wz)*s.x,(xz-wy)*s.x,0,(xy-wz)*s.y,(1-(xx+zz))*s.y,(yz+wx)*s.y,0,(xz+wy)*s.z,(yz-wx)*s.z,(1-(xx+yy))*s.z,0,p.x,p.y,p.z,1];return this;}
 invert(){const m=Array.from({length:4},(_,r)=>Array.from({length:8},(_,c)=>c<4?this.elements[c*4+r]:(c-4===r?1:0)));for(let c=0;c<4;c++){let p=c;for(let r=c+1;r<4;r++)if(Math.abs(m[r][c])>Math.abs(m[p][c]))p=r;[m[c],m[p]]=[m[p],m[c]];const s=m[c][c];if(Math.abs(s)<1e-15)throw Error('singular');for(let j=0;j<8;j++)m[c][j]/=s;for(let r=0;r<4;r++)if(r!==c){const f=m[r][c];for(let j=0;j<8;j++)m[r][j]-=f*m[c][j];}}for(let c=0;c<4;c++)for(let r=0;r<4;r++)this.elements[c*4+r]=m[r][c+4];return this;}
}
class Object3D{
 constructor(){this.position=new Vector3();this.quaternion=new Quaternion();this.rotation=new Euler();this.rotation._onChange=()=>this.quaternion.setFromEuler(this.rotation);this.scale=new Vector3(1,1,1);this.children=[];this.parent=null;this.matrix=new Matrix4();this.matrixWorld=new Matrix4();this.matrixAutoUpdate=true;this.name='';}
 add(...arr){arr.forEach(o=>{if(o.parent)o.parent.remove(o);this.children.push(o);o.parent=this;});return this;}remove(o){this.children=this.children.filter(c=>c!==o);o.parent=null;}removeFromParent(){if(this.parent)this.parent.remove(this);}
 updateMatrix(){this.matrix.compose(this.position,this.quaternion,this.scale);}
 updateMatrixWorld(force){if(this.matrixAutoUpdate)this.updateMatrix();if(this.parent)this.matrixWorld.multiplyMatrices(this.parent.matrixWorld,this.matrix);else this.matrixWorld.copy(this.matrix);this.children.forEach(c=>c.updateMatrixWorld(force));}
 updateWorldMatrix(parents,children){if(parents&&this.parent)this.parent.updateWorldMatrix(true,false);if(this.matrixAutoUpdate)this.updateMatrix();if(this.parent)this.matrixWorld.multiplyMatrices(this.parent.matrixWorld,this.matrix);else this.matrixWorld.copy(this.matrix);if(children)this.children.forEach(c=>c.updateWorldMatrix(false,true));}
 getWorldPosition(v){this.updateWorldMatrix(true,false);return v.fromArray(this.matrixWorld.elements,12);}getWorldQuaternion(q){this.updateWorldMatrix(true,false);return q.setFromRotationMatrix(this.matrixWorld);}
 worldToLocal(v){this.updateWorldMatrix(true,false);return v.applyMatrix4(this.matrixWorld.clone().invert());}localToWorld(v){this.updateWorldMatrix(true,false);return v.applyMatrix4(this.matrixWorld);}traverse(fn){fn(this);this.children.forEach(c=>c.traverse(fn));}
}
class Group extends Object3D{}class Bone extends Object3D{constructor(){super();this.isBone=true;}}
class BufferAttribute{constructor(a,n,normalized=false){this.array=a;this.itemSize=n;this.count=a.length/n;this.normalized=normalized;}getX(i){return this.array[i*this.itemSize];}getY(i){return this.array[i*this.itemSize+1];}getZ(i){return this.array[i*this.itemSize+2];}getW(i){return this.array[i*this.itemSize+3];}setY(i,v){this.array[i*this.itemSize+1]=v;return this;}setXYZ(i,x,y,z){const n=i*this.itemSize;this.array[n]=x;this.array[n+1]=y;this.array[n+2]=z;return this;}}
class Float32BufferAttribute extends BufferAttribute{constructor(a,n){super(new Float32Array(a),n);}}
class Uint16BufferAttribute extends BufferAttribute{constructor(a,n){super(new Uint16Array(a),n);}}
class Box3{constructor(min=new Vector3(Infinity,Infinity,Infinity),max=new Vector3(-Infinity,-Infinity,-Infinity)){this.min=min;this.max=max;}makeEmpty(){this.min.set(Infinity,Infinity,Infinity);this.max.set(-Infinity,-Infinity,-Infinity);return this;}copy(b){this.min.copy(b.min);this.max.copy(b.max);return this;}expandByPoint(v){for(const k of ['x','y','z']){this.min[k]=Math.min(this.min[k],v[k]);this.max[k]=Math.max(this.max[k],v[k]);}return this;}applyMatrix4(m){const min=this.min.clone(),max=this.max.clone();this.makeEmpty();for(let i=0;i<8;i++)this.expandByPoint(new Vector3(i&1?max.x:min.x,i&2?max.y:min.y,i&4?max.z:min.z).applyMatrix4(m));return this;}getSize(v){return v.copy(this.max).sub(this.min);}}
class Sphere{constructor(center=new Vector3(),radius=0){this.center=center;this.radius=radius;}}
class BufferGeometry{
 constructor(){this.attributes={};this.groups=[];this.index=null;this.morphAttributes={};this.morphTargetsRelative=false;}setAttribute(k,v){this.attributes[k]=v;return this;}addGroup(start,count,materialIndex){this.groups.push({start,count,materialIndex});}setIndex(a){if(a instanceof BufferAttribute){this.index=a;return this;}let max=0;for(let i=0;i<a.length;i++)if(a[i]>max)max=a[i];this.index=new BufferAttribute(max>65535?new Uint32Array(a):new Uint16Array(a),1);return this;}
 computeBoundingBox(){const b=this.boundingBox=new Box3(),p=this.attributes.position;if(p)for(let i=0;i<p.count;i++)b.expandByPoint(new Vector3().fromArray(p.array,i*3));for(const m of this.morphAttributes.position||[]){const mb=new Box3();for(let i=0;i<m.count;i++)mb.expandByPoint(new Vector3().fromArray(m.array,i*3));if(this.morphTargetsRelative){b.expandByPoint(new Vector3(b.min.x+mb.min.x,b.min.y+mb.min.y,b.min.z+mb.min.z));b.expandByPoint(new Vector3(b.max.x+mb.max.x,b.max.y+mb.max.y,b.max.z+mb.max.z));}else{b.expandByPoint(mb.min);b.expandByPoint(mb.max);}}return b;}
 computeBoundingSphere(){const p=this.attributes.position;this.computeBoundingBox();const b=this.boundingBox,center=p&&p.count?new Vector3((b.min.x+b.max.x)*.5,(b.min.y+b.max.y)*.5,(b.min.z+b.max.z)*.5):new Vector3();let radiusSq=0;if(p)for(let i=0;i<p.count;i++)radiusSq=Math.max(radiusSq,center.distanceToSquared(new Vector3().fromArray(p.array,i*3)));for(const m of this.morphAttributes.position||[])for(let i=0;i<m.count;i++){const point=new Vector3().fromArray(m.array,i*3);if(this.morphTargetsRelative)point.add(new Vector3().fromArray(p.array,i*3));radiusSq=Math.max(radiusSq,center.distanceToSquared(point));}this.boundingSphere=new Sphere(center,Math.sqrt(radiusSq));return this.boundingSphere;}
 computeVertexNormals(){const p=this.attributes.position.array,n=new Float32Array(p.length),ids=this.index.array;for(let i=0;i<ids.length;i+=3){const a=ids[i]*3,b=ids[i+1]*3,c=ids[i+2]*3,cbx=p[c]-p[b],cby=p[c+1]-p[b+1],cbz=p[c+2]-p[b+2],abx=p[a]-p[b],aby=p[a+1]-p[b+1],abz=p[a+2]-p[b+2],nx=cby*abz-cbz*aby,ny=cbz*abx-cbx*abz,nz=cbx*aby-cby*abx;for(const j of [a,b,c]){n[j]+=nx;n[j+1]+=ny;n[j+2]+=nz;}}for(let i=0;i<n.length;i+=3){const l=Math.sqrt(n[i]*n[i]+n[i+1]*n[i+1]+n[i+2]*n[i+2])||1;n[i]/=l;n[i+1]/=l;n[i+2]/=l;}this.setAttribute('normal',new BufferAttribute(n,3));}dispose(){}
 rotateX(a){const p=this.attributes.position,c=Math.cos(a),s=Math.sin(a);for(let i=0;i<p.count;i++){const y=p.getY(i),z=p.getZ(i);p.setXYZ(i,p.getX(i),y*c-z*s,y*s+z*c);}return this;}
}
class PlaneGeometry extends BufferGeometry{constructor(w,h,nx,ny){super();const p=[],ids=[];for(let y=0;y<=ny;y++)for(let x=0;x<=nx;x++)p.push(x/nx*w-w/2,h/2-y/ny*h,0);for(let y=0;y<ny;y++)for(let x=0;x<nx;x++){const a=y*(nx+1)+x,b=a+nx+1,c=b+1,d=a+1;ids.push(a,b,d,b,c,d);}this.setAttribute('position',new Float32BufferAttribute(p,3));this.setIndex(ids);}}
class Skeleton{constructor(b){this.bones=b;this.boneInverses=[];}calculateInverses(){this.boneInverses=this.bones.map(b=>b.matrixWorld.clone().invert());}update(){}dispose(){}}
class Mesh extends Object3D{constructor(g,m){super();this.geometry=g;this.material=m;this.isMesh=true;}}
class SkinnedMesh extends Mesh{
 constructor(g,m){super(g,m);this.isSkinnedMesh=true;this.bindMatrix=new Matrix4();this.bindMatrixInverse=new Matrix4();}
 bind(s,b){this.skeleton=s;this.bindMatrix.copy(b||this.matrixWorld);this.bindMatrixInverse.copy(this.bindMatrix).invert();}
 updateMatrixWorld(force){super.updateMatrixWorld(force);this.bindMatrixInverse.copy(this.matrixWorld).invert();}
 boneTransform(i,out){const g=this.geometry,p=new Vector3().fromArray(g.attributes.position.array,i*3).applyMatrix4(this.bindMatrix),idx=g.attributes.skinIndex.array,w=g.attributes.skinWeight.array;out.set(0,0,0);for(let k=0;k<4;k++)if(w[i*4+k]){const b=idx[i*4+k],m=new Matrix4().multiplyMatrices(this.skeleton.bones[b].matrixWorld,this.skeleton.boneInverses[b]);out.addScaledVector(p.clone().applyMatrix4(m),w[i*4+k]);}return out.applyMatrix4(this.bindMatrixInverse);}
}
class Color{constructor(hex=0xffffff){this.r=((hex>>16)&255)/255;this.g=((hex>>8)&255)/255;this.b=(hex&255)/255;}convertSRGBToLinear(){for(const k of ['r','g','b'])this[k]=this[k]<=.04045?this[k]/12.92:Math.pow((this[k]+.055)/1.055,2.4);return this;}copy(c){this.r=c.r;this.g=c.g;this.b=c.b;return this;}lerp(c,t){for(const k of ['r','g','b'])this[k]+=(c[k]-this[k])*t;return this;}}
class Material{constructor(o={}){Object.assign(this,o);}dispose(){}}
class CanvasTexture{constructor(c){this.image=c;}dispose(){}}
const MathUtils={clamp:(x,a,b)=>Math.max(a,Math.min(b,x)),lerp:(a,b,t)=>a+(b-a)*t,smoothstep:(x,a,b)=>{const t=Math.max(0,Math.min(1,(x-a)/(b-a)));return t*t*(3-2*t);}};
module.exports={Vector2,Vector3,Quaternion,Euler,Matrix4,Object3D,Group,Bone,BufferGeometry,BufferAttribute,Float32BufferAttribute,Uint16BufferAttribute,Box3,Sphere,PlaneGeometry,Skeleton,Mesh,SkinnedMesh,Color,MeshStandardMaterial:Material,MeshBasicMaterial:Material,ShaderMaterial:Material,CanvasTexture,MathUtils,RepeatWrapping:1000,sRGBEncoding:3001};
