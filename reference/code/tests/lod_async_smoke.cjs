// LOD appearance staging preserves the active model until an exact new variant is ready.
const fs=require('fs'),vm=require('vm'),path=require('path'),crypto=require('crypto');
const root=path.resolve(__dirname,'..');
global.window=global;global.THREE=require(root+'/tests/numeric_math.cjs');
THREE.Quaternion.prototype.dot=function(q){return this.x*q.x+this.y*q.y+this.z*q.z+this.w*q.w;};
THREE.SkinnedMesh.prototype.updateMorphTargets=function(){this.morphTargetInfluences=[];this.morphTargetDictionary={};for(const[i,a]of(this.geometry.morphAttributes.position||[]).entries()){this.morphTargetInfluences.push(0);this.morphTargetDictionary[a.name]=i;}};
global.document={createElement:()=>({getContext:()=>({createImageData:(w,h)=>({data:new Uint8ClampedArray(w*h*4)}),putImageData:()=>{}})})};
const loader=fs.readFileSync(path.join(root,'js/loader.js'),'utf8');
const modules=vm.runInNewContext(loader.match(/const local=(\[[\s\S]*?\]);/)[1]).map(f=>f.replace(/^js\//,'').replace(/\.js$/,''));
for(const file of modules)vm.runInThisContext(fs.readFileSync(path.join(root,'js',file+'.js'),'utf8'),{filename:file});
const R=RTS,side=new R.Side('LOD_TEST',R.Config.SIDES.SIDE_A),assert=(value,message)=>{if(!value)throw new Error(message);};
function signature(unit){
  const h=crypto.createHash('sha256'),add=(name,array)=>{h.update(name);h.update(Buffer.from(array.buffer,array.byteOffset,array.byteLength));};
  const surface=unit.model.surface.geometry,gear=unit.model.gear.geometry;
  for(const [name,g]of [['surface',surface],['gear',gear]]){
    add(name+':index',g.index.array);for(const key of Object.keys(g.attributes).sort())add(name+':'+key,g.attributes[key].array);
    for(const kind of Object.keys(g.morphAttributes||{}).sort())for(const attr of g.morphAttributes[kind])add(name+':'+kind+':'+attr.name,attr.array);
  }
  h.update(JSON.stringify(unit.model.surface.tags));h.update(JSON.stringify(unit.model.gear.records));
  h.update(JSON.stringify(unit.model.gear.contactIndices));h.update(JSON.stringify(unit.model.gear.headContactIndices));
  return h.digest('hex');
}
(async()=>{
  const createOptions={id:'async-created',side,seed:99173,detail:'far',equipmentOptions:{loadout:'SQUAD_LEADER'}};
  const syncCreated=new R.InfantryUnit({...createOptions,id:'sync-created'}),syncCreatedSignature=signature(syncCreated);syncCreated.dispose();R.AppearanceCache.clear();
  let createYields=0,phenotypeExpressions=0;const express=R.InfantryGenome.express;
  R.InfantryGenome.express=function(...args){phenotypeExpressions++;return express.apply(this,args);};
  let asyncCreated;
  try{asyncCreated=await R.InfantryUnit.createAsync(createOptions,async()=>{createYields++;},null,0);}
  finally{R.InfantryGenome.express=express;}
  assert(createYields>8,'async unit creation should yield while compiling body and equipment geometry');
  assert(phenotypeExpressions===1,'async unit creation should reuse the phenotype already computed for its prepared model');
  assert(signature(asyncCreated)===syncCreatedSignature,'async unit creation differs from the synchronous appearance');
  assert(asyncCreated.model.rig===asyncCreated.rig&&asyncCreated.model.detail==='far','async unit creation did not retain the prepared rig/model');
  asyncCreated.dispose();
  let cachedCreateYields=0;
  const cachedCreated=await R.InfantryUnit.createAsync({...createOptions,id:'async-cache-hit'},async()=>{cachedCreateYields++;},null,0);
  assert(cachedCreateYields===0,'async unit creation should assemble immediately from an exact appearance-cache hit');
  assert(cachedCreated.model.appearanceResources.fromCache===undefined,'cache staging marker must not escape into the live model');
  assert(signature(cachedCreated)===syncCreatedSignature,'cached async unit creation changed the appearance');
  cachedCreated.dispose();R.AppearanceCache.clear();

  const cancelledOptions={id:'async-cancelled',side,seed:99174,detail:'far',equipmentOptions:{loadout:'RIFLEMAN'}};
  let cancelledYields=0,cancelError=null;
  try{await R.InfantryUnit.createAsync(cancelledOptions,async()=>{if(++cancelledYields===3){const error=new Error('cancelled unit build');error.name='AbortError';throw error;}},null,0);}
  catch(error){cancelError=error;}
  assert(cancelError&&cancelError.name==='AbortError'&&cancelledYields===3,'async model build should cancel between bounded geometry batches');
  const retryReference=new R.InfantryUnit({...cancelledOptions,id:'retry-reference'}),retrySignature=signature(retryReference);retryReference.dispose();R.AppearanceCache.clear();
  const retry=await R.InfantryUnit.createAsync({...cancelledOptions,id:'retry-async'},async()=>{},null,0);
  assert(signature(retry)===retrySignature,'cancelled async creation leaked partial appearance/cache state into retry');
  retry.dispose();R.AppearanceCache.clear();

  const expectedUnit=new R.InfantryUnit({id:'lod-expected',side,seed:57431,detail:'world',equipmentOptions:{loadout:'SQUAD_LEADER'}});
  const expected=signature(expectedUnit),worldMorphNames=Object.keys(expectedUnit.model.mesh.morphTargetDictionary).sort();expectedUnit.dispose();R.AppearanceCache.clear();
  const unit=new R.InfantryUnit({id:'lod-staged',side,seed:57431,detail:'far',equipmentOptions:{loadout:'SQUAD_LEADER'}});
  const farMorphNames=Object.keys(unit.model.mesh.morphTargetDictionary).sort();
  assert(worldMorphNames.includes('eyelidsClose')&&worldMorphNames.includes('eyelidsArc')&&worldMorphNames.includes('neckFlex'),'near LOD must retain facial deformation channels');
  assert(farMorphNames.join(',')==='handsRelax','far LOD should retain hand relaxation while omitting subpixel face morphs');
  const oldSurface=unit.model.mesh.geometry,oldGear=unit.model.gearMesh.geometry,oldDetail=unit.model.detail;
  let checks=0,cancelled=false;
  try{await unit.setDetailAsync('world',()=>Promise.resolve(),()=>{if(++checks===8){const error=new Error('cancelled');error.name='AbortError';throw error;}},0);}
  catch(error){cancelled=error.name==='AbortError';}
  assert(cancelled,'stale LOD build should observe cancellation');
  assert(unit.model.detail===oldDetail&&unit.model.mesh.geometry===oldSurface&&unit.model.gearMesh.geometry===oldGear,'cancellation changed the active appearance');
  let yields=0;
  const changed=await unit.setDetailAsync('world',async()=>{yields++;},null,0);
  assert(changed,'successful asynchronous LOD build should commit');
  assert(yields>8,'surface and gear work should yield across bounded phases');
  assert(signature(unit)===expected,'staged appearance differs byte-for-byte from synchronous generation');
  assert(unit.model.detail==='world','committed detail does not match the request');
  unit.dispose();
  console.log('PASS async infantry LOD staging preserves old geometry on cancellation and exactly matches synchronous appearance');
})().catch(error=>{console.error(error);process.exitCode=1;});
