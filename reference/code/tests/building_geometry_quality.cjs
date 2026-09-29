'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict');
new Function('require','__dirname',fs.readFileSync(__dirname+'/building_v6.cjs','utf8').split('(async()=>')[0])(require,__dirname);
(async()=>{
  const B=RTS.Buildings;await B.initializeBuildingBackends();
  const generator=new RTS.ProceduralBuildingGenerator({presetId:'FAMILY_HOUSE',width:16,depth:14,storeyCount:2,footprintFamily:'RECT',roofFamily:'GABLE'});
  const instance=generator.generate({seed:42,features:{chimney:true,porch:true,balcony:true,dormer:true}}),plan=instance.plan;
  assert.equal(plan.spec.presetId,'FAMILY_HOUSE','constructor defaults applied');
  assert.equal(new Set(instance.components.map(c=>c.id)).size,instance.components.length,'unique component IDs');
  for(const feature of plan.features)assert(instance.components.some(c=>c.sourceId===feature.id),'rendered '+feature.type);
  assert.equal(plan.features.length,4);
  assert(instance.components.some(c=>c.category==='window'),'glass geometry');
  assert(instance.components.some(c=>c.category==='door'),'door geometry');
  assert(instance.components.some(c=>c.category==='foundation'),'anchored foundations');
  for(const c of instance.components.filter(c=>c.category==='slab'||c.category==='roof')){
    const g=c.mesh.geometry,p=g.attributes.position,n=g.attributes.normal;
    assert(p.count>=24,'solid prism, not a single surface');
    assert(n.getY(0)>0,'top faces point up');
    const b=g.boundingBox;assert(b.max.y-b.min.y>=.099,'nonzero thickness');
    assert(Array.from(p.array).every(Number.isFinite),'finite geometry');
  }
  instance.setRoofVisible(false);assert(instance.components.filter(c=>['roof','roof-detail','gable'].includes(c.category)).every(c=>!c.mesh.visible));
  instance.setRoofVisible(true);instance.setCutaway(true);
  assert(instance.components.some(c=>c.category==='wall'&&!c.mesh.visible),'cutaway changes walls');
  instance.setCutaway(false);assert(instance.components.filter(c=>c.category==='wall').every(c=>c.mesh.visible));
  instance.setVisibleFloor(0);assert(instance.components.filter(c=>c.floor===1).every(c=>!c.mesh.visible));instance.setVisibleFloor(null);
  const next=generator.generate({seed:42});
  assert.notEqual(instance.materials[0],next.materials[0],'instances do not dispose shared cached materials');
  instance.damageSphere(new THREE.Vector3(0,2,0),100,10000);assert(instance.manifest.damage.destroyed>0);assert(instance.manifest.damage.achievedPercent<=100);
  instance.resetDamage();assert.equal(instance.manifest.damage.destroyed,0);instance.dispose();next.dispose();

  // Explicit angled footprint: compare rendered long axis with source edge.
  const rotated=B.Polygon.rectangle(-8,8,-7,7);rotated.outer=rotated.outer.map(([x,z])=>[x*.8-z*.6,x*.6+z*.8]);
  const angled=generator.generate({seed:3,storeyCount:1,footprint:rotated});angled.root.updateMatrixWorld(true);
  for(const wall of angled.plan.walls.filter(w=>!w.internal)){
    const c=angled.components.find(c=>c.wallId===wall.id&&c.category==='wall'),axis=new THREE.Vector3(1,0,0).applyQuaternion(c.mesh.quaternion);
    assert(Math.abs(axis.x-wall.tangent[0])<1e-6&&Math.abs(axis.z-wall.tangent[1])<1e-6,'oriented wall follows footprint');
  }
  angled.dispose();
  console.log('PASS geometry quality: solids, features, joinery, angled walls, visibility, resource ownership');
})().catch(e=>{console.error(e);process.exitCode=1;});
