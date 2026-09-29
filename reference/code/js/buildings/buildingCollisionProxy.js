(function(){
'use strict';
const R=globalThis.RTS,B=R.Buildings,EPS=1e-7;
const sub=(a,b)=>a.map((v,i)=>v-b[i]),dot=(a,b)=>a.reduce((s,v,i)=>s+v*b[i],0),cross=(a,b)=>[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]];
function solidSegments(w){
 const openings=(w.openings||[]).filter(o=>o.open??(o.defaultState!=='CLOSED')),clamp=(v,max)=>Math.max(0,Math.min(max,v)),cuts=[0,w.length],out=[];
 for(const o of openings){const c=o.center??o.t*w.length;cuts.push(clamp(c-o.width/2,w.length),clamp(c+o.width/2,w.length));}cuts.sort((a,b)=>a-b);
 for(let i=0;i<cuts.length-1;i++){const lo=cuts[i],hi=cuts[i+1],mid=(lo+hi)/2;if(hi-lo<EPS)continue;
 const active=openings.filter(o=>{const c=o.center??o.t*w.length;return mid>c-o.width/2&&mid<c+o.width/2;}),ys=[0,w.height];
 for(const o of active)ys.push(clamp(o.bottom||0,w.height),clamp((o.bottom||0)+o.height,w.height));ys.sort((a,b)=>a-b);
 for(let j=0;j<ys.length-1;j++){const bottom=ys[j],top=ys[j+1];if(top-bottom>EPS&&!active.some(o=>(bottom+top)/2>(o.bottom||0)&&(bottom+top)/2<(o.bottom||0)+o.height))out.push({lo,hi,bottom,top,wall:w});}
 }return out;
}
function wallPoint(s,u,y,n=0){const w=s.wall,dx=(w.edge.b[0]-w.edge.a[0])/w.length,dz=(w.edge.b[1]-w.edge.a[1])/w.length;return [w.edge.a[0]+dx*u-dz*n,s.elevation+y,w.edge.a[1]+dz*u+dx*n];}
function wallLocal(s,p){const w=s.wall,dx=(w.edge.b[0]-w.edge.a[0])/w.length,dz=(w.edge.b[1]-w.edge.a[1])/w.length,x=p[0]-w.edge.a[0],z=p[2]-w.edge.a[1];return [x*dx+z*dz,p[1]-s.elevation,-x*dz+z*dx];}
function bounds(points){return [0,1,2].flatMap(i=>[Math.min(...points.map(p=>p[i])),Math.max(...points.map(p=>p[i]))]);}
function prism(points,offset){if(dot(cross(sub(points[1],points[0]),sub(points[2],points[0])),offset)>0)points=points.slice().reverse();const lower=points.map(p=>p.map((v,i)=>v+offset[i]));return [points,lower.slice().reverse(),...points.map((p,i)=>{const j=(i+1)%points.length;return [p,lower[i],lower[j],points[j]];})];}
function wallFaces(s){const n=s.wall.thickness/2;return prism([wallPoint(s,s.lo,s.bottom,n),wallPoint(s,s.hi,s.bottom,n),wallPoint(s,s.hi,s.top,n),wallPoint(s,s.lo,s.top,n)],sub(wallPoint(s,0,0,-n),wallPoint(s,0,0,n)));}
function triangles(points){
 if(points.length<3)return [];let normal=[0,0,0];for(let i=1;i<points.length-1&&Math.hypot(...normal)<EPS;i++)normal=cross(sub(points[i],points[0]),sub(points[i+1],points[0]));
 const drop=normal.map(Math.abs).indexOf(Math.max(...normal.map(Math.abs))),axes=[0,1,2].filter(i=>i!==drop),flat=points.flatMap(p=>axes.map(i=>p[i]));
 const indices=B.Triangulation.triangulate(flat,[],2),out=[];for(let i=0;i<indices.length;i+=3)out.push(indices.slice(i,i+3).map(j=>points[j]));return out;
}
function slabTriangles(slab){const t=B.Polygon.triangulate(slab.polygon),out=[];for(let i=0;i<t.indices.length;i+=3)out.push(t.indices.slice(i,i+3).map(j=>[t.vertices[2*j],slab.y,t.vertices[2*j+1]]));return out;}
function boxHit(a,b,box){let t0=0,t1=1;for(let i=0;i<3;i++){const d=b[i]-a[i];if(Math.abs(d)<EPS){if(a[i]<box[i*2]-EPS||a[i]>box[i*2+1]+EPS)return null;}else{let p=(box[i*2]-a[i])/d,q=(box[i*2+1]-a[i])/d;if(p>q)[p,q]=[q,p];t0=Math.max(t0,p);t1=Math.min(t1,q);if(t0>t1+EPS)return null;}}return t0;}
function triangleHit(a,b,tri){const e1=sub(tri[1],tri[0]),e2=sub(tri[2],tri[0]),d=sub(b,a),p=cross(d,e2),det=dot(e1,p);if(Math.abs(det)<EPS)return null;const v=sub(a,tri[0]),u=dot(v,p)/det,q=cross(v,e1),w=dot(d,q)/det,t=dot(e2,q)/det;if(u<-EPS||w<-EPS||u+w>1+EPS||t<-EPS||t>1+EPS)return null;return Math.max(0,Math.min(1,t));}
class BuildingCollisionProxy{
 constructor(plan){if(plan?.schema!=='rts.building-plan/4')throw new TypeError('BuildingCollisionProxy requires BuildingPlan/4');this.plan=plan;this.doors=new Map();this.supports=[];for(const w of plan.walls||[])for(const o of w.openings||[])if(o.kind==='door')this.doors.set(o.id,o);for(const f of plan.roof?.faces||[])this.supports.push({...f,faceId:f.id,kind:'roof',triangles:triangles(f.points),bounds:bounds(f.points)});this.rebuild();}
 rebuild(){this.walls=[];for(const w of this.plan.walls||[])for(const [index,s] of solidSegments(w).entries()){s.elevation=this.plan.storeys?.[w.floor]?.elevation??(w.floor||0)*this.plan.spec.storeys.floorHeight;s.wallId=w.id;s.physicalSolidId=w.physicalSolidId;s.segmentIndex=index;s.faces=wallFaces(s);s.box=bounds(s.faces.flat());this.walls.push(s);}this._index=this.walls;return this;}
 setDoorState(id,open){const d=this.doors.get(id);if(d)d.open=!!open;return this.rebuild();}
 querySegment(a,b){const out=[];for(const s of this._index){if(boxHit(a,b,s.box)===null)continue;const t=boxHit(wallLocal(s,a),wallLocal(s,b),[s.lo,s.hi,s.bottom,s.top,-s.wall.thickness/2,s.wall.thickness/2]);if(t!==null)out.push({...s,t,point:a.map((v,i)=>v+(b[i]-v)*t)});}for(const s of this.supports){if(boxHit(a,b,s.bounds)===null)continue;const hits=s.triangles.map((tri,triangleIndex)=>({t:triangleHit(a,b,tri),triangleIndex})).filter(h=>h.t!==null);if(hits.length){const {t,triangleIndex}=hits.sort((x,y)=>x.t-y.t)[0];out.push({...s,t,triangleIndex,point:a.map((v,i)=>v+(b[i]-v)*t)});}}return out.sort((x,y)=>x.t-y.t);}
 queryRay(a,d,length=1e5){return this.querySegment(a,d.map((v,i)=>a[i]+v*length));}
 stats(){return {wallSegments:this.walls.length,activeWallSegments:this._index.length,supports:this.supports.length,doors:this.doors.size};}
}
B.wallSolidSegments=solidSegments;B.collisionGeometry={solidSegments,wallPoint,wallLocal,wallFaces,triangles,slabTriangles,prism,bounds};R.BuildingCollisionProxy=BuildingCollisionProxy;
})();
