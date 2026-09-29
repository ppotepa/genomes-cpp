(function(){
'use strict';
const R=globalThis.RTS;
class BuildingDamageChunkManager{
 constructor(runtime,options={}){this.runtime=runtime;this.plan=runtime.plan;this.host=options.host||null;this.chunkSize=options.chunkSize??R.Config.WORLD_BUILDING_RUNTIME.chunkSize;this.maxActive=options.maxActive??R.Config.WORLD_BUILDING_RUNTIME.maxActiveDamageChunksPerBuilding;this.active=new Map();this.virtualSupports=new Map();this.pending=new Set();this.descriptors=new Map();this.revision=0;runtime.damage=this;}
 _id(kind,item,index=0){return kind+':'+(item.wallId||item.faceId||item.id)+':'+index;}
 _queue(kind,source,index=0){const id=this._id(kind,source,index);if(!this.active.has(id)){this.pending.add(id);this.descriptors.set(id,{kind,source,index});}return id;}
 _activate(id,descriptor){if(this.active.has(id)||this.active.size>=this.maxActive||!descriptor)return false;const part=this.materialize(id,descriptor);if(!part)return false;this.active.set(id,{id,descriptor,part,virtualSupport:true});this.runtime.hideDamageRegion(id);this.virtualSupports.set(id,new Set());return true;}
 materialize(id,descriptor){
  const G=R.Buildings.collisionGeometry,source=descriptor.source||descriptor,kind=descriptor.kind||id.split(':')[0],index=descriptor.index??Number(id.split(':').at(-1));let faces;
  if(kind==='wall'){const segment=(this.runtime.collision?.walls||[]).find(s=>s.wallId===source.id&&s.segmentIndex===index);if(!segment)return null;faces=segment.faces;}
  else if(kind==='roof'){const triangle=G.triangles(source.points)[index];if(!triangle)return null;faces=G.prism(triangle,[0,-(source.thickness||.15),0]);}
  else if(kind==='slab'){const triangle=G.slabTriangles(source)[index];if(!triangle)return null;faces=G.prism(triangle,[0,-(source.thickness||.18),0]);}
  else return null;
  this.runtime.root.updateWorldMatrix(true,false);const matrix=this.runtime.root.matrixWorld,transformed=faces.map(face=>face.map(point=>new THREE.Vector3(...point).applyMatrix4(matrix).toArray())),bounds=G.bounds(transformed.flat());
  const part={id,material:source.material||this.plan.spec.material||'concrete',min:[bounds[0],bounds[2],bounds[4]],max:[bounds[1],bounds[3],bounds[5]],faces:transformed,runtimeId:this.runtime.id,assemblyId:source.assemblyId||'world-damage',physicalSolidId:source.physicalSolidId||source.id,wallId:kind==='wall'?source.id:undefined,sourceId:source.id,layers:(source.layers||[]).map(layer=>({...layer})),grainDirection:source.grainDirection||source.grain||null,buildingPanel:true,structuralGraph:true,anchored:false,supports:[]};
  if(this.host?.model)return this.host.model.add(part);return new R.MaterialModel([part],this.plan.spec.seed).parts.get(id);
 }
 activateForAP(hit){
  if(!hit)return [];const d=hit.part||hit,wallId=hit.wallId||d.wallId,faceId=hit.faceId||d.faceId||(hit.kind==='roof'?hit.id:null);
  if(wallId){const source=this.plan.walls.find(w=>w.id===wallId);if(!source)return [];let index=hit.segmentIndex;if(index===undefined&&hit.point){const p=this.runtime.worldToLocal(hit.point),G=R.Buildings.collisionGeometry;index=this.runtime.collision.walls.find(s=>{if(s.wallId!==wallId)return false;const q=G.wallLocal(s,p);return q[0]>=s.lo-1e-6&&q[0]<=s.hi+1e-6&&q[1]>=s.bottom-1e-6&&q[1]<=s.top+1e-6;})?.segmentIndex;}return [this._queue('wall',source,index??0)];}
  const source=this.plan.roof?.faces.find(f=>f.id===(faceId||d.sourceId||d.id));if(source)return [this._queue('roof',source,hit.triangleIndex??0)];const slab=this.plan.slabs?.find(s=>s.id===(d.sourceId||d.id));return slab?[this._queue('slab',slab,hit.triangleIndex??0)]:[];
 }
 activateForHE(sphere){
  if(!sphere||!(sphere.radius>=0))return [];const center=new THREE.Vector3(...sphere.center),ids=[],G=R.Buildings.collisionGeometry;
  this.runtime.root.updateWorldMatrix(true,false);const matrix=this.runtime.root.matrixWorld;
  const near=triangles=>triangles.some(points=>{const p=points.map(q=>new THREE.Vector3(...q).applyMatrix4(matrix)),closest=new THREE.Triangle(...p).closestPointToPoint(center,new THREE.Vector3());return closest.distanceTo(center)<=sphere.radius;});
  for(const s of this.runtime.collision.walls)if(near(s.faces.flatMap(G.triangles)))ids.push(this._queue('wall',s.wall,s.segmentIndex));
  for(const f of this.plan.roof?.faces||[])for(const [i,tri] of G.triangles(f.points).entries())if(near([tri]))ids.push(this._queue('roof',f,i));
  for(const s of this.plan.slabs||[])for(const [i,tri] of G.slabTriangles(s).entries())if(near([tri]))ids.push(this._queue('slab',s,i));return ids;
 }
 flush(){let changed=false;const work=()=>{for(const id of this.pending){if(this.active.has(id)){this.pending.delete(id);continue;}if(this.active.size>=this.maxActive)break;if(this._activate(id,this.descriptors.get(id)))changed=true;this.pending.delete(id);this.descriptors.delete(id);}};if(this.host?.model?.batch)this.host.model.batch(work);else work();if(changed){this.host?.model?.reindex?.();this.revision++;}return changed;}
 markNeighborDestroyed(id){this.virtualSupports.delete(id);const chunk=this.active.get(id);if(chunk)chunk.virtualSupport=false;return this;}
 stats(){return {activeChunks:this.active.size,pendingChunks:this.pending.size,materialModelParts:[...this.active.values()].filter(x=>x.part).length,virtualSupportAnchors:this.virtualSupports.size,revision:this.revision};}
 dispose(){const model=this.host?.model;const work=()=>{for(const chunk of this.active.values()){if(model?.remove)for(const part of [...model.parts.values()])if(part.id===chunk.id||part.originalId===chunk.id)model.remove(part);this.runtime.showDamageRegion(chunk.id);}};if(model?.batch)model.batch(work);else work();this.active.clear();this.pending.clear();this.descriptors.clear();this.virtualSupports.clear();if(this.runtime.damage===this)this.runtime.damage=null;}
}
R.BuildingDamageChunkManager=BuildingDamageChunkManager;
})();
