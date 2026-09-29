const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),root=path.resolve(__dirname,'..');
global.window=global;global.THREE=require(path.join(root,'tools/destruction/node_modules/three'));
for(const f of ['js/vendor/buildings-6.0.0.js','js/buildings/buildingCatalog.js','js/buildings/buildingRandom.js','js/buildings/buildingBackends.js','js/buildings/buildingPolygon.js','js/buildings/buildingSpec.js','js/buildings/buildingFootprints.js','js/buildings/buildingPrograms.js','js/buildings/buildingSolver.js','js/buildings/buildingAssemblies.js','js/buildings/buildingPlan.js','js/buildings/buildingCollisionProxy.js','js/buildings/buildingWorldRepresentation.js','js/buildings/buildingRuntime.js','js/buildings/buildingDamageChunkManager.js','js/buildings/buildingSpatialIndex.js','js/buildings/buildingRepresentationManager.js'])vm.runInThisContext(fs.readFileSync(path.join(root,f),'utf8'),{filename:f});
(async()=>{
await RTS.Buildings.initializeBuildingBackends();
RTS.Config={WORLD_BUILDING_RUNTIME:{buildingSectorSize:32,worldDistance:100,maxDetailedBuildings:1}};
const B=RTS.Buildings;
function fixture(polygon){const walls=[];for(const ring of [polygon.outer,...polygon.holes])for(let i=0;i<ring.length;i++){const a=ring[i],b=ring[(i+1)%ring.length];walls.push({id:'w'+walls.length,edge:{a,b},length:Math.hypot(b[0]-a[0],b[1]-a[1]),height:3,thickness:.2,floor:0,openings:[]});}const t=B.Polygon.triangulate(polygon),faces=[];for(let i=0;i<t.indices.length;i+=3)faces.push({id:'r'+i,points:t.indices.slice(i,i+3).map(j=>[t.vertices[2*j],3+t.vertices[2*j]*.1,t.vertices[2*j+1]])});return {schema:'rts.building-plan/4',spec:{seed:1,storeys:{count:1,floorHeight:3}},storeys:[{elevation:0}],footprint:{polygon},walls,slabs:[{id:'floor',polygon,y:0}],roof:{faces}};}
const courtyard=B.Polygon.canonical({outer:[[0,0],[6,0],[6,6],[0,6]],holes:[[[2,2],[4,2],[4,4],[2,4]]]}),ell=B.Polygon.canonical({outer:[[0,0],[6,0],[6,2],[2,2],[2,6],[0,6]],holes:[]});
for(const [polygon,empty,solid] of [[courtyard,[3,3],[1,1]],[ell,[4,4],[1,4]]]){
 const plan=fixture(polygon),rep=new RTS.BuildingWorldRepresentation(plan),scene=new THREE.Scene();scene.add(rep.root);
 const cast=p=>{scene.updateMatrixWorld(true);return new THREE.Raycaster(new THREE.Vector3(p[0],10,p[1]),new THREE.Vector3(0,-1,0)).intersectObject(rep.root,true);};
 for(const detail of ['FAR','WORLD','FAR','WORLD']){rep.setDetail(detail);assert.equal(cast(empty).length,0,detail+' leaves void empty');const hits=cast(solid);assert(hits.length>0);assert(Math.abs(hits[0].point.y-(3+solid[0]*.1))<1e-5);assert(rep.envelope.visible,'WORLD retains walls');}
 const id='wall:w0:0';rep.hideDamageRegion(id);rep.setDetail('FAR');rep.setDetail('WORLD');assert.equal(rep.chunks.get(id).visible,false);rep.showDamageRegion(id);assert(rep.chunks.get(id).visible);
 let geometries=0,materials=0;const count=rep.chunks.size;rep.root.traverse(o=>o.geometry?.addEventListener('dispose',()=>geometries++));rep._materials.forEach(m=>m.addEventListener('dispose',()=>materials++));rep.dispose();rep.dispose();assert.equal(geometries,count);assert.equal(materials,2);assert.equal(scene.children.length,0);
}
const runtime=new RTS.BuildingRuntime(fixture(ell)),parent=new THREE.Group();parent.add(runtime.root);parent.position.set(20,2,30);parent.rotation.y=Math.PI/3;
const local=[1,1,1];assert(runtime.worldToLocal(runtime.localToWorld(local)).every((v,i)=>Math.abs(v-local[i])<1e-6));
const hit=runtime.queryLogicalHit(runtime.localToWorld([1,10,1]),runtime.localToWorld([1,0,1]))[0];assert(hit?.faceId);
const camera=new THREE.PerspectiveCamera(),manager=new RTS.BuildingRepresentationManager(camera);manager.add(runtime);manager.add(runtime);assert.equal(manager.runtimes.length,1);
const center=runtime.localToWorld([3,1.5,3]);camera.position.set(center[0]+80,center[1],center[2]);manager.update(0);assert.equal(runtime.representation.detail,'WORLD');
camera.position.x=center[0]+105;manager.update(100);assert.equal(runtime.representation.detail,'WORLD','exit hysteresis');
camera.position.x=center[0]+111;manager.update(200);assert.equal(runtime.representation.detail,'FAR');
camera.position.x=center[0]+95;manager.update(300);assert.equal(runtime.representation.detail,'FAR','entry hysteresis');
camera.position.x=center[0]+80;manager.update(400);assert.equal(runtime.representation.detail,'WORLD');manager.dispose();manager.dispose();assert.equal(parent.children.length,0);assert.equal(manager.index.stats().buildings,0);
console.log('PASS world: courtyard, L notch, sloped roof, transforms, transitions and disposal');
})().catch(e=>{console.error(e);process.exit(1);});
