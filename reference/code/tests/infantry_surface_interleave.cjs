// Reentrant build test for generator state isolation. Uses numeric_math.cjs, not WebGL.
const assert=require('node:assert/strict'),crypto=require('node:crypto'),fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const root=path.resolve(__dirname,'..');
global.window=global;global.THREE=require(root+'/tests/numeric_math.cjs');
THREE.Quaternion.prototype.dot=function(q){return this.x*q.x+this.y*q.y+this.z*q.z+this.w*q.w;};
THREE.SkinnedMesh.prototype.updateMorphTargets=function(){this.morphTargetInfluences=[];this.morphTargetDictionary={};for(const[i,a]of(this.geometry.morphAttributes.position||[]).entries()){this.morphTargetInfluences.push(0);this.morphTargetDictionary[a.name]=i;}};
global.document={createElement:()=>({getContext:()=>({createImageData:(w,h)=>({data:new Uint8ClampedArray(w*h*4)}),putImageData:()=>{}})})};
const loader=fs.readFileSync(root+'/js/loader.js','utf8'),modules=vm.runInNewContext(loader.match(/const local=(\[[\s\S]*?\]);/)[1]);
for(const file of modules)vm.runInThisContext(fs.readFileSync(root+'/'+file,'utf8'),{filename:file});
const R=RTS;
function makeSubject(seed,sideId,color,loadout){
  const phenotype=R.InfantryGenome.express(R.InfantryGenome.create(seed));
  const anatomy=R.InfantryAnatomy.create(phenotype.height,phenotype.face,phenotype.body);
  const rig=new R.InfantryRig(anatomy,new THREE.Group());
  const side=new R.Side(sideId,color);
  const equipment=new R.Equipment({unitSeed:seed,slotSchema:R.EquipmentSlots,loadout});
  return {rig,side,equipment};
}
function build(subject){return R.InfantrySurface.create(subject.rig,subject.side,'world',subject.equipment);}
{
  const b=new R.SurfaceBuilder({anatomy:{height:1},index:{hips:0}}),v=THREE.Vector3;
  for(const p of [[0,0,0],[1,0,0],[1,1,0],[0,1,0]])b.vertex(new v(...p),{hips:1},[1,1,1],new v(0,0,1));
  b.triangle(0,1,2);b.triangle(0,2,3);b.morph('handsRelax',2,new v(0,0,.25));
  const out=b.finish(),g=out.geometry,base=g.attributes.position.array,delta=g.morphAttributes.position[0].array,morphNormal=g.morphAttributes.normal[0].array;
  assert.deepEqual(Array.from(base),[0,0,0,1,0,0,1,1,0,0,1,0],'sparse morph finalization must restore the immutable base positions');
  const triangles=g.index.array,oldAbsolute=new Float32Array(base.length);
  for(let i=0;i<base.length;i++)oldAbsolute[i]=Math.fround(base[i]+delta[i]);
  const affected=new Set([0,1,2,3]),oldAccum=new Float32Array(base.length);
  for(let t=0;t<triangles.length;t+=3){
    const a=triangles[t],b=triangles[t+1],c=triangles[t+2];if(!affected.has(a)&&!affected.has(b)&&!affected.has(c))continue;
    const ai=3*a,bi=3*b,ci=3*c,cbx=oldAbsolute[ci]-oldAbsolute[bi],cby=oldAbsolute[ci+1]-oldAbsolute[bi+1],cbz=oldAbsolute[ci+2]-oldAbsolute[bi+2],abx=oldAbsolute[ai]-oldAbsolute[bi],aby=oldAbsolute[ai+1]-oldAbsolute[bi+1],abz=oldAbsolute[ai+2]-oldAbsolute[bi+2],nx=cby*abz-cbz*aby,ny=cbz*abx-cbx*abz,nz=cbx*aby-cby*abx;
    oldAccum[ai]+=nx;oldAccum[ai+1]+=ny;oldAccum[ai+2]+=nz;oldAccum[bi]+=nx;oldAccum[bi+1]+=ny;oldAccum[bi+2]+=nz;oldAccum[ci]+=nx;oldAccum[ci+1]+=ny;oldAccum[ci+2]+=nz;
  }
  const expected=new Float32Array(base.length);for(const i of affected){const at=3*i,x=oldAccum[at],y=oldAccum[at+1],z=oldAccum[at+2],length=Math.sqrt(x*x+y*y+z*z)||1;expected[at]=x/length-g.attributes.normal.array[at];expected[at+1]=y/length-g.attributes.normal.array[at+1];expected[at+2]=z/length-g.attributes.normal.array[at+2];}
  assert.deepEqual(Array.from(morphNormal),Array.from(expected),'sparse morph normals must match the former full-buffer reference algorithm');
  out.geometry.dispose();
}
{
  const Vector3=THREE.Vector3,probe=new R.SurfaceBuilder({anatomy:{height:1},index:{hips:0}});
  const points=[new Vector3(0,0,0),new Vector3(1,0,0),new Vector3(0,1,0)];
  for(const p of points)probe.vertex(p,{hips:1},[1,1,1],new Vector3(0,0,1));
  probe.triangleFilter=(a,b,c,pa,pb,pc)=>{assert.equal(a,0);assert.equal(b,1);assert.equal(c,2);assert.equal(pa,probe._a);assert.equal(pb,probe._b);assert.equal(pc,probe._c);return true;};
  let allocations=0;
  THREE.Vector3=class extends Vector3{constructor(...args){super(...args);allocations++;}};
  try{probe.triangle(0,1,2);}finally{THREE.Vector3=Vector3;}
  assert.equal(allocations,0,'triangle filtering must reuse sampled points without temporary vectors');
}
{
  const Vector3=THREE.Vector3,probe=new R.SurfaceBuilder({anatomy:{height:1},index:{hips:0}});
  const center=new Vector3(0,.2,0),axisX=new Vector3(1,0,0),axisZ=new Vector3(0,0,1),normal=new Vector3(0,1,0),radii=new Vector3(.1,.2,.08);
  let allocations=0,loop;
  THREE.Vector3=class extends Vector3{constructor(...args){super(...args);allocations++;}};
  try{
    loop=probe.ring(center,axisX,axisZ,.1,.08,12,{hips:1},[.4,.5,.6],.3,null,(x,z,a,out)=>{out[0]=x;out[1]=z;});
    probe.cap(loop,{hips:1},[.4,.5,.6],normal);
    probe.ellipsoid(center,radii,{hips:1},[.5,.6,.7],0,12,8);
  }finally{THREE.Vector3=Vector3;}
  assert.equal(allocations,0,'ring, cap and ellipsoid must reuse builder-owned vectors per generated vertex');
  assert.equal(probe.positions.length/3,12+1+12*9,'scratch-based primitives keep their expected vertex topology');
  const legacy=new R.SurfaceBuilder({anatomy:{height:1},index:{hips:0}}),legacyRing=[];
  for(let j=0;j<12;j++){
    const angle=2*Math.PI*j/12,x=Math.cos(angle),z=Math.sin(angle),point=center.clone().addScaledVector(axisX,.1*x).addScaledVector(axisZ,.08*z),n=axisX.clone().multiplyScalar(x/.1).addScaledVector(axisZ,z/.08).normalize();
    legacyRing.push(legacy.vertex(point,{hips:1},[.4,.5,.6],n,[j/12*3,.3]));
  }
  const capCenter=new Vector3();legacyRing.forEach(i=>capCenter.add(legacy.point(i)));capCenter.multiplyScalar(1/legacyRing.length);
  const capIndex=legacy.vertex(capCenter,{hips:1},[.4,.5,.6],normal);
  for(let i=0;i<legacyRing.length;i++)legacy.triangle(capIndex,legacyRing[i],legacyRing[(i+1)%legacyRing.length]);
  const ellipsoidRings=[];
  for(let y=0;y<=8;y++){
    const phi=-Math.PI/2+Math.PI*y/8,ring=[];
    for(let j=0;j<12;j++){
      const angle=j/12*Math.PI*2,point=new Vector3(Math.cos(phi)*Math.cos(angle)*radii.x,Math.sin(phi)*radii.y,Math.cos(phi)*Math.sin(angle)*radii.z),n=new Vector3(Math.cos(phi)*Math.cos(angle)/radii.x,Math.sin(phi)/radii.y,Math.cos(phi)*Math.sin(angle)/radii.z).normalize();
      point.add(center);ring.push(legacy.vertex(point,{hips:1},[.5,.6,.7],n,[j/12,y/8]));
    }
    if(y)legacy.bridge(ellipsoidRings[y-1],ring,0);ellipsoidRings.push(ring);
  }
  for(const field of ['positions','colors','uvs','hints','skinIndices','skinWeights'])assert.deepEqual(probe[field],legacy[field],`scratch-based primitive ${field} matches the former per-vertex vector path`);
  assert.deepEqual(probe.triangles,legacy.triangles,'scratch-based primitive indices preserve winding and topology');
}
function signature(surface){
  const g=surface.geometry,h=crypto.createHash('sha256');
  const add=(name,array)=>{h.update(name);h.update(Buffer.from(array.buffer,array.byteOffset,array.byteLength));};
  add('index',g.index.array);
  for(const name of Object.keys(g.attributes).sort())add(name,g.attributes[name].array);
  for(const kind of Object.keys(g.morphAttributes).sort())for(const attr of g.morphAttributes[kind])add(kind+':'+attr.name,attr.array);
  h.update(JSON.stringify(surface.tags));h.update(JSON.stringify(surface.faceMetadata));
  return h.digest('hex');
}
function dispose(surface){surface.geometry.dispose();}

