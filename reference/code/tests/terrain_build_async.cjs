// Exact sync/async terrain equivalence with the repository's numeric THREE shim.
const assert=require('assert'),fs=require('fs'),vm=require('vm'),path=require('path');
const root=path.resolve(__dirname,'..');
global.window=global;global.THREE=require('./numeric_math.cjs');
THREE.BufferGeometry.prototype.setDrawRange=function(start,count){this.drawRange={start,count};return this;};
THREE.Box3.prototype.getCenter=function(out){return out.copy(this.min).add(this.max).multiplyScalar(.5);};
function assertArraysEqual(actual,expected,label){
  assert.strictEqual(actual.length,expected.length,label+' length');
  for(let i=0;i<actual.length;i++)if(actual[i]!==expected[i])throw Error(label+' differs at '+i+': '+actual[i]+' !== '+expected[i]);
}
const PlaneGeometry=THREE.PlaneGeometry;
THREE.PlaneGeometry=class extends PlaneGeometry{constructor(...args){super(...args);this.drawRange={start:0,count:Infinity};}};
for(const file of ['js/config.js','js/core/seededRandom.js','js/noise.js','js/terrain.js'])
  vm.runInThisContext(fs.readFileSync(path.join(root,file),'utf8'),{filename:file});

// Pinned to Three.js r128 BufferGeometry.computeVertexNormals and Vector3.normalize semantics.
function referenceR128Normals(geometry){
  const p=geometry.attributes.position.array,ids=geometry.index.array,n=new Float32Array(p.length);
  for(let i=0;i<ids.length;i+=3){
    const a=ids[i]*3,b=ids[i+1]*3,c=ids[i+2]*3;
    const cbx=p[c]-p[b],cby=p[c+1]-p[b+1],cbz=p[c+2]-p[b+2];
    const abx=p[a]-p[b],aby=p[a+1]-p[b+1],abz=p[a+2]-p[b+2];
    const nx=cby*abz-cbz*aby,ny=cbz*abx-cbx*abz,nz=cbx*aby-cby*abx;
    n[a]+=nx;n[a+1]+=ny;n[a+2]+=nz;n[b]+=nx;n[b+1]+=ny;n[b+2]+=nz;n[c]+=nx;n[c+1]+=ny;n[c+2]+=nz;
  }
  for(let i=0;i<n.length;i+=3){const length=Math.sqrt(n[i]*n[i]+n[i+1]*n[i+1]+n[i+2]*n[i+2])||1,inverse=1/length;n[i]*=inverse;n[i+1]*=inverse;n[i+2]*=inverse;}
  return n;
}
function referenceNoise(noise,x,y){
  const floorX=Math.floor(x),floorY=Math.floor(y),xi=floorX&255,yi=floorY&255,xf=x-floorX,yf=y-floorY,u=noise.fade(xf),v=noise.fade(yf);
  const aa=noise.perm[noise.perm[xi]+yi],ab=noise.perm[noise.perm[xi]+yi+1],ba=noise.perm[noise.perm[xi+1]+yi],bb=noise.perm[noise.perm[xi+1]+yi+1];
  const x1=noise.lerp(noise.grad(aa,xf,yf),noise.grad(ba,xf-1,yf),u),x2=noise.lerp(noise.grad(ab,xf,yf-1),noise.grad(bb,xf-1,yf-1),u);
  return noise.lerp(x1,x2,v)*.72;
}
function referenceFbm(noise,x,z,octaves,lacunarity,gain){let value=0,amplitude=.5,frequency=1,total=0;for(let i=0;i<octaves;i++){value+=referenceNoise(noise,x*frequency,z*frequency)*amplitude;total+=amplitude;amplitude*=gain;frequency*=lacunarity;}return value/Math.max(total,.0001);}
function referenceWorldHeight(noise,x,z,halfMap){
  const warpX=referenceFbm(noise,x/850+18.1,z/850-7.4,3,2.02,.5)*130,warpZ=referenceFbm(noise,x/850-11.7,z/850+21.8,3,2.02,.5)*130,wx=x+warpX,wz=z+warpZ;
  const continental=referenceFbm(noise,wx/1350,wz/1350,5,2,.53),hills=referenceFbm(noise,wx/430+31.3,wz/430-16.2,5,2.08,.5),detail=referenceFbm(noise,wx/120-9,wz/120+12,4,2.15,.46),ridgeNoise=referenceFbm(noise,wx/620+4.2,wz/620+15.6,4,2,.5);
  const ridged=1-Math.abs(ridgeNoise),ridgeMask=THREE.MathUtils.clamp((continental+.12)*1.25,0,1),plainBase=Math.sign(continental)*Math.pow(Math.abs(continental),1.35),hillMask=THREE.MathUtils.smoothstep(continental,-.08,.42);
  let h=plainBase*92;h+=hills*(26+hillMask*44);h+=(ridged-.56)*36*ridgeMask;h+=detail*7;const edge=Math.max(Math.abs(x),Math.abs(z))/halfMap;h-=THREE.MathUtils.smoothstep(edge,.83,1)*8;return h;
}

