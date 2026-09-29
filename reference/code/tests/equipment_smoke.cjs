// Isolated generator smoke test, using numeric_math.cjs; NOT Three.js/WebGL.
const fs=require('fs'),vm=require('vm'),path=require('path');
const root=path.resolve(__dirname,'..');
global.window=global;global.THREE=require(root+'/tests/numeric_math.cjs');
THREE.Quaternion.prototype.dot=function(q){return this.x*q.x+this.y*q.y+this.z*q.z+this.w*q.w;};
THREE.SkinnedMesh.prototype.updateMorphTargets=function(){this.morphTargetInfluences=[];this.morphTargetDictionary={};for(const[i,a]of(this.geometry.morphAttributes.position||[]).entries()){this.morphTargetInfluences.push(0);this.morphTargetDictionary[a.name]=i;}};
global.document={createElement:()=>({getContext:()=>({createImageData:(w,h)=>({data:new Uint8ClampedArray(w*h*4)}),putImageData:()=>{}})})};
const loader=fs.readFileSync(root+'/js/loader.js','utf8');const modules=vm.runInNewContext(loader.match(/const local=(\[[\s\S]*?\]);/)[1]);
for(const file of modules)vm.runInThisContext(fs.readFileSync(root+'/'+file,'utf8'),{filename:file});
const R=RTS,side=new R.Side('SIDE_A',R.Config.SIDES.SIDE_A);
const assert=(cond,msg)=>{if(!cond)throw Error(msg);};
const EquipmentFitBase=R.EquipmentFit;let fitBuilds=0;
R.EquipmentFit=class extends EquipmentFitBase{constructor(...args){super(...args);fitBuilds++;}};
{
  const unit=new R.InfantryUnit({id:'fit-dependency-matrix',side,seed:7713,detail:'world'}),equipment=unit.equipment;
  try{
   const signature=value=>new R.EquipmentFit(unit.model.anatomy,value).surfaceSignature(),base=signature(equipment);
   const phenotype=unit.model.phenotype,bodyKey=value=>R.AppearanceCache.surfaceKey(phenotype,side,'world',value),gearKey=value=>R.AppearanceCache.gearKey(phenotype,side,value,'world'),appearanceKey=value=>R.AppearanceCache.key(phenotype,side,value,'world');
   const bodyBase=bodyKey(equipment),gearBase=gearKey(equipment),appearanceBase=appearanceKey(equipment);
   for(const [slot,id] of [['torsoBase','winter_jacket'],['legs','winter_pants'],['feet','light_boots'],['hands','gloves_full'],['head','helmet_light'],['face','balaclava']])
    {const variant=equipment.withSlot(slot,id);assert(signature(variant)!==base,'surface fit signature missed geometry-affecting '+slot+' variant '+id);assert(bodyKey(variant)!==bodyBase,'body key missed '+slot+' surface dependency');assert(gearKey(variant)!==gearBase,'gear key missed '+slot+' geometry dependency');assert(appearanceKey(variant)!==appearanceBase,'appearance key missed '+slot+' dependency');}
   for(const variant of [equipment.withSlot('back','pack_large'),equipment.withSlot('torsoArmor','plate_carrier'),equipment.clone({wear:Math.min(1,equipment.wear+.2),palette:{...equipment.palette,uniform:0x445566}})]){
    assert(signature(variant)===base,'surface fit signature included gear-only color/wear/slot data');assert(bodyKey(variant)===bodyBase,'gear-only change invalidated body geometry key');assert(gearKey(variant)!==gearBase,'gear key missed a slot/color/wear dependency');assert(appearanceKey(variant)!==appearanceBase,'combined appearance key missed gear dependency');
   }
   assert(signature(equipment.withSlot('hands','gloves_full'))===signature(equipment.withSlot('hands','gloves_winter')),
    'surface fit signature should compare effective glove geometry inputs, not unrelated glove names');
   assert(bodyKey(equipment.withSlot('hands','gloves_full'))===bodyKey(equipment.withSlot('hands','gloves_winter')),'body key should compare effective glove shape inputs, not glove item IDs');
   assert(gearKey(equipment.withSlot('hands','gloves_full'))!==gearKey(equipment.withSlot('hands','gloves_winter')),'combined gear key must retain exact item IDs');
   const animated={...phenotype,face:{...phenotype.face}},animatedNames=['blinkInterval','blinkDuration','gazeRestlessness','neutralBrow','expressionScale','eyeExpressionScale','mouthExpressionScale','browExpressionScale'];for(const name of animatedNames)animated.face[name]+=.01;
   assert(R.AppearanceCache.surfaceKey(animated,side,'world',equipment)===bodyBase,'animation-only face genes invalidated body geometry');
   assert(R.AppearanceCache.gearKey(animated,side,equipment,'world')===gearBase,'animation-only face genes invalidated gear geometry');
   const changedAnatomy={...phenotype,height:phenotype.height+.01,body:{...phenotype.body,chestWidthScale:phenotype.body.chestWidthScale+.01},face:{...phenotype.face,neutralEyeOpen:phenotype.face.neutralEyeOpen+.01}};
   assert(R.AppearanceCache.surfaceKey(changedAnatomy,side,'world',equipment)!==bodyBase,'anatomy or neutral face geometry dependency was omitted from body key');
   assert(R.AppearanceCache.gearKey(changedAnatomy,side,equipment,'world')!==gearBase,'fitted anatomy dependency was omitted from gear key');
   const changedSide={...side,uniformColor:(side.uniformColor^0x010101)&0xffffff};
   assert(R.AppearanceCache.surfaceKey(phenotype,changedSide,'world',equipment)!==bodyBase,'side color was omitted from body vertex-color key');
   assert(R.AppearanceCache.gearKey(phenotype,changedSide,equipment,'world')!==gearBase,'side color was omitted from gear vertex-color key');
   const worn=equipment.clone({wear:Math.min(1,equipment.wear+.2)}),otherDetail=R.AppearanceCache.surfaceKey(phenotype,side,'far',equipment);
   assert(otherDetail!==bodyBase,'detail tier was omitted from surface dependencies');
   assert(R.AppearanceCache.gearKey(phenotype,side,equipment,'far')!==gearBase,'detail tier was omitted from gear dependencies');
   assert(R.AppearanceCache.key(phenotype,side,worn,'world')===JSON.stringify(['appearance-3',bodyKey(worn),gearKey(worn)]),'combined key did not compose the explicit body and gear dependency keys');
   console.log('PASS complete appearance dependency matrix separates reusable body geometry from gear inputs');
 }finally{unit.dispose();}
}
{
 const face=R.InfantryGenome.express(R.InfantryGenome.create(8841)).face,animated=['blinkInterval','blinkDuration','gazeRestlessness','neutralBrow','expressionScale','eyeExpressionScale','mouthExpressionScale','browExpressionScale'];
 const changed={...face};for(const name of animated)changed[name]+=0.01;
 assert(JSON.stringify(R.AppearanceCache.geometryFaceGenes(face))===JSON.stringify(R.AppearanceCache.geometryFaceGenes(changed)),'geometry face key should omit animation-only values');
 changed.neutralEyeOpen+=0.01;
 assert(JSON.stringify(R.AppearanceCache.geometryFaceGenes(face))!==JSON.stringify(R.AppearanceCache.geometryFaceGenes(changed)),'geometry face key must retain neutral eye aperture metadata');
}
const summary=[];
{
 R.InfantryGear.clearSlotCache();
 const unit=new R.InfantryUnit({id:'gear-color-scratch',side,seed:9137,equipmentOptions:{loadout:'SQUAD_LEADER'},detail:'world'}),original=R.SurfaceBuilder.prototype.vertex,seen=new WeakMap();
 R.InfantryGear.clearSlotCache();
 let reusedValues=0,expected=null,expectedIndex=-1,geometry=null;
 R.SurfaceBuilder.prototype.vertex=function(p,weights,color,...rest){
  if(Array.isArray(color)){
   const previous=seen.get(color),index=this.positions.length/3;
   if(previous&&(previous[0]!==color[0]||previous[1]!==color[1]||previous[2]!==color[2])){reusedValues++;if(!expected){expected=color.slice();expectedIndex=index;}}
   seen.set(color,color.slice());
  }
  return original.call(this,p,weights,color,...rest);
 };
 try{geometry=R.InfantryGear.build(unit.rig,side,unit.equipment,'world',unit.model.gear.fit).geometry;}
 finally{R.SurfaceBuilder.prototype.vertex=original;unit.dispose();}
 try{
  assert(reusedValues>0,'gear color generation did not reuse its scratch color array');
  const values=geometry.attributes.color.array,offset=expectedIndex*3;
  for(let k=0;k<3;k++)assert(values[offset+k]===Math.fround(expected[k]),'scratch mutation changed an already emitted vertex color');
  console.log('PASS gear color scratch is reused and copied synchronously',JSON.stringify({reusedValues,checkedVertex:expectedIndex}));
 }finally{geometry.dispose();}
}
{
 let seed=9821;while(R.InfantryGenome.express(R.InfantryGenome.create(seed)).face.hairStyle===0)seed++;
 const originalHairVisible=R.EquipmentFit.prototype.hairVisible;let hairVisibilityChecks=0;
 R.EquipmentFit.prototype.hairVisible=function(point){hairVisibilityChecks++;return originalHairVisible.call(this,point);};
 const helmet=new R.InfantryUnit({id:'hair-helmet',side,seed,equipmentOptions:{loadout:'RIFLEMAN'},detail:'world'});
 const cap=new R.InfantryUnit({id:'hair-cap',side,seed,equipmentOptions:{loadout:'SCOUT'},detail:'world'});
 try{
  cap.setEquipment(cap.equipment.withSlot('head',null));
  const generatedVisibilityChecks=hairVisibilityChecks;
  assert(generatedVisibilityChecks===3*(12*40+1+40),'hair visibility must be checked only once per candidate vertex, not again for each triangle ('+generatedVisibilityChecks+' checks)');
  const hairIndices=unit=>new Set(unit.model.surface.tags.hair||[]),helmetHair=hairIndices(helmet),capHair=hairIndices(cap);
  assert(capHair.size>0,'hair coverage test requires a non-bald genome');
  assert(helmetHair.size<capHair.size,'helmet-covered hair vertices were still emitted');
  for(const [unit,indices] of [[helmet,helmetHair],[cap,capHair]]){
   const position=unit.model.surface.geometry.attributes.position.array,H=unit.model.anatomy.height;
   for(const index of indices){const p=new THREE.Vector3().fromArray(position,index*3).multiplyScalar(1/H);assert(unit.model.surface.clothingFit.hairVisible(p),'emitted hair vertex is hidden by headgear');}
   const all=unit.model.surface.geometry.index.array;for(const index of all)assert(index>=0,'hair omission emitted a negative geometry index');
  }
  console.log('PASS covered hair vertices are not emitted and visibility is checked before triangle emission',JSON.stringify({helmet:helmetHair.size,cap:capHair.size,visibilityChecks:generatedVisibilityChecks}));
 }finally{R.EquipmentFit.prototype.hairVisible=originalHairVisible;helmet.dispose();cap.dispose();}
}
for(const [n,role]of Object.keys(R.InfantryLoadouts).entries()){
 const beforeFitBuilds=fitBuilds;
 const unit=new R.InfantryUnit({id:'test',side,seed:1001+n,equipmentOptions:{loadout:role},detail:'world'});
 assert(fitBuilds-beforeFitBuilds===1,role+' should build one EquipmentFit shared by the surface and gear');
 assert(unit.model.surface.clothingFit===unit.model.gear.fit,role+' surface and gear do not share EquipmentFit');
 for(const [mesh,label]of [[unit.model.mesh,'body'],[unit.model.gearMesh,'gear']]){
  assert(!Array.isArray(mesh.material),role+' '+label+' must render through one region-aware material');
  const regions=mesh.geometry.attributes.materialRegion,indices=mesh.geometry.index.array;
  assert(regions&&regions.array instanceof Uint8Array&&regions.count===mesh.geometry.attributes.position.count,role+' '+label+' material regions must be compact per-vertex bytes');
  for(const group of mesh.geometry.groups)for(let i=group.start;i<group.start+group.count;i++)assert(regions.array[indices[i]]===group.materialIndex,role+' '+label+' has a cross-material vertex that cannot use region shading');
 }
 const data=unit.model.gear.geometry.attributes;
 assert(Array.from(data.position.array).every(Number.isFinite),role+' positions finite');
 assert(unit.model.gear.triangles>0,role+' gear has triangles');
 for(let i=0;i<data.skinWeight.count;i++){
   const a=data.skinWeight.array,offset=4*i;
   assert(Math.abs(a[offset]+a[offset+1]+a[offset+2]+a[offset+3]-1)<1e-5,role+' weights');
 }
 unit.setState('CROUCH');unit.step(1/60,R.FlatSurface,true);unit.render(1);
 const originalGenome=JSON.stringify(unit.genome),phase=unit.animator.phase,oldRig=unit.rig;
 unit.equip('head','beanie');unit.equip('back','pack_radio');
 assert(JSON.stringify(unit.genome)===originalGenome,'identity changed');assert(unit.rig===oldRig,'rig changed');assert(unit.animator.phase===phase,'phase changed');
 assert(unit.model.gearMesh.skeleton===unit.model.mesh.skeleton,'different skeleton');
 const next=unit.equipment.clone();assert(JSON.stringify(unit.equipment.toJSON())===JSON.stringify(next.toJSON()),'non-deterministic clone');
 const sameEquipment=unit.equipment.clone({loadout:unit.equipment.loadout}),sameSurface=unit.model.surface.geometry,sameGear=unit.model.gear.geometry,sameSupports=unit.animator.supportIndices,fitsBeforeNoop=fitBuilds;
 let supportRebuilds=0,contactReleases=0,weaponReconciles=0;
 const buildSupports=unit.animator.buildSupports,releaseContacts=unit.animator.releaseContacts,reconcile=unit.weapons.reconcile;
 unit.animator.buildSupports=function(){supportRebuilds++;return buildSupports.call(this);};
 unit.animator.releaseContacts=function(){contactReleases++;return releaseContacts.call(this);};
 unit.weapons.reconcile=function(...args){weaponReconciles++;return reconcile.apply(this,args);};
 unit.setEquipment(sameEquipment);
 assert(unit.equipment===sameEquipment,'equipment metadata instance was not updated');
 assert(fitBuilds===fitsBeforeNoop,'identical appearance recomputed EquipmentFit');
 assert(unit.model.surface.geometry===sameSurface&&unit.model.gear.geometry===sameGear,'identical appearance rebuilt geometry');
 assert(unit.animator.supportIndices===sameSupports&&supportRebuilds===0&&contactReleases===0,'identical appearance rebuilt contact state');
 assert(weaponReconciles===0,'identical equipment reconciled unchanged weapons');
 unit.animator.buildSupports=buildSupports;unit.animator.releaseContacts=releaseContacts;unit.weapons.reconcile=reconcile;
 if(n===0){
   const face={...unit.model.phenotype.face},animated=['blinkInterval','blinkDuration','gazeRestlessness','neutralBrow','expressionScale','eyeExpressionScale','mouthExpressionScale','browExpressionScale'];
   for(const name of animated)face[name]+=0.01;
   unit.model.phenotype={...unit.model.phenotype,face};
   const oldSurface=unit.model.surface.geometry,createSurface=R.InfantrySurface.create,takeAppearance=R.AppearanceCache.take;let surfaceBuilds=0;
   R.InfantrySurface.create=function(...args){surfaceBuilds++;return createSurface.apply(this,args);};R.AppearanceCache.take=()=>null;
   try{unit.setEquipment(unit.equipment.clone({wear:Math.min(1,unit.equipment.wear+.123)}));}
   finally{R.InfantrySurface.create=createSurface;R.AppearanceCache.take=takeAppearance;}
   assert(surfaceBuilds===0&&unit.model.surface.geometry===oldSurface,'animation-only face genes must not prevent reuse of unchanged surface geometry');
 }
 summary.push({role,stage:'after head=beanie and back=pack_radio overrides',weightKg:unit.equipment.totalWeightKg,baseTriangles:unit.model.surface.triangles,gearTriangles:unit.model.gear.triangles});
 unit.dispose();console.log('PASS',role);
}
// Schema without any humanoid slots still has an Equipment component.
const generic=new R.Unit({id:'generic',seed:8,side,category:'VEHICULAR',type:'TEST'});
assert(generic.equipment instanceof R.Equipment,'base Unit lacks inventory');assert(generic.equipment.count===0,'vehicle forced into infantry loadout');
const eq=new R.Equipment({unitSeed:1,slotSchema:R.EquipmentSlots,loadout:'SCOUT'});
try{eq.withSlot('head','pack_large');throw Error('incompatible accepted');}catch(e){assert(!e.message.includes('incompatible accepted'),'invalid slot not rejected');}
fs.mkdirSync(path.join(__dirname,'out'),{recursive:true});
fs.writeFileSync(path.join(__dirname,'out/equipment_smoke.json'),JSON.stringify({version:R.Config.VERSION,environment:'Node + bundled numeric_math.cjs. Not a WebGL/browser test.',profiles:summary},null,2));
console.log('PASS universal component, deterministic selection, slot validation, identity/phase preservation');
(async()=>{
 const unit=new R.InfantryUnit({id:'async-gear',side,seed:7741,equipmentOptions:{loadout:'SQUAD_LEADER'},detail:'world'});
 const interleavedUnit=new R.InfantryUnit({id:'interleaved-gear',side,seed:7742,equipmentOptions:{loadout:'MEDIC'},detail:'world'});
 const signature=gear=>{
   const hash=require('crypto').createHash('sha256'),g=gear.geometry;
   const add=(name,array)=>{hash.update(name);hash.update(Buffer.from(array.buffer,array.byteOffset,array.byteLength));};
   add('index',g.index.array);for(const name of Object.keys(g.attributes).sort())add(name,g.attributes[name].array);
   hash.update(JSON.stringify(g.groups));
   hash.update(JSON.stringify(gear.records));hash.update(JSON.stringify(gear.contactIndices));hash.update(JSON.stringify(gear.headContactIndices));
   return hash.digest('hex');
 };
 const cachedBuilder=new R.GearGeometry(unit.rig,'world'),referenceBuilder=new R.GearGeometry(unit.rig,'world'),weightCache=new WeakMap();
 let cacheHits=0,cacheSets=0;
 cachedBuilder._frozenWeightCache={get(key){const value=weightCache.get(key);if(value)cacheHits++;return value;},set(key,value){cacheSets++;weightCache.set(key,value);}};
 const center=new THREE.Vector3(.1,.5,0),size=new THREE.Vector3(.08,.06,.04),color=[.24,.31,.18],frozenWeight=R.SurfaceBuilder.staticWeights({chest:1});
 cachedBuilder.box(center,size,frozenWeight,color);referenceBuilder.box(center,size,{chest:1},color);
 const finish=builder=>{const steps=builder.finishSteps();let result=steps.next();while(!result.done)result=steps.next();return result.value.geometry;};
 const cachedGeometry=finish(cachedBuilder),referenceGeometry=finish(referenceBuilder);
 assert(cacheHits>0&&cacheSets===1,'immutable bone weights did not use the per-builder cache');
 assert(cachedGeometry.groups.length===1&&cachedGeometry.groups.every(group=>group.count>0),'builder retained empty material groups');
 for(const name of Object.keys(referenceGeometry.attributes))assert(JSON.stringify(Array.from(cachedGeometry.attributes[name].array))===JSON.stringify(Array.from(referenceGeometry.attributes[name].array)),'cached immutable weights changed '+name);
 assert(JSON.stringify(Array.from(cachedGeometry.index.array))===JSON.stringify(Array.from(referenceGeometry.index.array)),'cached immutable weights changed indices');
 cachedGeometry.dispose();referenceGeometry.dispose();
 let yields=0;
 const [asyncGear,interleavedGear]=await Promise.all([
   R.InfantryGear.buildAsync(unit.rig,side,unit.equipment,'world',unit.model.gear.fit,async()=>{yields++;},null,0),
   R.InfantryGear.buildAsync(interleavedUnit.rig,side,interleavedUnit.equipment,'world',interleavedUnit.model.gear.fit,async()=>{},null,0)
 ]);
 assert(yields>6,'async gear generation should yield after equipment components and contact bounds');
 assert(signature(asyncGear)===signature(unit.model.gear),'async gear differs byte-for-byte from synchronous build');
 assert(signature(interleavedGear)===signature(interleavedUnit.model.gear),'interleaved gear build corrupted per-geometry scratch');
 const beforeReuse=R.InfantryGear.slotCacheStats();
 const reused=R.InfantryGear.build(unit.rig,side,unit.equipment,'world',unit.model.gear.fit),afterReuse=R.InfantryGear.slotCacheStats();
 assert(signature(reused)===signature(unit.model.gear),'slot cache hit changed the composed gear buffers');
 assert(afterReuse.hits-beforeReuse.hits===Object.values(unit.equipment.slots).filter(Boolean).length,'identical build did not reuse each equipped slot part');
 reused.geometry.dispose();
 R.InfantryGear.clearSlotCache();
 const baseline=R.InfantryGear.build(unit.rig,side,unit.equipment,'world',unit.model.gear.fit),baselineSignature=signature(baseline);baseline.geometry.dispose();
 const originalPerformance=global.performance,clockReads={value:0};global.performance={now(){clockReads.value++;return clockReads.value;}};
 const zeroBudgetYields={value:0},zeroBudget=await R.InfantryGear.buildAsync(unit.rig,side,unit.equipment,'world',unit.model.gear.fit,async()=>{zeroBudgetYields.value++;},null,0);
 assert(clockReads.value===0,'zero-budget gear generation yields without reading the clock');
 const timedYields={value:0},timed=await R.InfantryGear.buildAsync(unit.rig,side,unit.equipment,'world',unit.model.gear.fit,async()=>{timedYields.value++;},null,1);
 assert(timedYields.value>0,'positive-budget gear generation stopped yielding');
 assert(clockReads.value<timedYields.value*2+4,'positive-budget gear generation samples the clock less often than every checkpoint');
 assert(signature(zeroBudget)===baselineSignature&&signature(timed)===baselineSignature,'clock checkpoint policy changed gear output');
 zeroBudget.geometry.dispose();timed.geometry.dispose();global.performance=originalPerformance;
 const changedEquipment=unit.equipment.withSlot('face','goggles'),changedFit=new R.EquipmentFit(unit.model.anatomy,changedEquipment),beforeSlotChange=R.InfantryGear.slotCacheStats();
 const changed=R.InfantryGear.build(unit.rig,side,changedEquipment,'world',changedFit),afterSlotChange=R.InfantryGear.slotCacheStats();
 assert(afterSlotChange.misses-beforeSlotChange.misses===1,'changing one fit-independent slot rebuilt unchanged gear parts ('+(afterSlotChange.misses-beforeSlotChange.misses)+' misses, '+(afterSlotChange.hits-beforeSlotChange.hits)+' hits)');
 const changedAgain=R.InfantryGear.build(unit.rig,side,changedEquipment,'world',changedFit);
 assert(signature(changedAgain)===signature(changed),'changed-slot cache reuse changed composed geometry/contact metadata');
 changed.geometry.dispose();changedAgain.geometry.dispose();
 console.log('PASS slot geometry cache reuses unchanged parts and recomposes one SkinnedMesh',{reusedSlots:afterReuse.hits-beforeReuse.hits,rebuiltSlots:afterSlotChange.misses-beforeSlotChange.misses});
 let checks=0,cancelled=false;
 try{await R.InfantryGear.buildAsync(unit.rig,side,unit.equipment,'world',unit.model.gear.fit,async()=>{},()=>{if(++checks===4){const error=new Error('cancelled');error.name='AbortError';throw error;}},0);}
 catch(error){cancelled=error.name==='AbortError';}
 assert(cancelled,'async gear generation should observe cancellation between bounded steps');
 asyncGear.geometry.dispose();interleavedGear.geometry.dispose();unit.dispose();interleavedUnit.dispose();
 console.log('PASS infantry gear sync/async and interleaved builds preserve exact buffers and async phases yield/cancel safely');
})().catch(error=>{console.error(error);process.exitCode=1;});