const A=makeSubject(1401,'A',0x596745,'RIFLEMAN'),B=makeSubject(2907,'B',0x475f78,'SCOUT');
const baselineA=build(A),baselineB=build(B),expectedA=signature(baselineA),expectedB=signature(baselineB);
let injected=false,nestedB=null;
const proto=R.SurfaceBuilder.prototype,originalRing=proto.ring;
proto.ring=function(...args){
  if(!injected&&this.rig===A.rig){
    injected=true;
    nestedB=build(B);
  }
  return originalRing.apply(this,args);
};
let interwovenA;
try{interwovenA=build(A);}finally{proto.ring=originalRing;}
assert.equal(injected,true,'second build must run inside the first build');
assert.equal(signature(interwovenA),expectedA,'interwoven build A differs from its sequential result');
assert.equal(signature(nestedB),expectedB,'nested build B differs from its sequential result');
(async()=>{
  let yields=0;
  const asyncSurface=await R.InfantrySurface.createAsync(A.rig,A.side,'world',A.equipment,null,async()=>{yields++;},null,0);
  assert.ok(yields>8,'asynchronous surface generation must yield across bounded geometry phases');
  assert.equal(signature(asyncSurface),expectedA,'asynchronous build differs byte-for-byte from synchronous surface');
  const originalPerformance=global.performance,clockReads={value:0};global.performance={now(){clockReads.value++;return clockReads.value;}};
  const zeroBudgetYields={value:0},zeroBudgetSurface=await R.InfantrySurface.createAsync(A.rig,A.side,'world',A.equipment,null,async()=>{zeroBudgetYields.value++;},null,0);
  assert.equal(clockReads.value,0,'zero-budget surface generation does not read the clock');
  const timedYields={value:0},timedSurface=await R.InfantrySurface.createAsync(A.rig,A.side,'world',A.equipment,null,async()=>{timedYields.value++;},null,1);
  assert.ok(timedYields.value>0,'positive-budget surface generation stopped yielding');
  assert.ok(clockReads.value<timedYields.value*2+4,'positive-budget surface generation samples the clock less often than every checkpoint');
  assert.equal(signature(zeroBudgetSurface),expectedA,'zero-budget checkpointing changed surface geometry');
  assert.equal(signature(timedSurface),expectedA,'timed checkpointing changed surface geometry');
  global.performance=originalPerformance;
  const reference=new THREE.BufferGeometry();reference.setAttribute('position',baselineA.geometry.attributes.position);reference.setIndex(baselineA.geometry.index.array);reference.morphAttributes=baselineA.geometry.morphAttributes;reference.morphTargetsRelative=baselineA.geometry.morphTargetsRelative;reference.computeVertexNormals();reference.computeBoundingBox();reference.computeBoundingSphere();
  const actualNormals=asyncSurface.geometry.attributes.normal.array,expectedNormals=reference.attributes.normal.array;
  assert.equal(actualNormals.length,expectedNormals.length,'chunked normal attribute size must match Three.js');
  for(let i=0;i<actualNormals.length;i++)assert.equal(actualNormals[i],expectedNormals[i],`chunked normal differs from Three.js at component ${i}`);
  for(const axis of ['x','y','z']){assert.equal(asyncSurface.geometry.boundingBox.min[axis],reference.boundingBox.min[axis],`chunked minimum bound differs on ${axis}`);assert.equal(asyncSurface.geometry.boundingBox.max[axis],reference.boundingBox.max[axis],`chunked maximum bound differs on ${axis}`);assert.equal(asyncSurface.geometry.boundingSphere.center[axis],reference.boundingSphere.center[axis],`chunked sphere center differs on ${axis}`);}
  assert.equal(asyncSurface.geometry.boundingSphere.radius,reference.boundingSphere.radius,'chunked bounding sphere must include the same morph range as Three.js');
  const releaseProbe=new R.SurfaceBuilder({anatomy:{height:1},index:{hips:0}});
  releaseProbe.vertex(new THREE.Vector3(0,0,0),{hips:1},[1,0,0],new THREE.Vector3(0,0,1));
  releaseProbe.vertex(new THREE.Vector3(1,0,0),{hips:1},[0,1,0],new THREE.Vector3(0,0,1));
  releaseProbe.vertex(new THREE.Vector3(0,1,0),{hips:1},[0,0,1],new THREE.Vector3(0,0,1));releaseProbe.triangle(0,1,2);
  const released=releaseProbe.finish();
  for(const field of ['positions','colors','uvs','skinIndices','skinWeights','hints','triangles'])assert.equal(releaseProbe[field],null,`finished builder should release source ${field}`);
  assert.equal(released.geometry.attributes.position.count,3,'releasing builder arrays must preserve final typed geometry');released.geometry.dispose();
  const finalizationProbe=new R.SurfaceBuilder({anatomy:{height:1},index:{hips:0}});
  finalizationProbe.positions=[0,0,0,1,0,0,0,1,0];finalizationProbe.colors=[1,1,1,1,1,1,1,1,1];finalizationProbe.uvs=[0,0,1,0,0,1];
  finalizationProbe.hints=[0,0,1,0,0,1,0,0,1];finalizationProbe.skinIndices=Array(12).fill(0);finalizationProbe.skinWeights=[1,0,0,0,1,0,0,0,1,0,0,0];
  for(let i=0;i<5000;i++)finalizationProbe.triangles[0].push(0,1,2);
  const builderOriginalPerformance=global.performance,builderClockReads={value:0};global.performance={now(){builderClockReads.value++;return builderClockReads.value;}};
  let finalizationYields=0,finalizationCancelled=false;
  try{await finalizationProbe.finishAsync(async()=>{finalizationYields++;},()=>{if(finalizationYields>=2){const error=new Error('cancelled in finalization');error.name='AbortError';throw error;}},0);}
  catch(error){finalizationCancelled=error.name==='AbortError';}
  assert.equal(builderClockReads.value,0,'zero-budget finalization never reads the clock');
  assert.equal(finalizationYields,2,'finalization should yield during index assembly and topology orientation');
  assert.equal(finalizationCancelled,true,'surface finalization should observe cancellation inside topology orientation');
  const timedProbe=new R.SurfaceBuilder({anatomy:{height:1},index:{hips:0}});
  timedProbe.positions=[0,0,0,1,0,0,0,1,0];timedProbe.colors=[1,1,1,1,1,1,1,1,1];timedProbe.uvs=[0,0,1,0,0,1];
  timedProbe.hints=[0,0,1,0,0,1,0,0,1];timedProbe.skinIndices=Array(12).fill(0);timedProbe.skinWeights=[1,0,0,0,1,0,0,0,1,0,0,0];timedProbe.trustLocalWindingHints=true;
  for(let i=0;i<50000;i++)timedProbe.triangles[0].push(0,1,2);
  let timedFinalizationYields=0;const timedFinalization=await timedProbe.finishAsync(async()=>{timedFinalizationYields++;},null,1);
  assert.ok(timedFinalizationYields>0,'positive-budget finalization stopped yielding');
  assert.ok(builderClockReads.value<timedFinalizationYields*2+4,'positive-budget finalization samples the clock less often than every chunk');
  assert.equal(timedFinalization.triangles,50000,'timed finalization changed triangle count');timedFinalization.geometry.dispose();global.performance=builderOriginalPerformance;
  let checks=0,cancelled=false;
  try{await R.InfantrySurface.createAsync(A.rig,A.side,'world',A.equipment,null,async()=>{},()=>{if(++checks===5){const error=new Error('cancelled');error.name='AbortError';throw error;}},0);}
  catch(error){cancelled=error.name==='AbortError';}
  assert.equal(cancelled,true,'surface generation should observe cancellation between bounded steps');
  for(const surface of [baselineA,baselineB,nestedB,interwovenA,asyncSurface,zeroBudgetSurface,timedSurface])dispose(surface);
  for(const subject of [A,B])subject.rig.dispose();
  console.log('PASS infantry surface sync/async builds preserve exact buffers and async phases yield/cancel safely');
})().catch(error=>{console.error(error);process.exitCode=1;});