async function main(){
  const noiseCoordinates=[[-256,-256],[-3.25,7.875],[-.001,0],[0,0],[1,1],[12.5,-31.75],[255.999,256.001],[1024.125,-2048.625]];
  for(const seed of [0,1,20260924,0xffffffff]){
    const noise=new RTS.SeededPerlin2D(seed);
    for(const [x,y] of noiseCoordinates)assert.strictEqual(noise.noise(x,y),referenceNoise(noise,x,y),'inline Perlin path changed seed '+seed+' sample '+x+','+y);
  }
  for(const seed of [0,8291,14523,0xffffffff]){
    const noise=new RTS.SeededPerlin2D(seed),terrain=new RTS.TerrainSystem(new THREE.Group(),{mapSize:RTS.Config.MAP_SIZE,segments:40,battlefield:false}),height=terrain.makeHeightFunction(seed);
    for(let zi=0;zi<=12;zi++)for(let xi=0;xi<=12;xi++){
      const x=-RTS.Config.HALF_MAP+xi*RTS.Config.MAP_SIZE/12,z=-RTS.Config.HALF_MAP+zi*RTS.Config.MAP_SIZE/12;
      assert.strictEqual(height(x,z),referenceWorldHeight(noise,x,z,RTS.Config.HALF_MAP),'precomputed FBM octave plan changed the legacy height at seed '+seed+' coordinate '+x+','+z);
    }
    terrain.dispose();
  }
  const options={mapSize:200,segments:160,battlefield:true};
  const sync=new RTS.TerrainSystem(new THREE.Group(),options);sync.build(20260924);
  const referenceHeight=(terrain,x,z)=>{
    const N=terrain.segments,step=terrain._step,half=terrain._half,fx=Math.max(0,Math.min(N,(x+half)/step)),fz=Math.max(0,Math.min(N,(z+half)/step));
    const ix=Math.min(N-1,Math.floor(fx)),iz=Math.min(N-1,Math.floor(fz)),u=fx-ix,v=fz-iz,k=iz*(N+1)+ix,a=terrain.heights[k],d=terrain.heights[k+1],b=terrain.heights[k+N+1],c=terrain.heights[k+N+2];
    return u+v<=1?a+(d-a)*u+(b-a)*v:c+(b-c)*(1-u)+(d-c)*(1-v);
  };
  let clampCalls=0;const originalClamp=RTS.Math.clamp;RTS.Math.clamp=function(...args){clampCalls++;return originalClamp.apply(this,args);};
  const terrainHeightCache={terrain:null,heights:null,ix:-1,iz:-1,a:0,b:0,c:0,d:0};
  try{
    for(const [x,z] of [[-100,-100],[100,100],[-101,101],[12.375,-39.625],[NaN,0],[Infinity,-Infinity]]){
      const expected=referenceHeight(sync,x,z),sampled=sync.sample(x,z,{height:0,normal:new THREE.Vector3()}).height,queried=sync.getHeightAt(x,z),cached=sync.getHeightAtCached(x,z,terrainHeightCache);
      assert.strictEqual(sampled,expected,'direct grid clamp changed sampled terrain at '+x+','+z);
      assert.strictEqual(queried,expected,'direct grid clamp changed terrain query at '+x+','+z);
      assert.strictEqual(cached,expected,'cached terrain query changed terrain height at '+x+','+z);
    }
  }finally{RTS.Math.clamp=originalClamp;}
  assert.strictEqual(clampCalls,0,'terrain height and normal sampling should not dispatch through generic clamp twice per query');
  const cachedQueries=[[-10,5],[-9.9,5.1],[-8.7,5.1]].map(([x,z])=>[x,z,sync.getHeightAt(x,z)]);
  const originalFloor=Math.floor;let floorCalls=0;Math.floor=value=>{floorCalls++;return originalFloor(value);};
  try{
    terrainHeightCache.terrain=null;terrainHeightCache.heights=null;
    for(let i=0;i<2;i++){const [x,z,height]=cachedQueries[i];assert.strictEqual(sync.getHeightAtCached(x,z,terrainHeightCache),height,'same-cell cache preserves exact height');}
    assert.strictEqual(floorCalls,2,'first cached query resolves row and column once');
    const [x,z,height]=cachedQueries[2];assert.strictEqual(sync.getHeightAtCached(x,z,terrainHeightCache),height,'cross-cell cache preserves exact height');
    assert.strictEqual(floorCalls,3,'moving to an adjacent column reuses the unchanged row lookup');
  }finally{Math.floor=originalFloor;}
  for(let zi=0;zi<=64;zi++)for(let xi=0;xi<=64;xi++){
    const x=-options.mapSize/2+xi*options.mapSize/64,z=-options.mapSize/2+zi*options.mapSize/64;
    assert.strictEqual(sync.getHeightAtCached(x,z,terrainHeightCache),sync.getHeightAt(x,z),'cached height differs on dense terrain grid at '+x+','+z);
  }
  for(let cell=0;cell<=sync.segments;cell++){
    const edge=-sync._half+cell*sync._step,offset=Math.max(1,Math.abs(edge))*Number.EPSILON*2;
    for(const x of [edge-offset,edge,edge+offset])for(const z of [-31.25,edge-offset,edge,edge+offset,47.5]){
      assert.strictEqual(sync.getHeightAtCached(x,z,terrainHeightCache),sync.getHeightAt(x,z),'cached height differs across grid boundary '+cell+' at '+x+','+z);
    }
  }
  const sliced=new RTS.TerrainSystem(new THREE.Group(),options);let yields=0;
  await sliced.buildAsync(20260924,async()=>{yields++;},()=>{});
  assert(yields>=35,'height/color/normal/chunk passes yielded at bounded intervals');
  assert.deepStrictEqual(Array.from(sliced.heights),Array.from(sync.heights),'height samples');
  const reference=new THREE.PlaneGeometry(options.mapSize,options.mapSize,options.segments,options.segments);
  reference.rotateX(-Math.PI/2);
  for(let i=0;i<reference.attributes.position.count;i++)reference.attributes.position.setY(i,sliced.heights[i]);
  assertArraysEqual(sliced.mesh.children[0].geometry.attributes.position.array,reference.attributes.position.array,'incremental plane positions');
  const expectedUvs=[];for(let iy=0;iy<=options.segments;iy++)for(let ix=0;ix<=options.segments;ix++)expectedUvs.push(ix/options.segments,1-iy/options.segments);
  assertArraysEqual(sliced.mesh.children[0].geometry.attributes.uv.array,new Float32Array(expectedUvs),'incremental plane UVs');
  assert.deepStrictEqual(Array.from(sliced.mesh.children[0].geometry.attributes.normal.array),Array.from(referenceR128Normals(reference)),'incremental normals match Three.js r128 computeVertexNormals exactly');
  assert.strictEqual(sliced.mesh.children.length,sync.mesh.children.length,'chunk count');
  for(let i=0;i<sync.mesh.children.length;i++){
    const a=sync.mesh.children[i].geometry,b=sliced.mesh.children[i].geometry;
    for(const name of ['position','normal','color','uv'])
      assert.deepStrictEqual(Array.from(b.attributes[name].array),Array.from(a.attributes[name].array),name+' chunk '+i);
    assert.deepStrictEqual(b.index.array,a.index.array,'indices chunk '+i);
    const match=/TerrainChunk-(\d+)-(\d+)/.exec(sync.mesh.children[i].name),chunkX=Number(match[1]),chunkZ=Number(match[2]),expectedIndices=[];
    for(let z=chunkZ*40;z<Math.min(options.segments,(chunkZ+1)*40);z++)for(let x=chunkX*40;x<Math.min(options.segments,(chunkX+1)*40);x++){
      const start=(z*options.segments+x)*6;for(let k=0;k<6;k++)expectedIndices.push(reference.index.array[start+k]);
    }
    assertArraysEqual(b.index.array,new Uint16Array(expectedIndices),'direct typed plane indices chunk '+i);
    assert.deepStrictEqual(b.boundingBox.min,a.boundingBox.min,'bounds min chunk '+i);
    assert.deepStrictEqual(b.boundingBox.max,a.boundingBox.max,'bounds max chunk '+i);
    assert.deepStrictEqual(b.boundingSphere.center,a.boundingSphere.center,'bounds sphere center chunk '+i);
    assert.strictEqual(b.boundingSphere.radius,a.boundingSphere.radius,'bounds sphere chunk '+i);
  }

  // Exercise the actual worker branch through a deterministic in-process
  // Worker shim, then compare its complete renderable output with sync build.
  const workerSource=fs.readFileSync(path.join(root,'js/workers/terrainHeightWorker.js'),'utf8');
  class InlineWorker{
    constructor(url){this.url=String(url);this.terminated=false;}
    postMessage(data){const self={postMessage:message=>queueMicrotask(()=>{if(!this.terminated)this.onmessage({data:message});})};vm.runInNewContext(workerSource,{self,Math,Float32Array,Uint8Array});self.onmessage({data});}
    terminate(){this.terminated=true;}
  }
  const previousWorker=global.Worker,previousDocument=global.document;
  global.Worker=InlineWorker;global.document={baseURI:'https://game.invalid/index.html'};
  const workerBuild=new RTS.TerrainSystem(new THREE.Group(),options);let workerYields=0;
  try{await workerBuild.buildAsync(20260924,async()=>{workerYields++;},()=>{});}
  finally{global.Worker=previousWorker;global.document=previousDocument;}
  assert.ok(workerYields>=35,'worker-computed heights still continue through bounded main-thread geometry slices');
  assert.deepStrictEqual(Array.from(workerBuild.heights),Array.from(sync.heights),'worker height field equals sync build');
  for(let i=0;i<sync.mesh.children.length;i++)for(const name of ['position','normal','color','uv'])
    assert.deepStrictEqual(Array.from(workerBuild.mesh.children[i].geometry.attributes[name].array),Array.from(sync.mesh.children[i].geometry.attributes[name].array),'worker '+name+' chunk '+i);
  workerBuild.dispose();
  const fullOptions={mapSize:RTS.Config.MAP_SIZE,segments:40,battlefield:false},fullSync=new RTS.TerrainSystem(new THREE.Group(),fullOptions),fullWorker=new RTS.TerrainSystem(new THREE.Group(),fullOptions);
  fullSync.build(14523);global.Worker=InlineWorker;global.document={baseURI:'https://game.invalid/index.html'};
  await fullWorker.buildAsync(14523,async()=>{},()=>{});global.Worker=previousWorker;global.document=previousDocument;
  assert.deepStrictEqual(Array.from(fullWorker.heights),Array.from(fullSync.heights),'worker full-world heights');
  for(let i=0;i<fullSync.mesh.children.length;i++)for(const name of ['position','normal','color','uv'])
    assert.deepStrictEqual(Array.from(fullWorker.mesh.children[i].geometry.attributes[name].array),Array.from(fullSync.mesh.children[i].geometry.attributes[name].array),'worker full-world '+name+' chunk '+i);
  fullWorker.dispose();fullSync.dispose();
  let abortedWorker=null;
  class HangingWorker extends InlineWorker{constructor(url){super(url);abortedWorker=this;}postMessage(){}}
  global.Worker=HangingWorker;global.document={baseURI:'https://game.invalid/index.html'};
  const workerCancelled=new RTS.TerrainSystem(new THREE.Group(),options),controller=new AbortController();
  const workerPending=workerCancelled.buildAsync(3,async()=>{},()=>{},controller.signal);controller.abort();
  await assert.rejects(workerPending,error=>error.name==='AbortError');
  assert.ok(abortedWorker&&abortedWorker.terminated,'aborting a worker terrain build terminates the worker promptly');
  global.Worker=previousWorker;global.document=previousDocument;workerCancelled.dispose();

  const cancelled=new RTS.TerrainSystem(new THREE.Group(),options);let checks=0;
  await assert.rejects(cancelled.buildAsync(7,async()=>{},()=>{if(++checks===3)throw Error('cancelled');}),/cancelled/);
  cancelled.dispose();assert.strictEqual(cancelled.heights,null,'cancel cleanup');
  sync.dispose();sliced.dispose();
  console.log('PASS sync/async terrain exactness: heights, chunk attributes/indices/bounds; bounded yields; cancellation cleanup');
}
main().catch(error=>{console.error(error);process.exitCode=1;});
