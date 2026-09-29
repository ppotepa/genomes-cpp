// Async plant generation keeps the synchronous generator's deterministic output.
const fs=require('fs'),vm=require('vm'),path=require('path');
const root=path.resolve(__dirname,'..');
global.window=global;global.THREE=require(root+'/tests/numeric_math.cjs');
THREE.Color.prototype.clone=function(){return new THREE.Color().copy(this);};
THREE.Color.prototype.multiplyScalar=function(s){this.r*=s;this.g*=s;this.b*=s;return this;};
THREE.BufferGeometry.prototype.computeVertexNormals=function(){this.setAttribute('normal',new THREE.Float32BufferAttribute(new Float32Array(this.attributes.position.count*3),3));};
for(const file of ['js/core/seededRandom.js','js/environment/plantCatalog.js','js/environment/plantGenerator.js'])vm.runInThisContext(fs.readFileSync(path.join(root,file),'utf8'),{filename:file});
const R=RTS,assert=(ok,msg)=>{if(!ok)throw Error(msg);};
function referenceNormals(position){
  const normals=new Float32Array(position.length);
  for(let i=0;i<position.length;i+=9){
    const bx=position[i+3],by=position[i+4],bz=position[i+5],cx=position[i+6],cy=position[i+7],cz=position[i+8];
    const cbx=cx-bx,cby=cy-by,cbz=cz-bz,abx=position[i]-bx,aby=position[i+1]-by,abz=position[i+2]-bz;
    const nx=cby*abz-cbz*aby,ny=cbz*abx-cbx*abz,nz=cbx*aby-cby*abx,length=Math.sqrt(nx*nx+ny*ny+nz*nz)||1,inverse=1/length;
    for(let j=i;j<i+9;j+=3){normals[j]=nx*inverse;normals[j+1]=ny*inverse;normals[j+2]=nz*inverse;}
  }
  return Array.from(normals);
}
function referenceSphere(position){
  let minX=Infinity,minY=Infinity,minZ=Infinity,maxX=-Infinity,maxY=-Infinity,maxZ=-Infinity;
  for(let i=0;i<position.length;i+=3){minX=Math.min(minX,position[i]);minY=Math.min(minY,position[i+1]);minZ=Math.min(minZ,position[i+2]);maxX=Math.max(maxX,position[i]);maxY=Math.max(maxY,position[i+1]);maxZ=Math.max(maxZ,position[i+2]);}
  const x=(minX+maxX)*.5,y=(minY+maxY)*.5,z=(minZ+maxZ)*.5;let radiusSq=0;
  for(let i=0;i<position.length;i+=3){const dx=x-position[i],dy=y-position[i+1],dz=z-position[i+2];radiusSq=Math.max(radiusSq,dx*dx+dy*dy+dz*dz);}
  return [x,y,z,Math.sqrt(radiusSq)];
}
const serialize=lods=>lods.map(lod=>({stats:lod.stats,parts:lod.root.children.map(mesh=>({name:mesh.name,position:Array.from(mesh.geometry.attributes.position.array),color:Array.from(mesh.geometry.attributes.color.array),normal:Array.from(mesh.geometry.attributes.normal.array),sphere:[mesh.geometry.boundingSphere.center.x,mesh.geometry.boundingSphere.center.y,mesh.geometry.boundingSphere.center.z,mesh.geometry.boundingSphere.radius]}))}));
(async()=>{
  const genome=R.PlantGenome.create('oak',930145),yieldCount={value:0};
  const originalPerformance=global.performance,clockReads={value:0};
  global.performance={now(){clockReads.value++;return clockReads.value;}};
  const originalSkeletonSteps=R.PlantGenerator.skeletonSteps;
  let skeletonStepsFinished=false,yieldedWhileBuildingSkeleton=false,skeletonBuilds=0;
  R.PlantGenerator.skeletonSteps=function*(...args){skeletonBuilds++;try{return yield* originalSkeletonSteps.apply(this,args);}finally{skeletonStepsFinished=true;}};
  const asyncLods=await R.PlantGenerator.createLodsAsync(genome,{age:.81,season:'autumn'},async()=>{yieldCount.value++;if(!skeletonStepsFinished)yieldedWhileBuildingSkeleton=true;},()=>{},0);
  R.PlantGenerator.skeletonSteps=originalSkeletonSteps;
  assert(yieldCount.value>0,'async tree generation did not yield');
  assert(clockReads.value===0,'zero-budget generation yields without reading the clock at every checkpoint');
  assert(yieldedWhileBuildingSkeleton,'async tree generation did not yield while creating the procedural skeleton');
  assert(yieldCount.value>100,'skeleton, foliage and final geometry work did not yield at fine bounded checkpoints ('+yieldCount.value+' yields)');
  assert(skeletonBuilds===1,'initial async LOD set should generate exactly one architecture');
  const springLods=await R.PlantGenerator.createLodsAsync(genome,{age:.81,season:'spring'},async()=>{yieldCount.value++;},()=>{},0);
  assert(skeletonBuilds===1&&springLods[0].skeleton===asyncLods[0].skeleton,'cross-season async generation should reuse the retained exact skeleton');
  springLods.forEach(lod=>lod.dispose());
  for(const lod of asyncLods)for(const mesh of lod.root.children){
    const position=Array.from(mesh.geometry.attributes.position.array);
    const actual=mesh.geometry.attributes.normal,expected=referenceNormals(position),scale=actual.normalized?32767:1;
    for(let i=0;i<expected.length;i++)assert(Math.abs(actual.array[i]/scale-expected[i])<=1/32767,mesh.name+' face normal exceeded packed quantization tolerance at '+i);
    assert(JSON.stringify([mesh.geometry.boundingSphere.center.x,mesh.geometry.boundingSphere.center.y,mesh.geometry.boundingSphere.center.z,mesh.geometry.boundingSphere.radius])===JSON.stringify(referenceSphere(position)),mesh.name+' bounding sphere changed');
  }
  const asyncOutput=JSON.stringify(serialize(asyncLods));asyncLods.forEach(lod=>lod.dispose());
  const timedYieldCount={value:0},timedGenome=R.PlantGenome.create('oak',381772);
  const timed=await R.PlantGenerator.createAsync(timedGenome,{age:.77,detail:'high'},async()=>{timedYieldCount.value++;},null,1);
  assert(timedYieldCount.value>0,'positive-budget generation stopped yielding');
  assert(clockReads.value<timedYieldCount.value*2+4,'positive-budget generation reads the clock substantially less often than it yields');
  timed.dispose();global.performance=originalPerformance;
  const syncLods=R.PlantGenerator.createLods(genome,{age:.81,season:'autumn'});
  assert(JSON.stringify(serialize(syncLods))===asyncOutput,'async generation changed deterministic geometry or statistics');
  syncLods.forEach(lod=>lod.dispose());
  const interruptedGenome=R.PlantGenome.create('oak',771903),BaseMaterial=THREE.MeshStandardMaterial,baseDispose=THREE.BufferGeometry.prototype.dispose;
  let timberMaterialBuilt=false,disposedGeometries=0,timberTubeCalls=0;
  THREE.MeshStandardMaterial=class extends BaseMaterial{constructor(options){super(options);if(!timberMaterialBuilt&&options.side===THREE.FrontSide)timberMaterialBuilt=true;}};
  THREE.BufferGeometry.prototype.dispose=function(){disposedGeometries++;return baseDispose.call(this);};
  R.PlantGenerator._setTimberTubeObserver(()=>timberTubeCalls++);
  const beforeDisposed=disposedGeometries;
  let cancellation=null;
  try{await R.PlantGenerator.createAsync(interruptedGenome,{age:.73,season:'summer',detail:'world'},async()=>{},()=>{if(timberMaterialBuilt){const error=new Error('cancel after timber cache insertion');error.name='AbortError';throw error;}},0);}catch(error){cancellation=error;}
  assert(cancellation&&/cancel after timber cache insertion/.test(cancellation.message),'generation did not propagate cancellation after timber creation');
  assert(timberMaterialBuilt,'cancellation fixture did not reach the shared timber cache');
  assert(disposedGeometries>beforeDisposed,'cancellation did not dispose its uncommitted timber geometry');
  const callsBeforeRetry=timberTubeCalls,retry=R.PlantGenerator.create(interruptedGenome,{age:.73,season:'summer',detail:'world'});
  assert(timberTubeCalls>callsBeforeRetry,'cancelled timber entry remained retained in the cache');retry.dispose();
  R.PlantGenerator._setTimberTubeObserver(null);THREE.MeshStandardMaterial=BaseMaterial;THREE.BufferGeometry.prototype.dispose=baseDispose;
  console.log('PASS async plant generation yielded '+yieldCount.value+' times, matched synchronous geometry, and released cancelled builds');
})().catch(error=>{console.error(error);process.exitCode=1;});
