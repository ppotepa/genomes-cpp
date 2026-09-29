// Procedural timber cache ownership and determinism; numeric math only, no WebGL.
const fs=require('fs'),vm=require('vm'),path=require('path'),crypto=require('crypto');
const root=path.resolve(__dirname,'..');
global.window=global;global.THREE=require(root+'/tests/numeric_math.cjs');
THREE.Color.prototype.clone=function(){return new THREE.Color().copy(this);};
THREE.Color.prototype.multiplyScalar=function(s){this.r*=s;this.g*=s;this.b*=s;return this;};
THREE.BufferGeometry.prototype.computeVertexNormals=function(){this.setAttribute('normal',new THREE.Float32BufferAttribute(new Float32Array(this.attributes.position.count*3),3));};
for(const file of ['js/core/seededRandom.js','js/environment/plantCatalog.js','js/environment/plantGenerator.js']){
  vm.runInThisContext(fs.readFileSync(path.join(root,file),'utf8'),{filename:file});
}
const R=RTS,assert=(ok,msg)=>{if(!ok)throw Error(msg);},seasons=['summer','spring','autumn','winter'],details=['world','distant','far'];
const plantMemoryBefore=R.PlantGenerator.memoryStats();
assert(plantMemoryBefore.retainedGeometryBytes===0,'plant cache should start without retained geometry');
const memoryFixture=R.PlantGenerator.createLods(R.PlantGenome.create('maple',91031),{age:.79,season:'summer'}),expectedPlantBuffers=new Set();
for(const variant of memoryFixture)for(const mesh of variant.root.children){
  const geometry=mesh.geometry,attributes=geometry.attributes||{};
  if(geometry.index)expectedPlantBuffers.add(geometry.index.array.buffer);
  for(const name in attributes)expectedPlantBuffers.add(attributes[name].array.buffer);
  const morphs=geometry.morphAttributes||{};for(const name in morphs)for(const attribute of morphs[name])expectedPlantBuffers.add(attribute.array.buffer);
}
const expectedPlantBytes=Array.from(expectedPlantBuffers).reduce((sum,buffer)=>sum+buffer.byteLength,0),plantMemoryLive=R.PlantGenerator.memoryStats();
assert(plantMemoryLive.retainedGeometryBytes===expectedPlantBytes,'plant cache byte count must exactly deduplicate retained geometry ArrayBuffers');
assert(plantMemoryLive.peakRetainedGeometryBytes>=expectedPlantBytes,'plant cache high-water must include the retained build');
const memoryFixtureHit=R.PlantGenerator.createLods(R.PlantGenome.create('maple',91031),{age:.79,season:'summer'});
assert(R.PlantGenerator.memoryStats().retainedGeometryBytes===expectedPlantBytes,'cache hit must not double-count shared plant buffers');
memoryFixtureHit.forEach(variant=>variant.dispose());memoryFixture.forEach(variant=>variant.dispose());
assert(R.PlantGenerator.memoryStats().retainedGeometryBytes===0,'last plant cache release must clear current retained bytes');
console.log('PASS exact plant-cache geometry bytes and high-water tracking',JSON.stringify({expectedPlantBytes,peak:R.PlantGenerator.memoryStats().peakRetainedGeometryBytes}));
const fiveSided=R.PlantGenerator._tubeProfile(5);
assert(fiveSided&&fiveSided.cosine.length===6&&fiveSided.sine.length===6,'five-sided timber profile was not precomputed');
for(let i=0;i<=5;i++){
  const angle=i*Math.PI*2/5;
  assert(fiveSided.cosine[i]===Math.cos(angle)&&fiveSided.sine[i]===Math.sin(angle),'precomputed five-sided profile changed trig result at '+i);
}
let triangleSoupBytes=0,indexedCandidateBytes=0,triangleSoupVertices=0,indexedUniqueVertices=0;
function accountExactIndexedCandidate(mesh){
  const geometry=mesh.geometry,p=geometry.attributes.position.array,c=geometry.attributes.color.array,n=geometry.attributes.normal.array,vertices=p.length/3,keys=new Set();
  assert(!geometry.index,'input triangle soup unexpectedly has an index buffer');
  for(let i=0;i<vertices;i++){
    const o=i*3;keys.add(p[o]+','+p[o+1]+','+p[o+2]+','+c[o]+','+c[o+1]+','+c[o+2]+','+n[o]+','+n[o+1]+','+n[o+2]);
  }
  const unique=keys.size,indexBytes=unique<=65536?2:4;
  triangleSoupBytes+=p.byteLength+c.byteLength+n.byteLength;
  indexedCandidateBytes+=unique*(3*p.BYTES_PER_ELEMENT+3*c.BYTES_PER_ELEMENT+3*n.BYTES_PER_ELEMENT)+vertices*indexBytes;
  triangleSoupVertices+=vertices;indexedUniqueVertices+=unique;
}
for(const [species,expected] of [['maple','7773616c95ee0310d96a7145dbdbf42ce9dda988069e511cf13be9322ab3542d'],['oak','4a8c220132df55eb22a655073ea81cc978b907f80d824280759ed0fb26d5596c']]){
  const model=R.PlantGenerator.create(R.PlantGenome.create(species,51372),{age:.81,season:'summer',detail:'high'}),hash=crypto.createHash('sha256');
  for(const mesh of model.root.children){hash.update(mesh.name);for(const name of ['position','color','normal']){const array=mesh.geometry.attributes[name].array;hash.update(Buffer.from(array.buffer,array.byteOffset,array.byteLength));}}
  model.dispose();const actual=hash.digest('hex');assert(actual===expected,species+' packed geometry digest changed: '+actual);
}
let checked=0,timberTubeCalls=0;
R.PlantGenerator._setTimberTubeObserver(()=>timberTubeCalls++);
const originalSkeletonSteps=R.PlantGenerator.skeletonSteps;let skeletonBuilds=0;
R.PlantGenerator.skeletonSteps=function*(...args){skeletonBuilds++;return yield* originalSkeletonSteps.apply(this,args);};
for(const species of Object.keys(R.EnvironmentCatalog.plants)){
  const genome=R.PlantGenome.create(species,84491),bySeason=[],timber=[];
  let skeletonJson=null,skeletonReference=null;
  for(const season of seasons){
    const beforeTimberTubes=timberTubeCalls,beforeSkeletonBuilds=skeletonBuilds;
    const lods=R.PlantGenerator.createLods(genome,{season,age:.83});
    if(season==='summer')assert(timberTubeCalls>beforeTimberTubes,species+' did not emit initial timber tubes');
    else assert(timberTubeCalls===beforeTimberTubes,species+' regenerated timber tubes on season cache hit');
    if(season==='summer')assert(skeletonBuilds===beforeSkeletonBuilds+1,species+' did not build its initial procedural skeleton');
    else assert(skeletonBuilds===beforeSkeletonBuilds,species+' rebuilt the season-independent skeleton');
    assert(lods.length===3,species+' LOD count');
    assert(lods[0].skeleton===lods[1].skeleton&&lods[1].skeleton===lods[2].skeleton,species+' architecture not shared between LODs');
    if(season==='summer')skeletonReference=lods[0].skeleton;else assert(lods[0].skeleton===skeletonReference,species+' did not reuse the exact skeleton object across seasons');
    const serialized=JSON.stringify(lods[0].skeleton);
    if(skeletonJson===null)skeletonJson=serialized;else assert(serialized===skeletonJson,species+' season changed architecture');
    for(let i=0;i<lods.length;i++){
      if(season==='summer')for(const mesh of lods[i].root.children)accountExactIndexedCandidate(mesh);
      const wood=lods[i].root.children.find(mesh=>mesh.name==='wood');
      assert(wood,species+' missing timber '+details[i]);
      assert(Array.from(wood.geometry.attributes.position.array).every(Number.isFinite),species+' non-finite timber '+details[i]);
      if(!timber[i]){
        timber[i]={geometry:wood.geometry,positions:Array.from(wood.geometry.attributes.position.array),branches:lods[i].stats.branches,disposed:0};
        const dispose=wood.geometry.dispose.bind(wood.geometry);wood.geometry.dispose=()=>{timber[i].disposed++;dispose();};
      }else{
        assert(wood.geometry===timber[i].geometry,species+' failed timber sharing across seasons '+details[i]);
        assert(JSON.stringify(Array.from(wood.geometry.attributes.position.array))===JSON.stringify(timber[i].positions),species+' timber positions changed '+details[i]);
        assert(lods[i].stats.branches===timber[i].branches,species+' timber statistics changed '+details[i]);
      }
    }
    bySeason.push(lods);checked++;
  }
  for(let s=0;s<seasons.length;s++){
    for(const model of bySeason[s])model.dispose();
    for(let i=0;i<details.length;i++)assert(timber[i].disposed===(s===seasons.length-1?1:0),species+' timber disposed while still referenced '+details[i]+' after '+seasons[s]);
  }
  if(checked===seasons.length){
    const beforeSkeletonBuilds=skeletonBuilds,rebuilt=R.PlantGenerator.createLods(genome,{season:'summer',age:.83});
    assert(skeletonBuilds===beforeSkeletonBuilds+1,'last world variant release should evict its unreferenced skeleton');
    rebuilt.forEach(variant=>variant.dispose());
  }
  for(let i=0;i<details.length;i++){
    const direct=R.PlantGenerator.create(genome,{season:'summer',age:.83,detail:details[i],skeleton:bySeason[0][i].skeleton});
    const wood=direct.root.children.find(mesh=>mesh.name==='wood');
    assert(JSON.stringify(Array.from(wood.geometry.attributes.position.array))===JSON.stringify(timber[i].positions),species+' cached timber differs from direct generation '+details[i]);
    direct.dispose();
  }
}
const fruitRngGenome=R.PlantGenome.create('oak',77231),fruitRngSkeleton=R.PlantGenerator.skeleton(fruitRngGenome,.83),originalRandomNext=R.SeededRandom.prototype.next;
let randomCalls=0;R.SeededRandom.prototype.next=function(){randomCalls++;return originalRandomNext.call(this);};
let highFruitRngCalls=0,worldFruitRngCalls=0;
try{
  const high=R.PlantGenerator.create(fruitRngGenome,{age:.83,season:'summer',detail:'high',skeleton:fruitRngSkeleton});highFruitRngCalls=randomCalls;high.dispose();
  randomCalls=0;
  const world=R.PlantGenerator.create(fruitRngGenome,{age:.83,season:'summer',detail:'world',skeleton:fruitRngSkeleton});worldFruitRngCalls=randomCalls;world.dispose();
}finally{R.SeededRandom.prototype.next=originalRandomNext;}
assert(highFruitRngCalls-worldFruitRngCalls===fruitRngSkeleton.tips.length*2,'world LOD should skip exactly its two unused fruit RNG samples per tip');
const winterRngGenome=R.PlantGenome.create('oak',77232),winterRngSkeleton=R.PlantGenerator.skeleton(winterRngGenome,.83),randomCallsBySeason={};
randomCalls=0;R.SeededRandom.prototype.next=function(){randomCalls++;return originalRandomNext.call(this);};
try{
  for(const season of ['summer','winter']){
    randomCalls=0;
    const model=R.PlantGenerator.create(winterRngGenome,{age:.83,season,detail:'high',skeleton:winterRngSkeleton});
    randomCallsBySeason[season]=randomCalls;model.dispose();
  }
}finally{R.SeededRandom.prototype.next=originalRandomNext;}
assert(randomCallsBySeason.summer-randomCallsBySeason.winter===winterRngSkeleton.tips.length*60,'deciduous winter LOD should skip six unused leaf RNG samples per leaf');
const OriginalSeasonColor=THREE.Color;let seasonalColorAllocations=0;
THREE.Color=class extends OriginalSeasonColor{constructor(...args){super(...args);seasonalColorAllocations++;}};
const seasonalGenome=R.PlantGenome.create('oak',99101),summerWood=R.PlantGenerator.create(seasonalGenome,{age:.83,season:'summer',detail:'world'}),summerColors=seasonalColorAllocations;
const autumnWood=R.PlantGenerator.create(seasonalGenome,{age:.83,season:'autumn',detail:'world'}),autumnColors=seasonalColorAllocations-summerColors;
const winterWood=R.PlantGenerator.create(seasonalGenome,{age:.83,season:'winter',detail:'world'}),winterColors=seasonalColorAllocations-summerColors-autumnColors;
THREE.Color=OriginalSeasonColor;
assert(summerColors>0,'initial wood/foliage generation did not prepare colors');
assert(autumnColors===1,'cross-season timber cache hit allocated '+autumnColors+' colors instead of only the foliage color');
assert(winterColors===0,'leafless deciduous winter variant allocated '+winterColors+' unused seasonal colors');
summerWood.dispose();autumnWood.dispose();winterWood.dispose();
const float32AttributeBytes=triangleSoupVertices*9*Float32Array.BYTES_PER_ELEMENT;
console.log('EXACT_INDEX_ANALYSIS',JSON.stringify({float32AttributeBytes,triangleSoupBytes,indexedCandidateBytes,triangleSoupVertices,indexedUniqueVertices,packedAttributeSavingsPercent:((1-triangleSoupBytes/float32AttributeBytes)*100).toFixed(2),byteDeltaPercent:((indexedCandidateBytes/triangleSoupBytes-1)*100).toFixed(2)}));
assert(indexedCandidateBytes>triangleSoupBytes,'exact position/color/normal indexing should not be enabled when it increases the buffer footprint');
assert(triangleSoupVertices===1865868&&indexedUniqueVertices===1735504,'documented exact-index vertex counts changed; refresh the cost analysis');
assert(triangleSoupBytes===44780832&&indexedCandidateBytes===46186392,'documented exact-index buffer sizes changed; refresh the cost analysis');
assert(float32AttributeBytes===67171248&&triangleSoupBytes*3===float32AttributeBytes*2,'documented normalized attribute savings changed; refresh the cost analysis');
const bloom=R.PlantGenerator.create(R.PlantGenome.create('cherry',0,{fruiting:1}),{season:'spring',age:.83,detail:'high'});
assert(bloom.stats.flowers>0,'flowering plant fixture did not produce flowers');
assert(!bloom.root.children.some(mesh=>mesh.name==='flowers'),'flowers still create a separate draw-call mesh');
assert(bloom.stats.drawCalls===2&&bloom.root.children.length===2,'flower and foliage material batches did not merge');
assert(bloom.stats.triangles===bloom.root.children.reduce((sum,mesh)=>sum+mesh.geometry.attributes.position.count/3,0),'merged flower geometry changed triangle accounting');
const bloomHash=crypto.createHash('sha256');
for(const mesh of bloom.root.children){bloomHash.update(mesh.name);for(const name of ['position','color','normal']){const array=mesh.geometry.attributes[name].array;bloomHash.update(Buffer.from(array.buffer,array.byteOffset,array.byteLength));}}
assert(bloomHash.digest('hex')==='c1f6a2d7a7848bc2bc7dfe3a8390fbf7c656a71e9a5d72c748b1e55aa3a9af33','precomputed blossom/cluster transforms changed exact packed geometry');
bloom.dispose();
const OriginalColor=THREE.Color;let fruitColorAllocations=0;
THREE.Color=class extends OriginalColor{constructor(...args){super(...args);fruitColorAllocations++;}};
const fruiting=R.PlantGenerator.create(R.PlantGenome.create('elder',0,{fruiting:1}),{season:'summer',age:.83,detail:'high'});
THREE.Color=OriginalColor;
assert(fruiting.stats.fruits>0,'fruit color allocation fixture emitted no fruit');
assert(fruitColorAllocations===2,'fruit tips allocated colors instead of using the species palette ('+fruitColorAllocations+' total)');
fruiting.dispose();
R.PlantGenerator._setTimberTubeObserver(null);
console.log('PASS',checked+' species/season batches; 20 species × 3 LOD × 4 seasons; precomputed 5-sided bark profile, shared architecture, exact timber positions, and refcounted disposal');
