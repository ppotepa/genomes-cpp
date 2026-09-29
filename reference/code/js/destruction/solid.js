(function(){
  'use strict';
  const R=globalThis.RTS;
  const V={add:(a,b)=>a.map((x,i)=>x+b[i]),sub:(a,b)=>a.map((x,i)=>x-b[i]),mul:(a,s)=>a.map(x=>x*s),dot:(a,b)=>a.reduce((s,x,i)=>s+x*b[i],0),cross:(a,b)=>[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]],length:a=>Math.hypot(...a)};
  V.unit=a=>V.mul(a,1/(V.length(a)||1));V.mix=(a,b,t)=>V.add(a,V.mul(V.sub(b,a),t));
  const EPS=1e-7;
  function bounds(faces){const min=[Infinity,Infinity,Infinity],max=[-Infinity,-Infinity,-Infinity];for(const f of faces)for(const p of f)for(let i=0;i<3;i++){min[i]=Math.min(min[i],p[i]);max[i]=Math.max(max[i],p[i]);}return {min,max};}
  function box(min,max){const p=[];for(let z=0;z<2;z++)for(let y=0;y<2;y++)for(let x=0;x<2;x++)p.push([x?max[0]:min[0],y?max[1]:min[1],z?max[2]:min[2]]);return [[0,2,3,1],[4,5,7,6],[0,4,6,2],[1,3,7,5],[0,1,5,4],[2,6,7,3]].map(f=>f.map(i=>p[i]));}
  function planes(faces){return faces.map(f=>{let area=[0,0,0];for(let i=1;i<f.length-1;i++)area=V.add(area,V.cross(V.sub(f[i],f[0]),V.sub(f[i+1],f[0])));const n=V.unit(area);return {n,d:V.dot(n,f[0])};});}
  // Clip a closed convex polyhedron and cap the cross section, retaining n.p <= d.
  function clip(faces,n,d){
    const out=[],cap=[];let cut=false,inside=false;
    for(const face of faces){const poly=[];for(let i=0;i<face.length;i++){
      const a=face[i],b=face[(i+1)%face.length],da=V.dot(n,a)-d,db=V.dot(n,b)-d;
      if(da<=EPS){poly.push(a);inside=true;}else cut=true;
      if((da< -EPS&&db>EPS)||(da>EPS&&db< -EPS)){const p=V.mix(a,b,da/(da-db));poly.push(p);cap.push(p);}
      else if(Math.abs(da)<=EPS)cap.push(a);
    }if(poly.length>=3)out.push(poly);}
    if(!inside)return [];if(!cut)return faces;
    const points=cap.filter((p,i)=>!cap.slice(0,i).some(q=>V.length(V.sub(p,q))<EPS*10));
    if(points.length>=3){const c=V.mul(points.reduce(V.add,[0,0,0]),1/points.length),u=V.unit(V.cross(n,Math.abs(n[0])<.8?[1,0,0]:[0,1,0])),v=V.cross(n,u);points.sort((a,b)=>Math.atan2(V.dot(V.sub(a,c),v),V.dot(V.sub(a,c),u))-Math.atan2(V.dot(V.sub(b,c),v),V.dot(V.sub(b,c),u)));out.push(points);}
    return out;
  }
  function subtract(faces,cutter){let remaining=faces;const pieces=[];for(const {n,d} of cutter){if(!remaining.length)break;const outside=clip(remaining,V.mul(n,-1),-d);if(outside.length)pieces.push(outside);remaining=clip(remaining,n,d);}return pieces;}
  function volume(faces){let v=0;for(const f of faces)for(let i=1;i<f.length-1;i++)v+=V.dot(f[0],V.cross(f[i],f[i+1]))/6;return Math.abs(v);}
  // Fast half-space sweep. With radius > 0 it is a conservative broad phase:
  // shifted planes have sharp expanded corners instead of a true rounded Minkowski sum.
  function ray(planes,a,b,radius=0){let enter=0,exit=1,normal=[0,1,0];const dx=b[0]-a[0],dy=b[1]-a[1],dz=b[2]-a[2];for(const p of planes){const n=p.n,dist=n[0]*a[0]+n[1]*a[1]+n[2]*a[2]-p.d-radius,den=n[0]*dx+n[1]*dy+n[2]*dz;if(Math.abs(den)<1e-12){if(dist>0)return null;continue;}const t=-dist/den;if(den<0){if(t>enter){enter=t;normal=n;}}else exit=Math.min(exit,t);if(enter>exit)return null;}return exit>=0&&enter<=1?{enter:Math.max(0,enter),exit:Math.min(1,exit),normal}:null;}
  function closestTriangle(p,a,b,c){
    const ab=V.sub(b,a),ac=V.sub(c,a),ap=V.sub(p,a),d1=V.dot(ab,ap),d2=V.dot(ac,ap);if(d1<=0&&d2<=0)return a;
    const bp=V.sub(p,b),d3=V.dot(ab,bp),d4=V.dot(ac,bp);if(d3>=0&&d4<=d3)return b;
    const vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0){const v=d1/(d1-d3);return V.add(a,V.mul(ab,v));}
    const cp=V.sub(p,c),d5=V.dot(ab,cp),d6=V.dot(ac,cp);if(d6>=0&&d5<=d6)return c;
    const vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0){const w=d2/(d2-d6);return V.add(a,V.mul(ac,w));}
    const va=d3*d6-d5*d4;if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0){const w=(d4-d3)/((d4-d3)+(d5-d6));return V.add(b,V.mul(V.sub(c,b),w));}
    const denom=1/(va+vb+vc),v=vb*denom,w=vc*denom;return V.add(a,V.add(V.mul(ab,v),V.mul(ac,w)));
  }
  function stableNormal(n){const u=V.unit(n);return u.map(x=>Math.abs(x)<1e-12?0:Math.abs(Math.abs(x)-1)<1e-12?Math.sign(x):x);}
  function closestSurface(faces,planes,p){
    if(planes.every(q=>V.dot(q.n,p)<=q.d+1e-9))return {distance2:0,point:p.slice(),normal:[0,1,0]};
    let best=Infinity,point=null,normal=[0,1,0];
    for(let fi=0;fi<faces.length;fi++){const face=faces[fi];for(let i=1;i<face.length-1;i++){const q=closestTriangle(p,face[0],face[i],face[i+1]),d=V.sub(p,q),d2=V.dot(d,d);if(d2<best){best=d2;point=q;normal=d2>1e-16?stableNormal(d):stableNormal(planes[fi]?.n||normal);}}}
    return {distance2:best,point,normal};
  }
  function sweptSphere(faces,planes,a,b,radius=0){
    const broad=ray(planes,a,b,radius);if(!broad||radius<=0)return broad;const r2=radius*radius,at=t=>V.mix(a,b,t),first=closestSurface(faces,planes,at(broad.enter));
    if(first.distance2<=r2*(1+1e-6))return {...broad,normal:first.distance2>1e-16?first.normal:broad.normal};
    let lo=broad.enter,hi=broad.exit;
    // Distance to a convex set along a segment is convex. Find its minimum only
    // for corner/edge candidates that failed the cheap face-contact verification.
    let l=lo,h=hi;for(let i=0;i<7;i++){const m1=l+(h-l)/3,m2=h-(h-l)/3,d1=closestSurface(faces,planes,at(m1)).distance2,d2=closestSurface(faces,planes,at(m2)).distance2;if(d1<d2)h=m2;else l=m1;}
    const tMin=(l+h)/2,min=closestSurface(faces,planes,at(tMin));if(min.distance2>r2*(1+1e-6))return null;
    hi=tMin;for(let i=0;i<10;i++){const mid=(lo+hi)/2,d=closestSurface(faces,planes,at(mid)).distance2;if(d<=r2)hi=mid;else lo=mid;}
    const enter=hi,exact=closestSurface(faces,planes,at(enter));let exitLo=tMin,exitHi=broad.exit,exit=broad.exit;
    if(closestSurface(faces,planes,at(exitHi)).distance2>r2){for(let i=0;i<10;i++){const mid=(exitLo+exitHi)/2,d=closestSurface(faces,planes,at(mid)).distance2;if(d<=r2)exitLo=mid;else exitHi=mid;}exit=exitLo;}
    return {enter,exit,normal:exact.normal};
  }
  function stream(seed,id){let h=seed>>>0;for(const c of String(id))h=Math.imul(h^c.charCodeAt(0),16777619);return ()=>{h=(h+0x6D2B79F5)|0;let t=Math.imul(h^(h>>>15),1|h);t^=t+Math.imul(t^(t>>>7),61|t);return ((t^(t>>>14))>>>0)/4294967296;};}
  function centroid(faces){const points=faces.flat();return V.mul(points.reduce(V.add,[0,0,0]),1/Math.max(1,points.length));}
  function splitThree(faces,seed,id,layering=0){const rng=stream(seed,id),normal=()=>V.unit([rng()-.5,(rng()-.5)*(1+6*layering),rng()-.5]),n=normal(),d=V.dot(n,centroid(faces)),a=clip(faces,n,d),b=clip(faces,V.mul(n,-1),-d),large=volume(a)>volume(b)?a:b,small=large===a?b:a,m=normal(),e=V.dot(m,centroid(large));return [small,clip(large,m,e),clip(large,V.mul(m,-1),-e)].filter(f=>f.length&&volume(f)>1e-10);}
  R.DestructionSolid={V,box,bounds,planes,clip,subtract,volume,ray,sweptSphere,closestSurface,stream,centroid,splitThree};
})();
