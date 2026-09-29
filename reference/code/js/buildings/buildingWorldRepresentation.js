(function(){
'use strict';
const R=globalThis.RTS,B=R.Buildings;
class BuildingWorldRepresentation{
 constructor(plan,options={}){if(plan?.schema!=='rts.building-plan/4')throw new TypeError('BuildingWorldRepresentation requires BuildingPlan/4');this.plan=plan;this.options=options;this.root=new THREE.Group();this.root.name='Building world representation';this.detail='FAR';this.damageRegions=new Map();this._bounds=B.Polygon.bounds(plan.footprint.polygon);this.chunks=new Map();this.detailRoot=new THREE.Group();this.detailRoot.name='world-detail';this.detailRoot.visible=false;this.root.add(this.detailRoot);this.detailInstance=null;this.detailBuildError=null;this.disposed=false;this._build();}
 _mesh(parent,id,triangles,material,source){const g=new THREE.BufferGeometry();g.setAttribute('position',new THREE.Float32BufferAttribute(triangles.flat(2),3));g.computeVertexNormals();const m=new THREE.Mesh(g,material);m.name=id;m.userData={sourceId:source.id,physicalSolidId:source.physicalSolidId};parent.add(m);this.chunks.set(id,m);return m;}
 _build(){
  const G=B.collisionGeometry,wallMaterial=new THREE.MeshStandardMaterial({color:0xaaa18f,side:THREE.DoubleSide}),roofMaterial=new THREE.MeshStandardMaterial({color:0x525b5d,side:THREE.DoubleSide});
  this.envelope=new THREE.Group();this.envelope.name='far-envelope';this.roof=new THREE.Group();this.roof.name='far-roof';this.root.add(this.envelope,this.roof);
  for(const w of this.plan.walls||[])for(const [i,s] of G.solidSegments(w).entries()){s.elevation=this.plan.storeys?.[w.floor]?.elevation??(w.floor||0)*this.plan.spec.storeys.floorHeight;const mesh=this._mesh(this.envelope,'wall:'+w.id+':'+i,G.wallFaces(s).flatMap(G.triangles),wallMaterial,w);mesh.userData.internal=!!w.internal;}
  for(const slab of this.plan.slabs||[])for(const [i,tri] of G.slabTriangles(slab).entries())this._mesh(this.envelope,'slab:'+slab.id+':'+i,[tri],wallMaterial,slab);
  for(const f of this.plan.roof?.faces||[])for(const [i,tri] of G.triangles(f.points).entries())this._mesh(this.roof,'roof:'+f.id+':'+i,[tri],roofMaterial,f);
  this._materials=[wallMaterial,roofMaterial];this._refresh();
 }
 _damageMatches(c){const regions=this.damageRegions,source=c.sourceId||'';if(regions.has(source)||regions.has(c.id))return true;if(c.wallId&&[...regions.keys()].some(id=>id.startsWith('wall:'+c.wallId+':')))return true;if(c.roofId&&[...regions.keys()].some(id=>id.startsWith('roof:'+c.roofId+':')))return true;if(c.category==='slab'&&source&&[...regions.keys()].some(id=>id.startsWith('slab:'+source+':')))return true;return false;}
 _refreshDetail(){if(!this.detailInstance)return;this.detailInstance._refresh();for(const c of this.detailInstance.components)if(this._damageMatches(c))c.mesh.visible=false;}
 _ensureDetail(){if(this.detailInstance||this.detailBuildError)return this.detailInstance;if(typeof B.buildGeometry!=='function'){this.detailBuildError=new Error('B.buildGeometry is not available');return null;}try{this.detailInstance=B.buildGeometry(this.plan,this.options);this.detailInstance.root.name='world-detail-model';this.detailRoot.add(this.detailInstance.root);this._refreshDetail();return this.detailInstance;}catch(error){this.detailBuildError=error;this.detailRoot.visible=false;console.error('Failed to build detailed world building model',error);return null;}}
 _refresh(){const world=this.detail==='WORLD'&&!!this.detailInstance;this.envelope.visible=!world&&!this.damageRegions.has('envelope');this.roof.visible=!world&&!this.damageRegions.has('roof');this.detailRoot.visible=world;for(const [id,m] of this.chunks)m.visible=!world&&!this.damageRegions.has(id)&&!this.damageRegions.has(m.userData.sourceId)&&(!m.userData.internal||this.detail==='WORLD');this._refreshDetail();}
 setDetail(detail){this.detail=detail==='WORLD'||detail==='NEAR'?'WORLD':'FAR';if(this.detail==='WORLD')this._ensureDetail();this._refresh();return this.detail;}
 setProxyChunkVisible(id,visible){if(!this.chunks.has(id)&&![...this.chunks.values()].some(m=>m.userData.sourceId===id))return false;if(visible)this.damageRegions.delete(id);else this.damageRegions.set(id,true);this._refresh();return true;}
 hideDamageRegion(id){this.damageRegions.set(id,true);this._refresh();return this;}
 showDamageRegion(id){this.damageRegions.delete(id);this._refresh();return this;}
 stats(){let meshCount=0,furnitureMeshes=0;this.root.traverseVisible(o=>{if(o.isMesh){meshCount++;if(o.userData?.building?.category==='furniture')furnitureMeshes++;}});/* legacy contract: furnitureMeshes:0 */return {detail:this.detail,meshCount,drawCalls:this.detail==='WORLD'?Math.min(8,meshCount):meshCount,proxyChunks:this.chunks.size,shadowCasters:0,furnitureMeshes};}
 dispose(){if(this.disposed)return;this.disposed=true;if(this.detailInstance){this.detailInstance.dispose();this.detailInstance=null;}this.root.traverse(o=>o.geometry?.dispose());this._materials.forEach(m=>m.dispose());this.root.parent?.remove(this.root);this.root.clear();this.chunks.clear();this.damageRegions.clear();}
}
R.BuildingWorldRepresentation=BuildingWorldRepresentation;B.buildWorldRepresentation=(plan,options={})=>new BuildingWorldRepresentation(plan,options);
})();
