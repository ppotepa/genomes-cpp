(function () {
  'use strict';
  const R=window.RTS,V=(x=0,y=0,z=0)=>new THREE.Vector3(x,y,z);
  const BOX_FACES=[[0,1],[0,-1],[1,1],[1,-1],[2,1],[2,-1]],BOX_AXES=['x','y','z'];
  // Small procedural primitives, directly emitted in skeleton bind space.
  // All gear is batched into one SkinnedMesh (three material groups), not a
  // separate draw call for each buckle. Rigid parts have one bone weight.
  R.GearGeometry=class GearGeometry extends R.SurfaceBuilder {
    constructor(rig,detail){super(rig);this.high=detail===true||detail==='high';this.far=detail==='far';this.morphs={};this._boxHalf=V();this._boxCore=V();this._boxRadius=V();this._boxRaw=V();this._boxPoint=V();this._boxNormal=V();}
    box(center,size,weights,color,material=0,round=.13,rotation=null){
      const half=this._boxHalf.copy(size).multiplyScalar(.5),radius=Math.min(half.x,half.y,half.z)*round*2;
      const core=this._boxCore.copy(half).sub(this._boxRadius.set(radius,radius,radius)),segments=this.high?4:this.far?1:2;
      for(const face of BOX_FACES){const axis=face[0],sign=face[1];
        const loops=[],keys=BOX_AXES,u=(axis+1)%3,v=(axis+2)%3;
        for(let j=0;j<=segments;j++){
          const row=[];
          for(let i=0;i<=segments;i++){
            const raw=this._boxRaw.set(0,0,0);raw[keys[axis]]=half[keys[axis]]*sign;
            raw[keys[u]]=(i/segments*2-1)*half[keys[u]];raw[keys[v]]=(j/segments*2-1)*half[keys[v]];
            const p=this._boxPoint.set(R.Math.clamp(raw.x,-core.x,core.x),R.Math.clamp(raw.y,-core.y,core.y),R.Math.clamp(raw.z,-core.z,core.z));
            const normal=this._boxNormal.copy(raw).sub(p).normalize();p.addScaledVector(normal,radius);
            if(rotation){p.applyQuaternion(rotation);normal.applyQuaternion(rotation);}
            p.add(center);row.push(this.vertex(p,typeof weights==='function'?weights(p):weights,color,normal,[i/segments,j/segments]));
          }loops.push(row);
        }
        for(let j=0;j<segments;j++)for(let i=0;i<segments;i++){
          this.triangle(loops[j][i],loops[j+1][i],loops[j][i+1],material);
          this.triangle(loops[j][i+1],loops[j+1][i],loops[j+1][i+1],material);
        }
      }
    }
    cylinder(a,b,r,weights,color,material=1,segments=10){
      const dir=b.clone().sub(a).normalize(),guide=Math.abs(dir.z)<.9?V(0,0,1):V(0,1,0),u=dir.clone().cross(guide).normalize(),v=u.clone().cross(dir).normalize();
      const one=this.ring(a,u,v,r,r,segments,weights,color),two=this.ring(b,u,v,r,r,segments,weights,color);
      this.bridge(one,two,material);this.cap(one,weights,color,dir.clone().multiplyScalar(-1),material);this.cap(two,weights,color,dir,material);
    }
    ribbon(points,width,weights,color,material=0,axis=V(1,0,0)){
      let prev=null;
      for(let i=0;i<points.length;i++){
        const p=points[i],w=typeof weights==='function'?weights(p):weights;
        const tangent=points[Math.min(i+1,points.length-1)].clone().sub(points[Math.max(i-1,0)]).normalize();
        const normal=axis.clone().cross(tangent).normalize();
        const edge=[this.vertex(p.clone().addScaledVector(axis,-width/2),w,color,normal,[0,i]),this.vertex(p.clone().addScaledVector(axis,width/2),w,color,normal,[1,i])];
        if(prev){this.triangle(prev[0],edge[0],prev[1],material);this.triangle(prev[1],edge[0],edge[1],material);}
        prev=edge;
      }
    }
    tube(points,r,weights,color,material=0){this.tubePath(points,r,weights,color,material,this.high?8:this.far?4:6);}
  };
})();
