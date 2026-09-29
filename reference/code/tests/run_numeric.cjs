// Isolated geometry/mathematics checks. This is NOT Three.js or WebGL.
const fs=require('fs'),path=require('path'),vm=require('vm');
const root=path.resolve(__dirname,'..');
global.window=global;global.THREE=require('./numeric_math.cjs');
THREE.Quaternion.prototype.dot=function(q){return this.x*q.x+this.y*q.y+this.z*q.z+this.w*q.w;};
THREE.SkinnedMesh.prototype.updateMorphTargets=function(){this.morphTargetInfluences=[];this.morphTargetDictionary={};for(const [i,a] of (this.geometry.morphAttributes.position||[]).entries()){this.morphTargetInfluences.push(0);this.morphTargetDictionary[a.name]=i;}};
global.document={createElement:()=>({getContext:()=>({createImageData:(w,h)=>({data:new Uint8ClampedArray(w*h*4)}),putImageData:()=>{}})})};
const loader=fs.readFileSync(path.join(root,'js/loader.js'),'utf8');
const modules=vm.runInNewContext(loader.match(/const local=(\[[\s\S]*?\]);/)[1]).map(f=>f.replace(/^js\//,'').replace(/\.js$/,''));
modules.forEach(f=>vm.runInThisContext(fs.readFileSync(path.join(root,'js',f+'.js'),'utf8'),{filename:f}));
vm.runInThisContext(fs.readFileSync(path.join(__dirname,'check_core.js'),'utf8'));
const R=RTS,side=new R.Side('SIDE_A',R.Config.SIDES.SIDE_A),report={version:R.Config.VERSION,environment:'Node + numeric_math.cjs; NOT Three.js/WebGL',browserExecuted:false,results:[]};
function test(name,fn){try{const details=fn();report.results.push({name,ok:true,details});console.log('PASS',name);}catch(e){report.results.push({name,ok:false,error:e.stack});console.error('FAIL',name,e.message);}}
for(const seed of [0,1001,1003,2002,2003,63])test('Anatomy / eye aperture / closed lids '+seed,()=>{
 const model=R.InfantryFactory.createModel(side,R.InfantryGenome.express(R.InfantryGenome.create(seed)),'world');
 try{return R.CoreChecks.verify(model);}finally{model.dispose();}
});
for(const extreme of [0,1])test('Extreme genes '+extreme,()=>{
 const base=R.InfantryGenome.create(7),overrides={};for(const group of ['body','face'])for(const key of Object.keys(base[group]))overrides[group+'.'+key]=extreme;
 const model=R.InfantryFactory.createModel(side,R.InfantryGenome.express(R.InfantryGenome.withOverrides(base,overrides)),'high');
 try{return R.CoreChecks.verify(model);}finally{model.dispose();}
});
test('Variation zero',()=>{const g=R.InfantryGenome.applyVariation(R.InfantryGenome.create(1003),0);R.CoreChecks.assert(g.heightGene===.5&&Object.values(g.body).every(v=>v===.5)&&Object.values(g.face).every(v=>v===.5),'Zero not preserved');return true;});
test('Face front projection matches the reference point path',()=>{
 let samples=0,maxError=0;
 for(const seed of [0,1003,63,2003]){
  const phenotype=R.InfantryGenome.express(R.InfantryGenome.create(seed)),face=new R.FaceAnatomy(phenotype.face,phenotype.body);
  for(let yi=0;yi<=48;yi++){
   const y=.875+yi*.0027,section=face.section(y),radius=section[1];
   for(let xi=-10;xi<=10;xi++){
    const x=radius*xi*.105,theta=Math.asin(R.Math.clamp(x/Math.max(.00001,radius),-.9999,.9999));
    const reference=face.pointFromSection(y,theta,section,new THREE.Vector3()).z,actual=face.frontZFromSection(x,y,section),error=Math.abs(reference-actual);
    maxError=Math.max(maxError,error);samples++;
    R.CoreChecks.assert(error<1e-14,'Direct face front projection differs from point reference by '+error);
   }
  }
 }
 const face=new R.FaceAnatomy(R.InfantryGenome.express(R.InfantryGenome.create(444)).face,R.InfantryGenome.express(R.InfantryGenome.create(444)).body),section=face.section,scratchCalls=[];
 face.section=function(y,out){scratchCalls.push(out||null);return section.call(this,y,out);};
 const first=face.section(.94),second=face.section(.94);
 R.CoreChecks.assert(first!==second,'public section() calls must still return independent arrays');
 face.point(.94,0);face.point(.94,.2);
 R.CoreChecks.assert(scratchCalls[2]===scratchCalls[3]&&scratchCalls[2]!==null,'point() did not reuse its private section scratch');
 const pointSection=[0,0,0,0],pointOut=new THREE.Vector3(),pointY=.941,pointTheta=.37;
 const point=face.point(pointY,pointTheta,pointOut,pointSection),expectedSection=face.section(pointY);
 R.CoreChecks.assert(point===pointOut&&scratchCalls[4]===pointSection,'point() did not fill and use its caller-provided section scratch');
 R.CoreChecks.assert(pointSection.every((value,index)=>value===expectedSection[index]),'point() caller-provided section differs from section()');
 const referencePoint=face.pointFromSection(pointY,pointTheta,expectedSection,new THREE.Vector3());
 R.CoreChecks.assert(point.x===referencePoint.x&&point.y===referencePoint.y&&point.z===referencePoint.z,'point() caller-provided section changed point coordinates');
 face.frontZ(0,.94);face.frontZ(.01,.94);
 R.CoreChecks.assert(scratchCalls[6]===scratchCalls[7]&&scratchCalls[6]!==null,'frontZ() did not reuse its private section scratch');
 face.normal(0,.94);face.normal(.01,.94);
 R.CoreChecks.assert(scratchCalls.slice(8,11).every((value,index)=>value&&value===scratchCalls[index+11]),'normal() did not reuse its three private section samples');
 face.section(.94);let cursorValue=face._sectionCursor,cursorWrites=0;
 Object.defineProperty(face,'_sectionCursor',{configurable:true,get(){return cursorValue;},set(value){cursorWrites++;cursorValue=value;}});
 face.section(.94001);R.CoreChecks.assert(cursorWrites===0,'same-interval query repeated the binary-search cursor write');
 face.section(.97);R.CoreChecks.assert(cursorWrites>0,'distant query did not fall back to binary interval search');
 return {samples,maxError,scratchPointFrontNormal:true,sameIntervalReusesCursor:true,binaryFallback:true};
});
test('Face anatomy section cache is bounded, exact and returns independent samples',()=>{
 let samples=0,hits=0;
 for(const seed of [0,1003,2003]){
  const phenotype=R.InfantryGenome.express(R.InfantryGenome.create(seed)),cached=new R.FaceAnatomy(phenotype.face,phenotype.body),reference=new R.FaceAnatomy(phenotype.face,phenotype.body);
  reference._sectionCacheLimit=0;
  for(let i=0;i<600;i++){
   const y=.82+i*.00031,actual=cached.section(y),expected=reference.section(y);
   R.CoreChecks.assert(actual.every((v,k)=>v===expected[k]),'cached section changed profile at '+seed+'/'+y);
   samples++;
  }
  const scratch=[0,0,0,0],first=cached.section(.941,scratch),entries=cached._sectionCache.size;
  const second=cached.section(.941);
  R.CoreChecks.assert(first===scratch&&second!==first&&second.every((v,k)=>v===first[k]),'section cache exposed mutable cached values');
  R.CoreChecks.assert(cached._sectionCache.size<=cached._sectionCacheLimit&&entries<=cached._sectionCacheLimit,'section cache exceeded its bound');
  R.CoreChecks.assert(cached._sectionCacheHits>0,'repeated exact section query missed the cache');
  hits+=cached._sectionCacheHits;
 }
 return {samples,cacheHits:hits,entriesPerFaceLimit:256,exact:true};
});
test('Face skin weight channels match the reference maps exactly',()=>{
 let samples=0;
 for(const seed of [0,63,1003,2003]){
  const phenotype=R.InfantryGenome.express(R.InfantryGenome.create(seed)),face=new R.FaceAnatomy(phenotype.face,phenotype.body);
  for(let yi=0;yi<=32;yi++)for(let ai=0;ai<48;ai++){
   const y=.828+yi*.0053,theta=-Math.PI+ai*Math.PI/24,p=face.point(y,theta),expected=face.skinWeights(p),channels=face.skinWeightChannels(p),actual={};
   for(let i=0;i<channels.count;i++)actual[channels.names[channels.indices[i]]]=channels.values[i];
   const names=new Set([...Object.keys(expected),...Object.keys(actual)]);
   for(const name of names)R.CoreChecks.assert((expected[name]||0)===(actual[name]||0),'Weight channel mismatch for '+name+' at '+seed+'/'+y+'/'+theta);
   samples++;
  }
 }
 return {samples,exact:true};
});
test('CPU skin palette reuses unchanged transforms and invalidates bone/ancestor edits',()=>{
 const model=R.InfantryFactory.createModel(side,R.InfantryGenome.express(R.InfantryGenome.create(91)),'world');
 try{
  const parent=new THREE.Group();parent.add(model.root);model.prepareSkinPalette();
  let updates=0,branchUpdates=0;const original=model.root.updateMatrixWorld,trackedBones=[];
  model.root.updateMatrixWorld=function(force){updates++;return original.call(this,force);};
  for(const bone of model.rig.bones){const update=bone.updateWorldMatrix;bone.updateWorldMatrix=function(parents,children){if(parents===true&&children===true)branchUpdates++;return update.call(this,parents,children);};trackedBones.push(()=>{bone.updateWorldMatrix=update;});}
  model.prepareSkinPalette();R.CoreChecks.assert(updates===0,'unchanged pose rebuilt palette');
  model.prepareSkinPalette();R.CoreChecks.assert(updates===0,'unchanged pose rebuilt palette');
  const morphIndex=model.mesh.morphTargetDictionary.eyelidsClose,morph=model.mesh.geometry.morphAttributes.position[morphIndex].array;
  const lidIndices=model.surface.tags['lidUpper.L'];let vertex=-1;
  for(const index of lidIndices){const o=index*3;if(morph[o]||morph[o+1]||morph[o+2]){vertex=index;break;}}
  R.CoreChecks.assert(vertex>=0,'test requires an eyelid vertex affected by the morph');
  const before=new THREE.Vector3(),after=new THREE.Vector3();model.skinVertex(vertex,before);model.mesh.morphTargetInfluences[morphIndex]=1;
  model.prepareSkinPalette();model.skinVertex(vertex,after);
  R.CoreChecks.assert(updates===0,'morph-only edit rebuilt bone palette');
  R.CoreChecks.assert(Math.abs(before.x-after.x)+Math.abs(before.y-after.y)+Math.abs(before.z-after.z)>1e-8,'vertex skinning ignored the live morph influence');
  model.mesh.morphTargetInfluences[morphIndex]=0;
  const influences=model.mesh.morphTargetInfluences.slice(),skinWeights=model.mesh.geometry.attributes.skinWeight.array,boneOperations=Array.from({length:4},(_,k)=>skinWeights[vertex*4+k]>0?1:0).reduce((a,b)=>a+b,0),originalAdd=THREE.Vector3.prototype.addScaledVector;
  let scaledAdds=0;THREE.Vector3.prototype.addScaledVector=function(vector,scale){scaledAdds++;return originalAdd.call(this,vector,scale);};
  try{
    for(let i=0;i<model.mesh.morphTargetInfluences.length;i++)model.mesh.morphTargetInfluences[i]=0;
    model.skinVertex(vertex,new THREE.Vector3());R.CoreChecks.assert(scaledAdds===boneOperations,'zero morph weights still performed per-vertex blend operations');
    scaledAdds=0;const activeCount=Math.min(3,model.mesh.morphTargetInfluences.length);
    for(let i=0;i<activeCount;i++)model.mesh.morphTargetInfluences[i]=.15*(i+1);
    model.skinVertex(vertex,new THREE.Vector3());R.CoreChecks.assert(scaledAdds===boneOperations+activeCount,'active morph targets were omitted from the sparse per-vertex blend');
  }finally{
    THREE.Vector3.prototype.addScaledVector=originalAdd;
    for(let i=0;i<influences.length;i++)model.mesh.morphTargetInfluences[i]=influences[i];
  }
  model.prepareSkinPalette();
  const bone=model.rig.bones[model.rig.bones.length-1],oldY=bone.position.y;bone.position.y+=.001;model.prepareSkinPalette();
  R.CoreChecks.assert(updates===0&&branchUpdates===1,'bone mutation must refresh a sparse branch instead of the full root');
  bone.position.y=oldY;model.prepareSkinPalette();R.CoreChecks.assert(updates===0&&branchUpdates===2,'restored bone transform did not refresh its sparse branch');
  parent.position.x+=.25;model.prepareSkinPalette();R.CoreChecks.assert(updates===0,'ancestor transform should use an ancestor branch update');
  parent.remove(model.root);model.prepareSkinPalette();R.CoreChecks.assert(updates===1,'detaching from an ancestor must use the full hierarchy fallback');
  parent.position.x+=1;model.prepareSkinPalette();R.CoreChecks.assert(updates===1,'detached ancestor remained in the palette dependency chain');
  const replacement=new THREE.Group();replacement.add(model.root);model.prepareSkinPalette();R.CoreChecks.assert(updates===2,'reparenting to a new ancestor failed to invalidate palette');
  trackedBones.forEach(restore=>restore());
  return {unchangedRebuilds:0,sparseBoneBranchUpdates:branchUpdates,hierarchyFallbacks:updates};
 }finally{model.dispose();}
});
test('Crouch/prone/run finite transforms + LOD + blink',()=>{
 const values=[];
 for(const state of ['CROUCH','PRONE','RUN']){
  const u=new R.InfantryUnit({id:'check',side,seed:1003,state});u.seek(.25);
  for(const b of u.rig.bones)R.CoreChecks.assert(b.matrixWorld.elements.every(Number.isFinite),'Nonfinite transform');
  u.setEyesClosed(true);u.seek(.25);R.CoreChecks.assert(u.model.mesh.morphTargetInfluences[0]===1,'Forced closure');
  u.setDetail('high');R.CoreChecks.assert(u.model.mesh.morphTargetInfluences[0]===1,'LOD lost expression');
  values.push({state,finite:true,closedEyesPreserved:true});u.dispose();
 }
 return values;
});
report.passed=report.results.filter(x=>x.ok).length;report.failed=report.results.length-report.passed;
fs.mkdirSync(path.join(__dirname,'out'),{recursive:true});fs.writeFileSync(path.join(__dirname,'out/numerical_report.json'),JSON.stringify(report,null,2));
console.log('RESULT',report.passed,report.failed);if(report.failed)process.exitCode=1;
